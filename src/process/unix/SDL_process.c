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
} SDL_Process;

char **dupstrlist(const char * const * list)
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

void freestrlist(char **list)
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
    SDL_Process *process = SDL_malloc(sizeof(SDL_Process));

    if (!process) {
        return NULL;
    }

    process->flags = flags;

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

    if ((flags & SDL_PROCESS_STDERR) && (pipe(process->stderr_pipe) < 0)) {
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
            close(process->stderr_pipe[READ_END]);
            dup2(process->stderr_pipe[WRITE_END], STDERR_FILENO);
        }

        char **mutable_args = dupstrlist(args);
        char **mutable_env = env ? dupstrlist(env) : NULL;

        // We are in a new process; don't bother freeing because either exec*() or exit() will succeed

        if (!mutable_args || (env && !mutable_env)) {
            fprintf(stderr, "Could not clone args/env: %s\n", SDL_GetError());
            exit(1);
        }

        if (env) {
            execve(args[0], mutable_args, mutable_env);
        } else {
            execv(args[0], mutable_args);
        }

        // If this is reached, execv() failed

        if (flags & SDL_PROCESS_ERRORS_TO_STDERR) {
            fprintf(stderr, "Could not execv/execve(): %s\n", strerror(errno));
        }

        exit(1);
    } else {
        if (flags & SDL_PROCESS_STDIN) {
            close(process->stdin_pipe[READ_END]);
        }

        if (flags & SDL_PROCESS_STDOUT) {
            close(process->stdout_pipe[WRITE_END]);
        }

        if (flags & SDL_PROCESS_STDERR) {
            close(process->stderr_pipe[WRITE_END]);
        }

        return process;
    }
}

int SDL_WriteProcess(SDL_Process *process, const void *buffer, int size)
{
    if (!process) {
        SDL_SetError("Attempt to call SDL_WriteProcess() with NULL process");
        return -1;
    }

    if (!(process->flags & SDL_PROCESS_STDIN)) {
        SDL_SetError("Cannot read from process created without SDL_PROCESS_STDIN");
        return -1;
    }

    int ret = write(process->stdin_pipe[WRITE_END], buffer, size);

    if (ret < 0) {
        SDL_SetError("Could not write(): %s", strerror(errno));
    }

    return ret;
}

int SDL_ReadProcess(SDL_Process *process, void *buffer, int size)
{
    if (!process) {
        SDL_SetError("Attempt to call SDL_ReadProcess() with NULL process");
        return -1;
    }

    if (!(process->flags & SDL_PROCESS_STDOUT)) {
        SDL_SetError("Cannot read from process created without SDL_PROCESS_STDOUT");
        return -1;
    }

    int ret = read(process->stdout_pipe[READ_END], buffer, size);

    if (ret < 0) {
        SDL_SetError("Could not read(): %s", strerror(errno));
    }

    return ret;
}

int SDL_ReadErrProcess(SDL_Process *process, void *buffer, int size)
{
    if (!process) {
        SDL_SetError("Attempt to call SDL_ReadErrProcess() with NULL process");
        return -1;
    }

    if (!(process->flags & SDL_PROCESS_STDERR)) {
        SDL_SetError("Cannot read from process created without SDL_PROCESS_STDERR");
        return -1;
    }

    int ret = read(process->stderr_pipe[READ_END], buffer, size);

    if (ret < 0) {
        SDL_SetError("Could not read(): %s", strerror(errno));
    }

    return ret;
}

SDL_bool SDL_KillProcess(SDL_Process *process, SDL_bool force)
{
    if (!process) {
        SDL_SetError("Attempt to call SDL_KillProcess() with NULL process");
        return -1;
    }

    int ret = kill(process->pid, force ? SIGKILL : SIGTERM);

    if (ret < 0) {
        SDL_SetError("Could not kill(): %s", strerror(errno));
    }

    return ret == 0;
}

/** @returns 1 if the process exited, 0 if not, -1 if an error occured. */
int SDL_WaitProcess(SDL_Process *process, SDL_bool block)
{
    if (!process) {
        SDL_SetError("Attempt to call SDL_WaitProcess() with NULL process");
        return -1;
    }

    int ret = waitpid(process->pid, NULL, block ? 0 : WNOHANG);

    if (ret < 0) {
        SDL_SetError("Could not waitpid(): %s", strerror(errno));
        return -1;
    }

    return ret != 0;
}

void SDL_DestroyProcess(SDL_Process *process)
{
    SDL_free(process);
}
