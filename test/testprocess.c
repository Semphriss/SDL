#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>

#ifdef SDL_PLATFORM_WINDOWS
#define EXE ".exe"
#else
#define EXE ""
#endif

/*
 * FIXME: Additional tests:
 * - arguments with spaces, '"', '. special chars
 * - stdin to stdout
 * - stdin to stderr
 * - read env, using env inherited from parent process
 * - read env, using env set by parent process
 * - exit codes
 * - kill process
 * - waiting twice on process
 */

typedef struct {
    const char *sdlsubprocess_path;
} TestProcessData;

static const char *options[] = { "/path/to/sdlsubprocess" EXE, NULL };

static int test_stdin_to_stdout(void *arg) {
    TestProcessData *data = (TestProcessData *)arg;
    const char *process_args[] = {
        data->sdlsubprocess_path,
        "--stdin-to-stdout",
        NULL,
    };
    const char *const *process_env = NULL;
    SDL_Process *process = NULL;
    SDL_IOStream *process_stdin = NULL;
    SDL_IOStream *process_stdout = NULL;
    const char *text_in = "Tests whether we can write to stdin and read from stdout\r\n{'succes': true, 'message': 'Success!'}\r\nYippie ka yee\r\nEOF";
    size_t amount_written;
    size_t amount_to_write;
    char buffer[128];
    size_t total_read;
    int wait_result;
    int exit_code;

    process = SDL_CreateProcess(process_args, process_env, SDL_PROCESS_STDIN | SDL_PROCESS_STDOUT);
    SDLTest_AssertCheck(process != NULL, "SDL_CreateProcess should not return NUL");
    if (!process) {
        goto failed;
    }

    process_stdin = (SDL_IOStream *)SDL_GetPointerProperty(SDL_GetProcessProperties(process), SDL_PROP_PROCESS_STDIN_STREAM, NULL);
    SDLTest_AssertCheck(process_stdin != NULL, "SDL_GetPointerProperty(SDL_PROP_PROCESS_STDIN_STREAM) returns a valid IO stream");
    process_stdout = (SDL_IOStream *)SDL_GetPointerProperty(SDL_GetProcessProperties(process), SDL_PROP_PROCESS_STDOUT_STREAM, NULL);
    SDLTest_AssertCheck(process_stdout != NULL, "SDL_GetPointerProperty(SDL_PROP_PROCESS_STDOUT_STREAM) returns a valid IO stream");
    if (!process_stdin || !process_stdout) {
        goto failed;
    }
    SDLTest_AssertPass("About to write to process");
    amount_to_write = SDL_strlen(text_in);
    amount_written = SDL_WriteIO(process_stdin, text_in, amount_to_write);
    SDLTest_AssertCheck(amount_written == amount_to_write, "SDL_WriteIO(subprocess.stdin) wrote %" SDL_PRIu64 " bytes, expected %" SDL_PRIu64, (Uint64)amount_written, (Uint64)amount_to_write);
    if (amount_to_write != amount_written) {
        goto failed;
    }

    total_read = 0;
    for (;;) {
        size_t amount_read;
        if (total_read >= sizeof(buffer) - 1) {
            SDLTest_AssertCheck(0, "Buffer is too small for input data.");
            goto failed;
        }

        SDLTest_AssertPass("About to read from process");
        amount_read = SDL_ReadIO(process_stdout, buffer + total_read, sizeof(buffer) - total_read - 1);
        total_read += amount_read;
        buffer[total_read] = '\0';
        if (total_read >= sizeof(buffer) - 1 || SDL_strstr(buffer, "EOF")) {
            break;
        }
        SDL_CPUPauseInstruction();
    }
    buffer[sizeof(buffer) - 1] = '\0';
    SDLTest_Log("Text read from subprocess: %s", buffer);
    SDLTest_AssertCheck(SDL_strcmp(buffer, text_in) == 0, "Subprocess stdout should match text written to stdin");

    SDLTest_AssertPass("About to close stdin");
    /* Closing stdin of `subprocessstdin --stdin-to-stdout` should close the process */
    SDL_CloseIO(process_stdin);

    SDLTest_AssertPass("About to wait on process");
    exit_code = 0xdeadbeef;
    wait_result = SDL_WaitProcess(process, SDL_TRUE, &exit_code);
    SDLTest_AssertCheck(wait_result == 1, "Process should have closed when closing stdin");
    SDLTest_AssertCheck(exit_code == 0, "Exit code should be 0, is %d", exit_code);
    if (wait_result != 1) {
        SDL_bool killed;
        SDL_Log("About to kill process");
        killed = SDL_KillProcess(process, SDL_TRUE);
        SDLTest_AssertCheck(killed, "SDL_KillProcess succeeded");
    }
    SDLTest_AssertPass("About to destroy process");
    SDL_DestroyProcess(process);
    return TEST_COMPLETED;
failed:

    SDL_DestroyProcess(process);
    return TEST_ABORTED;
}

int main(int argc, char *argv[])
{
    int i;
    SDLTest_CommonState *state;
    TestProcessData data = { 0 };

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_TEST, SDL_LOG_PRIORITY_INFO);

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (!consumed) {
            if (!data.sdlsubprocess_path) {
                data.sdlsubprocess_path = argv[i];
                consumed = 1;
            }
        }
        if (consumed <= 0) {
            SDL_Log("A");
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (!data.sdlsubprocess_path) {
        SDL_Log("B");
        SDLTest_CommonLogUsage(state, argv[0], options);
        return 1;
    }

    test_stdin_to_stdout(&data);

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
