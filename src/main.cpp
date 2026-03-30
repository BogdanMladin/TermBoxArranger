
// clang-format off
#include <windows.h>
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
#include <winuser.h>
#include <stdio.h>
#include <stdint.h>
// clang-format on

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#define BUFFER_SIZE_BYTES 1024

internal int StrLen(const char *s)
{
    int result = 0;
    while (*s)
    {
        result++;
        s++;
    }
    return result;
}
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


internal void FillBuffer(char *buffer,
                         int bufferSize,
                         int &bytesWritten,
                         char *inputBuffer,
                         int numberOfBytesRead,
                         int &exit)
{
    bytesWritten = 0;
    int writeIndex = 0;

    if (inputBuffer[0] == 'q')
    {
        exit = 0;
    }
    if (inputBuffer[0] == '\x1b')
        inputBuffer[0] = 0;

    for (int i = 0; i < numberOfBytesRead; i++)
    {
        buffer[bytesWritten++] = inputBuffer[i];
    }
}

int main()
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
    int bufferSize = BUFFER_SIZE_BYTES;

    int exit = 1;
    HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD bytesWritten;
    DWORD numberOfBytesRead;
    char inputBuffer[10];
    int fillBytesWritten = 0;
    printToStdHandle("\x1b[?1000h");
    printToStdHandle("\x1b[?1006h");

    printToStdHandle("\x1b[?1049h");
    while (exit)
    {

        ReadFile(hIn, inputBuffer, 10, &numberOfBytesRead, NULL);

        FillBuffer(buffer, bufferSize, fillBytesWritten, inputBuffer, numberOfBytesRead, exit);

        WriteFile(hOut, buffer, fillBytesWritten, NULL, NULL);

        // WriteFile(stdHandle, buffer, fillBytesWritten, &bytesWritten, NULL);
    }
    printToStdHandle("\x1b[?1049l");

    printToStdHandle("\x1b[?1000l"); // disable
    printToStdHandle("\x1b[?1006l"); // disable SGR modebytesWritten

    SetConsoleMode(hOut, initOutMode);
    SetConsoleMode(hIn, initInMode);

    // printToStdHandle("strlen result: %d|");
    // Try some Set Graphics Rendition (SGR) terminal escape sequences
    // clang-format off
  // WriteFile(hOut, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
  // printToStdHandle("\x1b[31mThis text has a red foreground using SGR.31.\r\n");
  // printToStdHandle("\x1b[1mThis text has a bright (bold) red foreground using SGR.1 to ");
  // printToStdHandle("affect the previous color setting.\r\n");
  // wprintf(L"\x1b[mThis text has returned to default colors using SGR.0 " L"implicitly.\r\n");
  // wprintf(L"\x1b[34;46mThis text shows the foreground and background change at " L"the same time.\r\n");
  // wprintf(L"\x1b[0mThis text has returned to default colors using SGR.0 " L"explicitly.\r\n");
  // wprintf( L"\x1b[31;32;33;34;35;36;101;102;103;104;105;106;107mThis text attempts " L"to apply many colors in the same command. Note the colors are applied " L"from left to right so only the right-most option of foreground cyan " L"(SGR.36) and background bright white (SGR.107) is effective.\r\n");
  // wprintf(L"\x1b[39mThis text has restored the foreground color only.\r\n");
  // wprintf(L"\x1b[49mThis text has restored the background color only.\r\n");
    // clang-format on

    return 0;
}
