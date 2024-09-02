#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>

#include <stdio.h>

int main(int argc, char *argv[]) {
    SDLTest_CommonState *state;
    int i;
    SDL_bool stdin_to_stdout = SDL_FALSE;
    SDL_bool stdin_to_stderr = SDL_FALSE;
    int exit_code = 0;

    state = SDLTest_CommonCreateState(argv, 0);

    for (i = 1; i < argc;) {
        int consumed = SDLTest_CommonArg(state, i);
        if (SDL_strcmp(argv[i], "--stdin-to-stdout") == 0) {
            stdin_to_stdout = SDL_TRUE;
            consumed = 1;
        } else if (SDL_strcmp(argv[i], "--stdin-to-stderr") == 0) {
            stdin_to_stderr = SDL_TRUE;
            consumed = 1;
        } else if (SDL_strcmp(argv[i], "--stdout") == 0) {
            if (i + 1 < argc) {
                fprintf(stdout, "%s", argv[i + 1]);
                consumed = 2;
            }
        } else if (SDL_strcmp(argv[i], "--stderr") == 0) {
            if (i + 1 < argc) {
                fprintf(stderr, "%s", argv[i + 1]);
                consumed = 2;
            }
        } else if (SDL_strcmp(argv[i], "--exit-code") == 0) {
            if (i + 1 < argc) {
                char *endptr = NULL;
                exit_code = SDL_strtol(argv[i + 1], &endptr, 0);
                if (endptr && *endptr == '\0') {
                    consumed = 2;
                }
            }
        }
        if (consumed <= 0) {
            const char *args[] = { "[--stdin-to-stdout]", "[--stdout TEXT]", "[--stdin-to-stderr]", "[--stderr TEXT]", "[--exit-code EXIT_CODE]" };
            SDLTest_CommonLogUsage(state, argv[0], args);
            return 1;
        }
        i += consumed;
    }

    if (stdin_to_stdout || stdin_to_stderr) {

        for (;;) {
            int c;
            c = fgetc(stdin);
            if (c == EOF) {
                break;
            }
            if (stdin_to_stdout) {
                fputc(c, stdout);
                fflush(stdout);
            }
            if (stdin_to_stderr) {
                fputc(c, stderr);
            }
        }
    }

    SDLTest_CommonDestroyState(state);

    return exit_code;
}
