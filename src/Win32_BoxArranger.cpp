
// clang-format off
#include <cassert>
#include <urlmon.h>
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

#include <math.h>
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

#define BUFFER_SIZE_BYTES 4048

internal int32 StrLen(const char *s)
{
    int32 result = 0;
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

struct output_buffer
{
    int32 windowWidth;
    int32 windowHeight;
    int32 bufferSize;
    int32 bytesWritten;
    char *buffer;
};

struct game_state
{
    int32 currentLine;
    int32 squareRadius;
    real32 rotationOffset;
};

struct point
{
    real32 x;
    real32 y;
};
struct line
{
    real32 m;
    real32 b;
    int32 isVertical;
};

struct square
{
    union {
        point points[4];
        struct
        {
            point ul;
            point ur;
            point ll;
            point lr;
        };
    };
};

inline internal int32 RoundReal32ToInt32(real32 Real32)
{
    // int32 result = (int32)lrintf(Real32); //this uses intrinsic
    int32 Result = 0;
    Result = (int32)(Real32 + 0.5f);
    return Result;
}

internal void BufWrite(output_buffer *outputBuffer, const char *s, int32 bytesToWrite)
{
    if (outputBuffer->bytesWritten + bytesToWrite <= outputBuffer->bufferSize)
    {
        for (int32 i = 0; i < bytesToWrite; i++)
        {
            outputBuffer->buffer[outputBuffer->bytesWritten] = s[i];
            outputBuffer->bytesWritten++;
        }
    }
    else
    {
        assert(false);
    }
}
internal inline int32 CharLenPozInt32(int32 integer)
{
    if (integer == 0)
        return 1;

    int32 result = 0;

    while (integer > 0)
    {
        integer /= 10;
        result++;
    }
    return result;
}

internal void BufDrawPoint(output_buffer *OB, real32 x, real32 y)
{
    char s[20] = {'\x1b', '['};
    int32 sBytes = 2;
    int32 roundX = RoundReal32ToInt32(x);
    int32 roundY = RoundReal32ToInt32(y);

    if (roundY >= 0 && roundY <= OB->windowHeight)
    {
        int32 yLen = CharLenPozInt32(roundY);
        for (int i = 0; i < yLen; i++)
        {
            s[sBytes + yLen - i - 1] = roundY % 10 + '0';
            roundY /= 10;
        }
        sBytes += yLen;
    }

    s[sBytes] = ';';
    sBytes++;

    if (roundX >= 0 && roundX <= OB->windowWidth)
    {
        int32 xLen = CharLenPozInt32(roundX);
        for (int i = 0; i < xLen; i++)
        {
            s[sBytes + xLen - i - 1] = roundX % 10 + '0';
            roundX /= 10;
        }
        sBytes += xLen;
    }

    s[sBytes] = 'H';
    sBytes++;

    s[sBytes] = ' ';
    sBytes++;

    BufWrite(OB, s, sBytes);
}

internal line LineFromPoints(point pointA, point pointB)
{
    // TODO(bogdan): Address case where line is vertical (pointB.x = pointA.x)
    line result = {};
    if (pointB.x == pointA.x)
    {
        result.isVertical = 1;
        result.b = pointA.x;
        return result;
    }
    result.m = (pointB.y - pointA.y) / (pointB.x - pointA.x);

    result.b = pointA.y - (result.m * pointA.x);
    return result;
}
internal void BufDrawLine(output_buffer *OB, point pointA, point pointB)
{
    line line = LineFromPoints(pointA, pointB);
    if (line.isVertical)
    {
        int32 roundAY = RoundReal32ToInt32(pointA.y);
        int32 roundBY = RoundReal32ToInt32(pointB.y);
        int32 roundX = RoundReal32ToInt32(pointA.x);

        if (roundAY > roundBY)
        {
            int32 aux = roundAY;
            roundAY = roundBY;
            roundBY = aux;
        }

        for(int i = roundAY; i <= roundBY; i++){
            BufDrawPoint(OB , roundX, i);
        }
    }
    else{
        int32 roundAY = RoundReal32ToInt32(pointA.y);
        int32 roundBY = RoundReal32ToInt32(pointB.y);
        int32 roundAX = RoundReal32ToInt32(pointA.x);
        int32 roundBX = RoundReal32ToInt32(pointB.x);

        int32 granularity = abs(roundAX - roundBX) + abs(roundAY - roundBY);

        real32 distance = pointA.x - pointB.x;
        real32 step = distance /(real32)granularity;

        real32 currentX = pointA.x;
        for(int i = 0; i< granularity; i++){
            real32 currentY = line.m*currentX + line.b;
            BufDrawPoint(OB, currentX , currentY);
            currentX -= step;
            
        }
        
        
    }
}

internal void FillBuffer(output_buffer *outputBuffer,
                         char *inputBuffer,
                         int32 numberOfBytesRead,
                         game_state *gameState,
                         int32 &running)
{
    outputBuffer->bytesWritten = 0;
    int32 writeIndex = 0;
    BufWrite(outputBuffer,
             "\x1b[0m\x1b[3J\x1b[2J\x1b[H",
             15); // Clear screen, attributes, and move cursor to top left

    BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green

    if (inputBuffer[0] == 'q')
    {
        running = 0;
    }
    if (inputBuffer[0] == 'j')
    {
        gameState->rotationOffset += Pi32 / 38.0f;
    }
    if(inputBuffer[0] == 'k'){
        gameState->rotationOffset -= Pi32 / 38.0f;
    }
    if (inputBuffer[0] == 'l')
    {
        gameState->squareRadius++;
    }
    if (inputBuffer[0] == 'h')
    {
        gameState->squareRadius--;
    }

    // point p1;
    // p1.x = 2;
    // p1.y = 2;
    // point p2;
    // p2.x = 3;
    // p2.y = 5;
    // BufDrawLine(outputBuffer , p1 , p2);

    real32 midX = (real32)outputBuffer->windowWidth / 2.0f;
    real32 midY = (real32)outputBuffer->windowHeight / 2.0f;
    // BufDrawPoint(outputBuffer, midX, midY);

    square square;
    real32 pointOffset = gameState->rotationOffset;
    real32 squareRadius = gameState->squareRadius;
    for (int i = 0; i < 4; i++)
    {
        square.points[i].x = midX + (cosf(pointOffset) * squareRadius * 2.0f);
        square.points[i].y = midY + (sinf(pointOffset) * squareRadius);
        pointOffset += Pi32 / 2.0f;
    }

    for (int i = 0; i < 4; i++)
    {
        point pointA = square.points[i];
        point pointB = square.points[(i + 1) % 4];

        BufDrawLine(outputBuffer, pointA, pointB);
    }

    for (int i = 0; i < 4; i++)
    {
        BufDrawPoint(outputBuffer, square.points[i].x, square.points[i].y);
    }
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
