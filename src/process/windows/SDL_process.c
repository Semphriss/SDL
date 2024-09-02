/*
Simple DirectMedia Layer
Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"
#include "../../core/windows/SDL_windows.h"

#include <windows.h>

#define READ_END 0
#define WRITE_END 1

typedef struct SDL_Process {
    HANDLE stdin_pipe[2];
    HANDLE stdout_pipe[2];
    HANDLE stderr_pipe[2];
    PROCESS_INFORMATION process_information;
    SDL_ProcessFlags flags;
    SDL_PropertiesID props;
} SDL_Process;

static Sint64 SDLCALL process_size_unsupported(void *userdata)
{
    SDL_SetError("Underlying stream has no pre-determined size");
    return -1;
}

static Sint64 SDLCALL process_seek_unsupported(void *userdata, Sint64 offset, SDL_IOWhence whence)
{
    SDL_SetError("Underlying stream is not seekable");
    return -1;
}

static size_t SDLCALL process_read_unsupported(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    if (status) {
        *status = SDL_IO_STATUS_ERROR;
    }
    SDL_SetError("Underlying stream is not readable");
    return 0;
}

static size_t SDLCALL process_write_unsupported(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    if (status) {
        *status = SDL_IO_STATUS_ERROR;
    }
    SDL_SetError("Underlying stream is not writable");
    return 0;
}

static size_t SDLCALL process_read_from_stdout(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    SDL_Process *process = (SDL_Process *) userdata;

    if (!(process->flags & SDL_PROCESS_STDOUT)) {
        if (status) {
            *status = SDL_IO_STATUS_ERROR;
        }
        SDL_SetError("Cannot read from process created without SDL_PROCESS_STDOUT");
        return 0;
    }

    DWORD actual;
    if (!ReadFile(process->stdout_pipe[READ_END], ptr, (DWORD)SDL_min(0xffffffffu, size), &actual, NULL)) {
        WIN_SetError("ReadFile");
        *status = SDL_IO_STATUS_ERROR;
        return actual;
    }

    return actual;
}

static size_t SDLCALL process_read_from_stderr(void *userdata, void *ptr, size_t size, SDL_IOStatus *status)
{
    SDL_Process *process = (SDL_Process *) userdata;

    if (!(process->flags & SDL_PROCESS_STDERR)) {
        if (status) {
            *status = SDL_IO_STATUS_ERROR;
        }
        SDL_SetError("Cannot read from process created without SDL_PROCESS_STDERR");
        return 0;
    }

    DWORD actual;
    if (!ReadFile(process->stderr_pipe[READ_END], ptr, (DWORD)SDL_min(0xffffffffu, size), &actual, NULL)) {
        WIN_SetError("ReadFile");
        *status = SDL_IO_STATUS_ERROR;
        return actual;
    }

    return actual;
}

static size_t SDLCALL process_write_to_stdin(void *userdata, const void *ptr, size_t size, SDL_IOStatus *status)
{
    SDL_Process *process = (SDL_Process *) userdata;

    if (!(process->flags & SDL_PROCESS_STDIN)) {
        SDL_SetError("Cannot read from process created without SDL_PROCESS_STDIN");
        if (status) {
            *status = SDL_IO_STATUS_ERROR;
        }
        return 0;
    }

    DWORD actual;
    if (!WriteFile(process->stdin_pipe[WRITE_END], ptr, (DWORD)SDL_min(0xffffffffu, size), &actual, NULL)) {
        WIN_SetError("WriteFile");
        *status = SDL_IO_STATUS_ERROR;
        return actual;
    }

    return actual;
}

static SDL_bool SDLCALL process_stdin_close(void *userdata)
{
    SDL_Process *process = (SDL_Process *) userdata;

    if (!(process->flags & SDL_PROCESS_STDIN)) {
        SDL_SetError("Cannot close stdin from process created without SDL_PROCESS_STDIN");
        return SDL_FALSE;
    }
    if (process->stdin_pipe[WRITE_END] < 0) {
        SDL_SetError("stdin already closed");
        return SDL_FALSE;
    }
    CloseHandle(process->stdin_pipe[WRITE_END]);
    process->stdin_pipe[WRITE_END] = NULL;
    SDL_ClearProperty(process->props, SDL_PROP_PROCESS_STDIN_STREAM);
    return SDL_TRUE;
}

static SDL_bool SDLCALL process_stdout_close(void *userdata)
{
    SDL_Process *process = (SDL_Process *) userdata;

    if (!(process->flags & SDL_PROCESS_STDOUT)) {
        SDL_SetError("Cannot close stdout from process created without SDL_PROCESS_STDOUT");
        return SDL_FALSE;
    }
    if (process->stdout_pipe[READ_END] < 0) {
        SDL_SetError("stdout already closed");
        return SDL_FALSE;
    }
    CloseHandle(process->stdout_pipe[READ_END]);
    process->stdout_pipe[READ_END] = NULL;
    SDL_ClearProperty(process->props, SDL_PROP_PROCESS_STDOUT_STREAM);
    return SDL_TRUE;
}

static SDL_bool SDLCALL process_stderr_close(void *userdata)
{
    SDL_Process *process = (SDL_Process *) userdata;

    if (!(process->flags & SDL_PROCESS_STDERR)) {
        SDL_SetError("Cannot close stderr from process created without SDL_PROC ESS_STDOUT");
        return SDL_FALSE;
    }
    if (process->stderr_pipe[READ_END] < 0) {
        SDL_SetError("stderr already closed");
        return SDL_FALSE;
    }
    CloseHandle(process->stderr_pipe[READ_END]);
    process->stderr_pipe[READ_END] = NULL;
    SDL_ClearProperty(process->props, SDL_PROP_PROCESS_STDERR_STREAM);
    return SDL_TRUE;
}

SDL_bool join_arguments(const char * const *args, char **args_out) {
    size_t len;
    int i;
    int i_out;
    char *result;

    len = 0;
    for (i = 0; args[i]; i++) {
        const char *a = args[i];

        for (; *a; a++) {
            switch (*a) {
            case '"':
            case '\\':
            case ' ':
            case '\t':
                len += 2;
                break;
            default:
                len += 1;
                break;
            }
        }
        /* space separator */
        len += 1;
    }

    result = SDL_malloc(len);
    if (!result) {
        *args_out = NULL;
        return SDL_FALSE;
    }

    i_out = 0;
    for (i = 0; args[i]; i++) {
        const char *a = args[i];

        for (; *a; a++) {
            switch (*a) {
            case '"':
            case '\\':
            case ' ':
            case '\t':
                result[i_out++] = '\\';
                result[i_out++] = *a;
                break;
            default:
                result[i_out++] = *a;
                break;
            }
        }
        result[i_out++] = ' ';
    }
    SDL_assert(i_out == len);
    result[len - 1] = '\0';
    *args_out = result;
    return SDL_TRUE;
}

SDL_bool join_env(const char * const *env, char **environment_out) {
    size_t len;
    const char * const *var;
    char *result;

    if (!env) {
        *environment_out = NULL;
        return SDL_TRUE;
    }

    len = 0;
    for (var = env; *var; var++) {
        len += SDL_strlen(*var) + 1;
    }
    result = SDL_malloc(len + 1);
    if (!result) {
        return SDL_FALSE;
    }

    len = 0;
    for (var = env; *var; var++) {
        size_t l = SDL_strlen(*var);
        SDL_memcpy(result + len, *var, l);
        result[len + l] = '\0';
        len += l + 1;
    }
    result[len] = '\0';

    *environment_out = result;
    return SDL_TRUE;
}

SDL_Process *SDL_CreateProcess(const char * const *args, const char * const *env, SDL_ProcessFlags flags)
{
    // Keep the malloc() before exec() so that an OOM won't run a process at all
    SDL_Process *process;
    char *createprocess_cmdline = NULL;
    char *createprocess_env = NULL;
    STARTUPINFOA startup_info;
    DWORD creation_flags;
    char *create_process_cwd;
    SECURITY_ATTRIBUTES security_attributes;

    if (!args) {
        SDL_SetError("args");
        return NULL;
    }

    process = SDL_calloc(1, sizeof(SDL_Process));
    if (!process) {
        return NULL;
    }
    process->flags = flags;

    process->props = SDL_CreateProperties();
    if (!process->props) {
        goto failed;
    }

    if (!join_arguments(args, &createprocess_cmdline)) {
        goto failed;
    }

    if (!join_env(env, &createprocess_env)) {
        goto failed;
    }

    SDL_zero(startup_info);
    startup_info.cb = sizeof(startup_info);
    creation_flags = 0;
    if (flags & (SDL_PROCESS_STDIN | SDL_PROCESS_STDOUT | SDL_PROCESS_STDERR)) {
        startup_info.dwFlags |= STARTF_USESTDHANDLES;

        SDL_zero(security_attributes);
        security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        security_attributes.bInheritHandle = TRUE;
        security_attributes.lpSecurityDescriptor = NULL;

        if (flags & SDL_PROCESS_STDIN) {
            if (!CreatePipe(&process->stdin_pipe[READ_END], &process->stdin_pipe[WRITE_END], &security_attributes, 0)) {
                goto failed;
            }
            if (!SetHandleInformation(process->stdin_pipe[WRITE_END], HANDLE_FLAG_INHERIT, 0) ) {
                WIN_SetError("SetHandleInformation(stdin_pipe[WRITE_END])");
                goto failed;
            }
            startup_info.hStdInput = process->stdin_pipe[READ_END];
        } else {
            startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        }

        if (flags & SDL_PROCESS_STDOUT) {
            if (!CreatePipe(&process->stdout_pipe[READ_END], &process->stdout_pipe[WRITE_END], &security_attributes, 0)) {
                goto failed;
            }
            if (!SetHandleInformation(process->stdout_pipe[READ_END], HANDLE_FLAG_INHERIT, 0) ) {
                WIN_SetError("SetHandleInformation(stdout_pipe[READ_END])");
                goto failed;
            }
            startup_info.hStdOutput = process->stdout_pipe[WRITE_END];
        } else {
            startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        }

        if (flags & SDL_PROCESS_STDERR) {
            if ((flags & SDL_PROCESS_STDERR_TO_STDOUT) == SDL_PROCESS_STDERR_TO_STDOUT) {
                startup_info.hStdError = process->stdout_pipe[WRITE_END];
            } else {
                if (!CreatePipe(&process->stderr_pipe[READ_END], &process->stderr_pipe[WRITE_END], &security_attributes, 0)) {
                    goto failed;
                }
                if (!SetHandleInformation(process->stderr_pipe[READ_END], HANDLE_FLAG_INHERIT, 0)) {
                    WIN_SetError("SetHandleInformation(stderr_pipe[READ_END])");
                    goto failed;
                }
                startup_info.hStdError = process->stderr_pipe[WRITE_END];
            }
        } else {
            startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        }
    }
    /* FIXME: current directory as extended option? SDL_CreatProcessFromProperties */
    create_process_cwd = NULL;
    if (!CreateProcessA(args[0], createprocess_cmdline, NULL, NULL, TRUE, creation_flags, createprocess_env, create_process_cwd, &startup_info, &process->process_information)) {
        WIN_SetError("CreateProcessA");
        goto failed;
    }
    SDL_free(createprocess_cmdline);
    SDL_free(createprocess_env);
    if ((flags & SDL_PROCESS_STDIN)) {
        SDL_IOStreamInterface iface;
        iface.size = process_size_unsupported;
        iface.seek = process_seek_unsupported;
        iface.read = process_read_unsupported;
        iface.write = process_write_to_stdin;
        iface.close = process_stdin_close;
        SDL_IOStream *io = SDL_OpenIO(&iface, process);
        SDL_SetPointerProperty(process->props, SDL_PROP_PROCESS_STDIN_STREAM, io);
        CloseHandle(process->stdin_pipe[READ_END]);
        process->stdin_pipe[READ_END] = NULL;
    }
    if ((flags & SDL_PROCESS_STDOUT)) {
        SDL_IOStreamInterface iface;
        iface.size = process_size_unsupported;
        iface.seek = process_seek_unsupported;
        iface.read = process_read_from_stdout;
        iface.write = process_write_unsupported;
        iface.close = process_stdout_close;
        SDL_IOStream *io = SDL_OpenIO(&iface, process);
        SDL_SetPointerProperty(process->props, SDL_PROP_PROCESS_STDOUT_STREAM, io);
        CloseHandle(process->stdout_pipe[WRITE_END]);
        process->stdout_pipe[WRITE_END] = NULL;
    }
    if ((flags & SDL_PROCESS_STDERR)) {
        if ((flags & SDL_PROCESS_STDERR_TO_STDOUT) != SDL_PROCESS_STDERR_TO_STDOUT) {
            SDL_IOStreamInterface iface;
            iface.size = process_size_unsupported;
            iface.seek = process_seek_unsupported;
            iface.read = process_read_from_stderr;
            iface.write = process_write_unsupported;
            iface.close = process_stderr_close;
            SDL_IOStream *io = SDL_OpenIO(&iface, process);
            SDL_SetPointerProperty(process->props, SDL_PROP_PROCESS_STDERR_STREAM, io);
            CloseHandle(process->stderr_pipe[WRITE_END]);
            process->stderr_pipe[WRITE_END] = NULL;
        }
    }
    return process;
failed:
    if (flags & SDL_PROCESS_STDIN) {
        if (process->stdin_pipe[READ_END]) {
            CloseHandle(process->stdin_pipe[READ_END]);
        }
        if (process->stdin_pipe[READ_END]) {
            CloseHandle(process->stdin_pipe[READ_END]);
        }
    }
    if (flags & SDL_PROCESS_STDOUT) {
        if (process->stdout_pipe[READ_END]) {
            CloseHandle(process->stdout_pipe[READ_END]);
        }
        if (process->stdout_pipe[READ_END]) {
            CloseHandle(process->stdout_pipe[READ_END]);
        }
    }
    if (flags & SDL_PROCESS_STDERR) {
        if (process->stderr_pipe[READ_END]) {
            CloseHandle(process->stderr_pipe[READ_END]);
        }
        if (process->stderr_pipe[READ_END]) {
            CloseHandle(process->stderr_pipe[READ_END]);
        }
    }
    SDL_DestroyProperties(process->props);
    SDL_free(createprocess_cmdline);
    SDL_free(createprocess_env);
    SDL_free(process);
    return NULL;
}

SDL_PropertiesID SDL_GetProcessProperties(SDL_Process *process)
{
    if (!process) {
        SDL_SetError("Attempt to call SDL_GetProcessProperties() with NULL process");
        return -1;
    }
    return process->props;
}

SDL_bool SDL_KillProcess(SDL_Process *process, SDL_bool force)
{
    if (!process) {
        SDL_SetError("Attempt to call SDL_KillProcess() with NULL process");
        return -1;
    }

    if (!TerminateProcess(process->process_information.hProcess, 1)) {
        WIN_SetError("TerminateProcess failed");
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

/** @returns 1 if the process exited, 0 if not, -1 if an error occured. */
int SDL_WaitProcess(SDL_Process *process, SDL_bool block, int *returncode)
{
    DWORD result;

    if (!process) {
        SDL_SetError("Attempt to call SDL_WaitProcess() with NULL process");
        return -1;
    }

    result = WaitForSingleObject(process->process_information.hProcess, block ? INFINITE : 0);

    if (result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT) {
        DWORD rc;
        if (!GetExitCodeProcess(process->process_information.hProcess, &rc)) {
            WIN_SetError("GetExitCodeProcess");
            return -1;
        }
        if (returncode) {
            *returncode = (int)rc;
        }
        return 1;
    } else if (result == WAIT_FAILED) {
        WIN_SetError("WaitForSingleObject(hProcess) returned WAIT_FAILED");
        return -1;
    } else {
        return 0;
    }
}

void SDL_DestroyProcess(SDL_Process *process)
{
    if (process->flags & SDL_PROCESS_STDIN) {
        SDL_IOStream *io = (SDL_IOStream *) SDL_GetPointerProperty(process->props, SDL_PROP_PROCESS_STDIN_STREAM, NULL);
        if (io) {
            SDL_CloseIO(io);
        }
    }
    if (process->flags & SDL_PROCESS_STDERR) {
        SDL_IOStream *io = (SDL_IOStream *) SDL_GetPointerProperty(process->props, SDL_PROP_PROCESS_STDERR_STREAM, NULL);
        if (io) {
            SDL_CloseIO(io);
        }
    }
    if (process->flags & SDL_PROCESS_STDOUT) {
        SDL_IOStream *io = (SDL_IOStream *) SDL_GetPointerProperty(process->props, SDL_PROP_PROCESS_STDOUT_STREAM, NULL);
        if (io) {
            SDL_CloseIO(io);
        }
    }
    CloseHandle(process->process_information.hThread);
    CloseHandle(process->process_information.hProcess);
    SDL_DestroyProperties(process->props);
    SDL_free(process);
}
