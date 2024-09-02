#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    const char *args[] = {
        "/usr/bin/cat",
        NULL,
    };
    SDL_Process *process;
    SDL_IOStream *process_stdin;
    SDL_IOStream *process_stdout;
    char buffer[128];
    const char text_in[] = "Yippie ka yee\n";
    size_t read;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    process = SDL_CreateProcess(args, NULL, SDL_PROCESS_STDIN | SDL_PROCESS_STDOUT);
    if (!process) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateProcess failed (%s)", SDL_GetError());
        return 1;
    }
    SDL_Log("SDL_CreateProcess -> %process (%s)", process, SDL_GetError());

    process_stdin = SDL_GetPointerProperty(SDL_GetProcessProperties(process), SDL_PROP_PROCESS_STDIN_STREAM, NULL);
    if (!process_stdin) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not get stdin of process (%s)", SDL_GetError());
        return 1;
    }
    process_stdout = SDL_GetPointerProperty(SDL_GetProcessProperties(process), SDL_PROP_PROCESS_STDOUT_STREAM, NULL);
    if (!process_stdout) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not get stdout of process (%s)", SDL_GetError());
        return 1;
    }
    if (SDL_WriteIO(process_stdin, text_in, SDL_strlen(text_in)) != SDL_strlen(text_in)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to write to stdin (%s)", SDL_GetError());
        return 1;
    }
    read = SDL_ReadIO(process_stdout, buffer, sizeof(buffer) - 1);
    if (read != SDL_strlen(text_in)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read expected amount of data from stdout (%s)", SDL_GetError());
    }
    buffer[sizeof(buffer)-1] = '\0';
    SDL_Log("stdout: %s", buffer);

    if (SDL_strcmp(buffer, text_in) == 0) {
        SDL_Log("Text matches!!!");
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Text written to stdin does not match read from stdout");
    }
    /* Closing stdin from /usr/bin/cat should close the process */
    SDL_CloseIO(process_stdin);

    if (SDL_WaitProcess(process, SDL_TRUE) != 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Process should have closed when closing stdin");
        SDL_KillProcess(process, SDL_TRUE);
    }
    SDL_DestroyProcess(process);

    SDLTest_CommonDestroyState(state);
    return 0;
}
