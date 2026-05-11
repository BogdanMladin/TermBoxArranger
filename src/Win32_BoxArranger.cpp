
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

struct box
{
    union {
        struct
        {
            real32 length;
            real32 width;
            real32 height;
        };
        real32 dimensions[3];
    };
};

struct game_state
{
    int32 selectedListLine;
    int32 selectedNewItemDimension;
    int32 selectedDimension;

    int32 squareRadius;
    real32 rotationOffset;

    box boxes[12];
    int32 boxCount;
    int32 maxBoxCount = 12;
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

internal void BufWriteHighlightGreen(output_buffer *outputBuffer, const char *s, int32 bytesToWrite)
{
    if (outputBuffer->bytesWritten + 5 <= outputBuffer->bufferSize)
        BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green
    else
        assert(false);
    outputBuffer->bytesWritten += 5;

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

// Calculates the number of characters needed to represent the pozitive integer, equivalent to
// calculating the number of digits
internal inline int32 CharLenUInt32(uint32 integer)
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

internal void BufWriteUInt32(output_buffer *OB, uint32 uint)
{
    int32 charLen = CharLenUInt32(uint);
    assert(OB->bytesWritten + charLen <= OB->bufferSize);
    for (int i = 0; i < charLen; i++)
    {
        OB->buffer[OB->bytesWritten + charLen - i - 1] = uint % 10 + '0';
        uint /= 10;
    }
    OB->bytesWritten += charLen;
}

internal void BufSetPos(output_buffer *OB, real32 x, real32 y)
{
    char s[20] = {'\x1b', '['};
    int32 sBytes = 2;
    int32 roundX = RoundReal32ToInt32(x);
    int32 roundY = RoundReal32ToInt32(y);

    if (roundY >= 0 && roundY <= OB->windowHeight)
    {
        int32 yLen = CharLenUInt32(roundY);
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
        int32 xLen = CharLenUInt32(roundX);
        for (int i = 0; i < xLen; i++)
        {
            s[sBytes + xLen - i - 1] = roundX % 10 + '0';
            roundX /= 10;
        }
        sBytes += xLen;
    }

    s[sBytes] = 'H';
    sBytes++;

    BufWrite(OB, s, sBytes);
}

internal void BufDrawPoint(output_buffer *OB, real32 x, real32 y)
{
    BufSetPos(OB, x, y);
    BufWrite(OB, " ", 1);
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

        for (int i = roundAY; i <= roundBY; i++)
        {
            BufDrawPoint(OB, roundX, i);
        }
    }
    else
    {
        int32 roundAY = RoundReal32ToInt32(pointA.y);
        int32 roundBY = RoundReal32ToInt32(pointB.y);
        int32 roundAX = RoundReal32ToInt32(pointA.x);
        int32 roundBX = RoundReal32ToInt32(pointB.x);

        int32 granularity = abs(roundAX - roundBX) + abs(roundAY - roundBY);

        real32 distance = pointA.x - pointB.x;
        real32 step = distance / (real32)granularity;

        real32 currentX = pointA.x;
        for (int i = 0; i < granularity; i++)
        {
            real32 currentY = line.m * currentX + line.b;
            BufDrawPoint(OB, currentX, currentY);
            currentX -= step;
        }
    }
}

internal void FillBuffer(output_buffer *outputBuffer,
                         char *inputBuffer,
                         int32 numberOfBytesRead,
                         game_state *GS,
                         int32 &running)
{
    outputBuffer->bytesWritten = 0;
    int32 writeIndex = 0;
    BufWrite(outputBuffer,
             "\x1b[0m\x1b[3J\x1b[2J\x1b[H",
             15); // Clear screen, attributes, and move cursor to top left

    if (inputBuffer[0] == 'q')
    {
        running = 0;
    }

    if (inputBuffer[0] == 'j')
    {

        if (GS->selectedListLine > -1)
        {
            if (GS->selectedListLine < GS->boxCount - 1)
            { // -1 because start form 0
                GS->selectedListLine++;
            }
            else if (GS->selectedListLine == GS->boxCount - 1)
            {
                GS->selectedListLine = -1;
                GS->selectedNewItemDimension = 0;
            };
        }
        else if (GS->selectedNewItemDimension > -1)
        {
            if (GS->selectedNewItemDimension < 2)
            {
                GS->selectedNewItemDimension++;
            }
        }
    }

    if (inputBuffer[0] == 'k')
    {
        if (GS->selectedListLine > -1)
        {
            if (GS->selectedListLine > 0)
            {
                GS->selectedListLine--;
            }
        }
        else if (GS->selectedNewItemDimension > -1)
        {
            if (GS->selectedNewItemDimension > 0)
            {
                GS->selectedNewItemDimension--;
            }
            else if (GS->selectedNewItemDimension == 0)
            {
                GS->selectedNewItemDimension = -1;
                GS->selectedListLine = GS->boxCount - 1;
            }
        }
    }

    if (inputBuffer[0] == 'l')
    {
        if (GS->selectedDimension < 2)
        {
            GS->selectedDimension++;
        }
    }

    if (inputBuffer[0] == 'h')
    {
        if (GS->selectedDimension > 0)
        {
            GS->selectedDimension--;
        }
    }

    if (inputBuffer[0] >= '0' && inputBuffer[0] <= '9')
    {
        int32 intInput = inputBuffer[0] - '0';
        box *selectedBox = &GS->boxes[GS->selectedListLine];

        if (GS->selectedDimension >= 0)
        {
            real32 *dimension = &selectedBox->dimensions[GS->selectedDimension];
            *dimension *= 10;
            *dimension += intInput;
        }
    }

    if (inputBuffer[0] == 127)
    {
        box *selectedBox = &GS->boxes[GS->selectedListLine];

        if (GS->selectedDimension >= 0)
        {
            real32 *dimension = &selectedBox->dimensions[GS->selectedDimension];
            *dimension /= 10;
        }
    }

    // ACTUAL_APP:

    // Render Current list
    int32 baseX = 2;
    int32 baseY = 2;

    for (int i = 0; i < GS->boxCount; i++)
    {
        if (i == GS->selectedListLine)
            BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green

        // Print box line
        BufSetPos(outputBuffer, baseX, baseY + i);
        BufWrite(outputBuffer, "Box ", 4);
        BufWriteUInt32(outputBuffer, i);
        BufWrite(outputBuffer, ":", 1);

        if (i == GS->selectedListLine)
            BufWrite(outputBuffer, "\x1b[0m", 4); // Set text atributes to default

        BufWrite(outputBuffer, " ", 1);

        if (GS->selectedDimension == 0 && i == GS->selectedListLine)
            BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green
        BufWrite(outputBuffer, "L", 1);

        BufWriteUInt32(outputBuffer, GS->boxes[i].length);

        if (GS->selectedDimension == 0 && i == GS->selectedListLine)
            BufWrite(outputBuffer, "\x1b[0m", 4); // Set text atributes to default

        BufWrite(outputBuffer, " ", 1);

        if (GS->selectedDimension == 1 && i == GS->selectedListLine)
            BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green

        BufWrite(outputBuffer, "W", 1);
        BufWriteUInt32(outputBuffer, GS->boxes[i].width);

        if (GS->selectedDimension == 1 && i == GS->selectedListLine)
            BufWrite(outputBuffer, "\x1b[0m", 4); // Set text atributes to default

        BufWrite(outputBuffer, " ", 1);

        if (GS->selectedDimension == 2 && i == GS->selectedListLine)
            BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green

        BufWrite(outputBuffer, "H", 1);
        BufWriteUInt32(outputBuffer, GS->boxes[i].height);

        if (GS->selectedDimension == 2 && i == GS->selectedListLine)
            BufWrite(outputBuffer, "\x1b[0m", 4); // Set text atributes to default
    }

    if (GS->selectedNewItemDimension > -1)
    {
        BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green
    }

    // Print box line
    BufSetPos(outputBuffer, baseX, baseY + GS->boxCount);
    BufWrite(outputBuffer, "New Box:", 8);

    if (GS->selectedNewItemDimension > -1)
    {
        BufWrite(outputBuffer, "\x1b[0m", 4); // Set text atributes to default
    }

    int32 newBoxIndex = GS->boxCount;
    BufWrite(outputBuffer, " L", 3);
    BufWriteUInt32(outputBuffer, GS->boxes[newBoxIndex].length);
    BufWrite(outputBuffer, " W", 2);
    BufWriteUInt32(outputBuffer, GS->boxes[newBoxIndex].width);
    BufWrite(outputBuffer, " H", 2);
    BufWriteUInt32(outputBuffer, GS->boxes[newBoxIndex].height);

#if 0
    // Render new item box
    BufSetPos(outputBuffer, baseX + 7, baseY + GS->selectedListLine);

    if (GS->selectedNewItemDimension >= 0)
        BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green
    BufWrite(outputBuffer, "New box: ", 9);
    if (GS->selectedNewItemDimension >= 0)
        BufWrite(outputBuffer, "\x1b[0m", 4); // Set text atributes to default

    if (GS->selectedNewItemDimension == 0)
        BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green
    BufWrite(outputBuffer, "L: ", 3);
    if (GS->selectedNewItemDimension == 0)
        BufWrite(outputBuffer, "\x1b[0m", 4); // Set text atributes to default

    if (GS->selectedNewItemDimension == 1)
        BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green
    BufWrite(outputBuffer, "W: ", 3);
    if (GS->selectedNewItemDimension == 1)
        BufWrite(outputBuffer, "\x1b[0m", 4); // Set text atributes to default

    if (GS->selectedNewItemDimension == 2)
        BufWrite(outputBuffer, "\x1b[42m", 5); // Set background color to green
    BufWrite(outputBuffer, "H: ", 3);
    if (GS->selectedNewItemDimension == 2)
        BufWrite(outputBuffer, "\x1b[0m", 4); // Set text atributes to default
#endif
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
    gameState.selectedNewItemDimension = -1;

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

    return 0;
}
