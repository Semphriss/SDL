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

/**
 * # CategoryProcess
 *
 * Process control support.
 */

#ifndef SDL_process_h_
#define SDL_process_h_

#include <SDL3/SDL_error.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /** Create a pipe to the process' stdin. */
    SDL_PROCESS_STDIN = 1 << 0,
    /** Create a pipe from the process' stdout. Without this option, the process will output to the parent's stdout. */
    SDL_PROCESS_STDOUT = 1 << 1,
    /** Create a pipe from the process' stderr. Without this option, the process will output to the parent's stderr. */
    SDL_PROCESS_STDERR = 1 << 2,
    /** Allow SDL to report errors on the child process' stderr if launching the process failed after a fork(). */
    SDL_PROCESS_ERRORS_TO_STDERR = 1 << 3,
} SDL_ProcessFlags;

typedef struct SDL_Process SDL_Process;

/**
 * Create a new process.
 *
 * The path to the executable is supplied in args[0]. The path must be a full path.
 *
 * \param args The arguments to send the new executable.
 * \param env The environment to assign to the new executable. May be NULL, in which case the environment will be inherited from the parent process. If an environment is defined, all other entries are deleted; merging the parent's environment with some extra entries must be done beforehand.
 * \param flags The flags to control the process options.
 *
 * \returns The newly created, now running process.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_Process *SDLCALL SDL_CreateProcess(const char * const *args, const char * const *env, SDL_ProcessFlags flags);

#define SDL_PROP_PROCESS_STDIN_STREAM "SDL.process.stdin"
#define SDL_PROP_PROCESS_STDOUT_STREAM "SDL.process.stdout"
#define SDL_PROP_PROCESS_STDERR_STREAM "SDL.process.stderr"

extern SDL_DECLSPEC SDL_PropertiesID SDL_GetProcessProperties(SDL_Process *process);

/**
 * Stop a process.
 *
 * \param process The process to stop.
 * \param force Stop the process immediately, without giving it a chance to free its resources.
 *
 * \returns true on success, false on error; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_KillProcess(SDL_Process *process, SDL_bool force);

/**
 * Wait for a process to finish.
 *
 * This function must be called prior to destroying (or losing) any process.
 *
 * Process that died but aren't waited for remain zombie processes that take resources on the system.
 *
 * \param process The process to wait for.
 * \param block If true, block until the process finishes; otherwise, report on the process' status.
 *
 * \returns 1 if the process exited, 0 if not, -1 if an error occured. Call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL SDL_WaitProcess(SDL_Process *process, SDL_bool block);

/**
 * Destroy a previously created process
 *
 * SDL_ProcessWait() MUST have been called on the process before destroying it, including if it has been killed. Failing to do so will keep the process in a "zombie" state, which will consume resources until the dead process is waited for.
 *
 * \param process The process to destroy.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC void SDLCALL SDL_DestroyProcess(SDL_Process *process);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_process_h_ */
