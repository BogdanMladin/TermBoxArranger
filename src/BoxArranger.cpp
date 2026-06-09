#include "BoxArranger.h"

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

// TODO Check if this funciton still exists or makes sense
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

// Writes to the output buffer a line of the following format:
// Box boxNumber: Llength Wwidth Hheight
// Eg: Box 1: L2 W3 H4
internal void BufPrintBoxLine(
    output_buffer *OB, int32 boxNumber, int32 length, int32 width, int32 height)
{
    BufWrite(OB, "Box ", 4);
    BufWriteUInt32(OB, boxNumber);
    BufWrite(OB, ":", 1);

    BufWrite(OB, " ", 1);

    BufWrite(OB, "L", 1);
    BufWriteUInt32(OB, length);

    BufWrite(OB, " ", 1);

    BufWrite(OB, "W", 1);
    BufWriteUInt32(OB, width);

    BufWrite(OB, " ", 1);

    BufWrite(OB, "H", 1);
    BufWriteUInt32(OB, height);
}

// Returns a pointer to the box at index index
// Expects that the index is not larger than the number
// of boxes in the list
internal box *GetBoxAtIndex(box *head, int32 index)
{
    box *result = head;
    while (index)
    {
        assert(result->next); // If this fires index is out of bounds
        result = result->next;
        index--;
    }
    return result;
}

internal void FillBuffer(
    output_buffer *OB, char *inputBuffer, int32 numberOfBytesRead, game_state *GS, int32 *running)
{
    OB->bytesWritten = 0;
    int32 writeIndex = 0;

    // Clear screen, attributes, and move cursor to top left
    BufWrite(OB, "\x1b[0m\x1b[3J\x1b[2J\x1b[H", 15);

    if (inputBuffer[0] == 'q')
    {
        *running = 0;
    }

    if (inputBuffer[0] == 'j')
    {
        if (GS->selectedListLine > -1)
        {
            if (GS->selectedListLine < GS->boxCount)
            { // -1 because start form 0
                GS->selectedListLine++;
            }
            if (GS->selectedListLine == GS->boxCount)
            {
                GS->selectedListLine = -1;
                GS->selectedNewBox = 1;
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
        if (GS->selectedNewBox)
        {
            GS->selectedNewBox = false;
            GS->selectedListLine = GS->boxCount - 1;
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
        box *selectedBox;
        if (GS->selectedListLine > -1)
        {
            selectedBox = GetBoxAtIndex(GS->boxHead, GS->selectedListLine);
            if (GS->selectedDimension >= 0)
            {
                int32 *dimension = &selectedBox->dimensions[GS->selectedDimension];
                *dimension *= 10;
                *dimension += intInput;
            }
        }
        else if (GS->selectedNewBox){
            // TODO: create new box funciton here
        }
    }

    if (inputBuffer[0] == 127)
    {
        box *selectedBox = &GS->boxes[GS->selectedListLine];

        if (GS->selectedDimension >= 0)
        {
            int32 *dimension = &selectedBox->dimensions[GS->selectedDimension];
            *dimension /= 10;
        }
    }

#if 0
    // because it starts from 0 this is +1 actually
    if (GS->boxes[GS->boxCount].height || GS->boxes[GS->boxCount].length ||
        GS->boxes[GS->boxCount].width)
    {
        GS->boxCount++;
    }
#endif

    // ACTUAL_APP:

    // Render Current list
    int32 baseX = 2;
    int32 baseY = 2;

    box *inext = GS->boxHead;
    int32 i = 0;

    while (inext)
    {
        // Print box line
        BufSetPos(OB, baseX, baseY + i);
        BufPrintBoxLine(OB, i, inext->length, inext->width, inext->height);

        inext = inext->next;
        i++;
    }
    // Print new box line
    BufSetPos(OB, baseX, baseY + i);
    BufPrintBoxLine(OB, i, 0, 0, 0);

    // Render Highlights

    BufWrite(OB,
             SET_BACKGROUND_GREEN,
             sizeof(SET_BACKGROUND_GREEN)); // Set background color to green
    //  TODO: This seems like not the way this should be done
    int32 *selectedDimensions;
    int32 emptyArray[3] = {0, 0, 0};
    int32 selectedLine;

    if (GS->selectedListLine > -1)
    {
        box *selectedBox = GetBoxAtIndex(GS->boxHead, GS->selectedListLine);
        selectedDimensions = selectedBox->dimensions;
        selectedLine = GS->selectedListLine;
    }
    else if (GS->selectedNewBox)
    {
        selectedDimensions = emptyArray;
        selectedLine = GS->boxCount;
    }

    BufSetPos(OB, baseX, baseY + selectedLine);
    BufWrite(OB, "Box ", 4);
    BufWriteUInt32(OB, selectedLine);
    BufWrite(OB, ":", 1);
    int32 newX = baseX;
    newX += 4 + CharLenUInt32(selectedLine) + 2;
    for (int i = 0; i < GS->selectedDimension; i++)
    {
        newX += 2;
        newX += CharLenUInt32(selectedDimensions[i]);
    }
    BufSetPos(OB, newX, baseY + selectedLine);
    switch (GS->selectedDimension)
    {
    case 0:
        BufWrite(OB, "L", 1);
        break;
    case 1:
        BufWrite(OB, "W", 1);
        break;
    case 2:
        BufWrite(OB, "H", 1);
        break;
    }

    // Selected dimension number
    BufWriteUInt32(OB, selectedDimensions[GS->selectedDimension]);

    BufWrite(OB,
             SET_DEFAULT_ATTRIBUTES,
             sizeof(SET_DEFAULT_ATTRIBUTES)); // Set text atributes to default
}
