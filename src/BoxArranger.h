
#include <cassert>
#include <math.h>
#include <stdint.h>

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

#define SET_BACKGROUND_GREEN "\x1b[42m"
#define SET_DEFAULT_ATTRIBUTES "\x1b[0m"

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
            int32 length;
            int32 width;
            int32 height;
        };
        int32 dimensions[3];
    };
    box* next;
};

struct game_state
{
    int32 selectedListLine;
    int32 selectedNewBox;
    int32 selectedDimension;

    box boxes[12];
    box *boxHead;
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
