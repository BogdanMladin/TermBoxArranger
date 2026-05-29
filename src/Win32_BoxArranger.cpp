
// clang-format off
#include <windows.h>
#include <libloaderapi.h>
#include <string.h>
#include <handleapi.h>
#include <urlmon.h>
#include <consoleapi3.h>
#include <wincontypes.h>
#include <sysinfoapi.h>
#include <processenv.h>
#include <winbase.h>
#include <cstdio>
#include <cstddef>
#include <minwindef.h>
#include <fileapi.h>
#include <consoleapi2.h>
#include <consoleapi.h>
#include <winnt.h>
#include <winuser.h>
#include <stdio.h>
// clang-format on

#include "BoxArranger.cpp"

struct path_buffer
{
    char buffer[MAX_PATH];
    char *onePastLastSlash;
};

internal void printToStdHandle(const char *s)
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD bytesWritten;
    WriteFile(handle, s, strlen(s), &bytesWritten, NULL);
}

inline LARGE_INTEGER win32GetWallClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return (Result);
}

internal void GetExePath(path_buffer *dest)
{
    GetModuleFileName(0, dest->buffer, sizeof(dest->buffer));
    for (char *scan = dest->buffer; *scan; ++scan)
    {
        if (*scan == '\\')
        {
            dest->onePastLastSlash = scan + 1;
        }
    }
}

internal void CreateFullPath(path_buffer *basePath, char *appended)
{
    char *a = basePath->onePastLastSlash;
    for (char *scan = appended; *scan; scan++)
    {
        *a = *scan;
        a++;
    }

    *a = 0;
}

int32 main()
{

    // FreeConsole();
    // AllocConsole();

    // Set output mode to handle virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return GetLastError();
    }
    DWORD OutMode = 0;
    if (!GetConsoleMode(hOut, &OutMode))
    {
        return GetLastError();
    }
    DWORD initOutMode = OutMode;

    OutMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, OutMode))
    {
        return GetLastError();
    }

    // start input loop

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    DWORD inMode;
    GetConsoleMode(hIn, &inMode);
    DWORD initInMode = inMode;

    inMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    inMode |= ENABLE_WINDOW_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT; // optional (resize events)

    SetConsoleMode(hIn, inMode);

    char buffer[BUFFER_SIZE_BYTES] = {};
    int32 bufferSize = BUFFER_SIZE_BYTES;
    output_buffer outputBuffer = {};
    outputBuffer.buffer = buffer;
    outputBuffer.bufferSize = bufferSize;

    int32 running = 1;
    HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD bytesWritten;
    DWORD numberOfBytesRead;
    DWORD screenNumberOfBytesRead;
    char inputBuffer[20];
    char screenSizeBuffer[20];
    int32 fillBytesWritten = 0;

    game_state gameState = {};
    gameState.squareRadius = 8;
    gameState.selectedListLine = 0;

    gameState.boxes[0].length = 1;
    gameState.boxes[0].width = 2;
    gameState.boxes[0].height = 3;

    gameState.boxes[1].length = 4;
    gameState.boxes[1].width = 5;
    gameState.boxes[1].height = 6;

    gameState.boxes[2].length = 7;
    gameState.boxes[2].width = 8;
    gameState.boxes[2].height = 9;

    gameState.boxCount = 3;

    path_buffer basePath = {};
    GetExePath(&basePath);
    CreateFullPath(&basePath, "hello.txt");

    HANDLE fileHandle1 =
        CreateFile(basePath.buffer, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);

    DWORD nobw1;
    ReadFile(fileHandle1, &gameState, sizeof(game_state), &nobw1, 0);
    CloseHandle(fileHandle1);

    printToStdHandle("\x1b[?1000h"); // Get mouse input
    printToStdHandle("\x1b[?1006h"); // Get mouse input

    printToStdHandle("\x1b[?1049h"); // Switch to alternate buffer
    printToStdHandle("\x1b[?25l");   // Hide the cursor. ESC[?25h to unhide
    while (running)
    {
        ReadFile(hIn, inputBuffer, 20, &numberOfBytesRead, NULL);

        printToStdHandle("\x1b[18t");
        ReadFile(hIn, screenSizeBuffer, 20, &screenNumberOfBytesRead, NULL);
        if (screenSizeBuffer[0] == '\x1b' && screenSizeBuffer[1] == '[' &&
            screenSizeBuffer[2] == '8' && screenSizeBuffer[3] == ';')
        {
            int32 i = 4;
            int32 heightSize = 0;
            int32 widthSize = 0;
            int32 windowWidth = 0;
            int32 windowHeight = 0;
            while (screenSizeBuffer[i] != ';')
            {
                heightSize++;
                i++;
            }
            i++;
            while (screenSizeBuffer[i] != 't')
            {
                widthSize++;
                i++;
            }

            i = 4;

            while (screenSizeBuffer[i] != ';')
            {
                int32 pow = 1;
                for (int32 i = 0; i < heightSize - 1; i++)
                {
                    pow *= 10;
                }
                windowHeight += pow * (screenSizeBuffer[i] - '0');
                heightSize--;
                i++;
            }
            i++;
            while (screenSizeBuffer[i] != 't')
            {
                int32 pow = 1;
                for (int32 i = 0; i < widthSize - 1; i++)
                {
                    pow *= 10;
                }
                windowWidth += pow * (screenSizeBuffer[i] - '0');
                widthSize--;
                i++;
            }
            outputBuffer.windowWidth = windowWidth;
            outputBuffer.windowHeight = windowHeight;
        }

        FillBuffer(&outputBuffer, inputBuffer, numberOfBytesRead, &gameState, running);

        WriteFile(hOut, outputBuffer.buffer, outputBuffer.bytesWritten, NULL, NULL);
    }
    printToStdHandle("\x1b[?1049l"); // Switch back to main buffer

    printToStdHandle("\x1b[?1000l"); // disable
    printToStdHandle("\x1b[?1006l"); // disable SGR modebytesWritten

    SetConsoleMode(hOut, initOutMode);
    SetConsoleMode(hIn, initInMode);

    HANDLE fileHandle =
        CreateFile(basePath.buffer, GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);
    DWORD nobw;
    WriteFile(fileHandle1, &gameState, sizeof(game_state), &nobw1, 0);
    CloseHandle(fileHandle1);

    return 0;
}
