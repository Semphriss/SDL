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

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define READ_END 0
#define WRITE_END 1

typedef struct SDL_Process {
    int pid;
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];
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

    int ret = read(process->stdout_pipe[READ_END], ptr, size);

    if (ret < 0) {
        if (status) {
            *status = SDL_IO_STATUS_ERROR;
        }
        SDL_SetError("Could not read(): %s", strerror(errno));
        return 0;
    }

    return ret;
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

    int ret = read(process->stderr_pipe[READ_END], ptr, size);

    if (ret < 0) {
        if (status) {
            *status = SDL_IO_STATUS_ERROR;
        }
        SDL_SetError("Could not read(): %s", strerror(errno));
        return 0;
    }

    return ret;
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

    int ret = write(process->stdin_pipe[WRITE_END], ptr, size);

    if (ret < 0) {
        SDL_SetError("Could not write(): %s", strerror(errno));
        if (status) {
            *status = SDL_IO_STATUS_ERROR;
        }
        return 0;
    }

    return ret;
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
    close(process->stdin_pipe[WRITE_END]);
    process->stdin_pipe[WRITE_END] = -1;
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
    close(process->stdout_pipe[READ_END]);
    process->stdout_pipe[READ_END] = -1;
    SDL_ClearProperty(process->props, SDL_PROP_PROCESS_STDOUT_STREAM);
    return SDL_TRUE;
}

static SDL_bool SDLCALL process_stderr_close(void *userdata)
{
    SDL_Process *process = (SDL_Process *) userdata;

    if (!(process->flags & SDL_PROCESS_STDERR)) {
        SDL_SetError("Cannot close stderr from process created without SDL_PROCESS_STDOUT");
        return SDL_FALSE;
    }
    if (process->stderr_pipe[READ_END] < 0) {
        SDL_SetError("stderr already closed");
        return SDL_FALSE;
    }
    close(process->stderr_pipe[READ_END]);
    process->stderr_pipe[READ_END] = -1;
    SDL_ClearProperty(process->props, SDL_PROP_PROCESS_STDERR_STREAM);
    return SDL_TRUE;
}

static char **dupstrlist(const char * const * list)
{
    unsigned int n = 0;
    const char * const * ptr = list;

    while (*ptr++) {
        n++;
    }

    char **newlist = SDL_malloc(sizeof(char *) * (n + 1));

    if (!newlist) {
        return NULL;
    }

    ptr = list;
    char **newptr = newlist;
    while (*ptr) {
        *newptr = SDL_strdup(*ptr);

        if (!*newptr) {
            for (char **delptr = newlist; delptr != newptr; delptr++) {
                SDL_free(*delptr);
            }
            return NULL;
        }

        newptr++;
        ptr++;
    }

    *newptr = NULL;
    return newlist;
}

static void freestrlist(char **list)
{
  char **ptr = list;

  while (*ptr) {
      SDL_free(*ptr);
      ptr++;
  }

  SDL_free(list);
}

SDL_Process *SDL_CreateProcess(const char * const *args, const char * const *env, SDL_ProcessFlags flags)
{
    // Keep the malloc() before exec() so that an OOM won't run a process at all
    SDL_Process *process;

    if (!args) {
        SDL_SetError("args");
        return NULL;
    }

    process = SDL_malloc(sizeof(SDL_Process));

    if (!process) {
        return NULL;
    }

    process->flags = flags;

    process->props = SDL_CreateProperties();
    if (!process->props) {
        SDL_free(process);
        return NULL;
    }

    if ((flags & SDL_PROCESS_STDIN) && (pipe(process->stdin_pipe) < 0)) {
        SDL_SetError("Could not pipe(): %s", strerror(errno));
        SDL_free(process);
        return NULL;
    }

    if ((flags & SDL_PROCESS_STDOUT) && (pipe(process->stdout_pipe) < 0)) {
        SDL_SetError("Could not pipe(): %s", strerror(errno));

        if (flags & SDL_PROCESS_STDIN) {
            close(process->stdin_pipe[READ_END]);
            close(process->stdin_pipe[WRITE_END]);
        }

        SDL_free(process);
        return NULL;
    }

    if ((flags & SDL_PROCESS_STDERR) && (flags & SDL_PROCESS_STDERR_TO_STDOUT) != SDL_PROCESS_STDERR_TO_STDOUT && pipe(process->stderr_pipe) < 0) {
        SDL_SetError("Could not pipe(): %s", strerror(errno));

        if (flags & SDL_PROCESS_STDIN) {
            close(process->stdin_pipe[READ_END]);
            close(process->stdin_pipe[WRITE_END]);
        }

        if (flags & SDL_PROCESS_STDOUT) {
            close(process->stdout_pipe[READ_END]);
            close(process->stdout_pipe[WRITE_END]);
        }

        SDL_free(process);
        return NULL;
    }

    process->pid = fork();

    if (process->pid < 0) {
        SDL_SetError("Could not fork(): %s", strerror(errno));
        if (flags & SDL_PROCESS_STDIN) {
            close(process->stdin_pipe[READ_END]);
            close(process->stdin_pipe[WRITE_END]);
        }

        if (flags & SDL_PROCESS_STDOUT) {
            close(process->stdout_pipe[READ_END]);
            close(process->stdout_pipe[WRITE_END]);
        }

        if (flags & SDL_PROCESS_STDERR) {
            close(process->stderr_pipe[READ_END]);
            close(process->stderr_pipe[WRITE_END]);
        }

        SDL_free(process);

        return NULL;
    } else if (process->pid == 0) {
        if (flags & SDL_PROCESS_STDIN) {
            close(process->stdin_pipe[WRITE_END]);
            dup2(process->stdin_pipe[READ_END], STDIN_FILENO);
        }

        if (flags & SDL_PROCESS_STDOUT) {
            close(process->stdout_pipe[READ_END]);
            dup2(process->stdout_pipe[WRITE_END], STDOUT_FILENO);
        }

        if (flags & SDL_PROCESS_STDERR) {
            if ((flags & SDL_PROCESS_STDERR_TO_STDOUT) == SDL_PROCESS_STDERR_TO_STDOUT) {
                dup2(process->stdout_pipe[WRITE_END], STDERR_FILENO);
            } else {
                close(process->stderr_pipe[READ_END]);
                dup2(process->stderr_pipe[WRITE_END], STDERR_FILENO);
            }
        }

        char **mutable_args = dupstrlist(args);
        char **mutable_env = env ? dupstrlist(env) : NULL;

        // We are in a new process; don't bother freeing because either exec*() or exit() will succeed

        if (!mutable_args || (env && !mutable_env)) {
            SDL_LogError(SDL_LOG_CATEGORY_PROCESS, "Could not clone args/env: %s\n", SDL_GetError());
            exit(1);
        }

        if (env) {
            execve(args[0], mutable_args, mutable_env);
        } else {
            execv(args[0], mutable_args);
        }

        // If this is reached, execv() failed

        SDL_LogError(SDL_LOG_CATEGORY_PROCESS, "Could not execv/execve(): %s", strerror(errno));

        freestrlist(mutable_args);
        freestrlist(mutable_env);

        exit(1);
    } else {
        if (flags & SDL_PROCESS_STDIN) {
            SDL_IOStreamInterface iface;
            iface.size = process_size_unsupported;
            iface.seek = process_seek_unsupported;
            iface.read = process_read_unsupported;
            iface.write = process_write_to_stdin;
            iface.close = process_stdin_close;
            SDL_IOStream *io = SDL_OpenIO(&iface, process);
            SDL_SetPointerProperty(process->props, SDL_PROP_PROCESS_STDIN_STREAM, io);

            close(process->stdin_pipe[READ_END]);
        }

        if (flags & SDL_PROCESS_STDOUT) {
            SDL_IOStreamInterface iface;
            iface.size = process_size_unsupported;
            iface.seek = process_seek_unsupported;
            iface.read = process_read_from_stdout;
            iface.write = process_write_unsupported;
            iface.close = process_stdout_close;
            SDL_IOStream *io = SDL_OpenIO(&iface, process);
            SDL_SetPointerProperty(process->props, SDL_PROP_PROCESS_STDOUT_STREAM, io);

            close(process->stdout_pipe[WRITE_END]);
        }

        if ((flags & SDL_PROCESS_STDERR) && (flags & SDL_PROCESS_STDERR_TO_STDOUT) != SDL_PROCESS_STDERR_TO_STDOUT) {
            SDL_IOStreamInterface iface;
            iface.size = process_size_unsupported;
            iface.seek = process_seek_unsupported;
            iface.read = process_read_from_stderr;
            iface.write = process_write_unsupported;
            iface.close = process_stderr_close;
            SDL_IOStream *io = SDL_OpenIO(&iface, process);
            SDL_SetPointerProperty(process->props, SDL_PROP_PROCESS_STDERR_STREAM, io);

            close(process->stderr_pipe[WRITE_END]);
        }

        return process;
    }
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
        return SDL_FALSE;
    }

    int ret = kill(process->pid, force ? SIGKILL : SIGTERM);

    if (ret < 0) {
        SDL_SetError("Could not kill(): %s", strerror(errno));
    }

    /* FIXME: when ret != 0, SDL_SetError should be called */

    return ret == 0;
}

/** @returns 1 if the process exited, 0 if not, -1 if an error occured. */
int SDL_WaitProcess(SDL_Process *process, SDL_bool block, int *returncode)
{
    if (!process) {
        SDL_SetError("Attempt to call SDL_WaitProcess() with NULL process");
        return -1;
    }

    int wstatus = 0;
    int ret = waitpid(process->pid, &wstatus, block ? 0 : WNOHANG);

    if (ret < 0) {
        SDL_SetError("Could not waitpid(): %s", strerror(errno));
        return -1;
    }

    if (ret == 0) {
        return 0;
    }

    if (WIFEXITED(wstatus)) {
        if (returncode) {
            *returncode = WEXITSTATUS(wstatus);
        }
        return 1;
    }

    if (WIFSIGNALED(wstatus)) {
        if (returncode) {
            *returncode = WTERMSIG(wstatus);
        }
        return 1;
    }

    return 1;
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
    SDL_DestroyProperties(process->props);
    SDL_free(process);
}
