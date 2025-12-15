#include "Time.hpp"

#if defined(_MSC_VER)
#include <Windows.h>
#else
#include <time.h>
#endif

#if defined(_MSC_VER)
    //https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency
    //The frequency of the performance counter is fixed at system boot and is consistent across all processors.
    //Therefore, the frequency need only be queried upon application initalisation, and the result can be cached.
    static LARGE_INTEGER frequency;
#endif

//Computes the (value * numerator) / denomator without overflow, as long as 
//both numerator * denomator fit into the int64_t
static int64_t int64MulDiv(int64_t value, int64_t numerator, int64_t denomator) 
{
    const int64_t q = value / denomator;
    const int64_t r = value % denomator;

    return (q * numerator + r * numerator / denomator);
}

void timeServiceInit() 
{
#if defined(_MSC_VER)
    QueryPerformanceFrequency(&frequency);
#endif
}

void timeServiceShutdown() 
{
}

int64_t timeNow() 
{
#if defined(_MSC_VER)
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);

    //Convert to micro seconds with 1000000LL.
    const int64_t microseconds = int64MulDiv(time.QuadPart, 1000000LL, frequency.QuadPart);
#else
    timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);

    const uint64_t now = tp.tv_sec * 1000000000 + tp.tv_nsec;
    const int64_t microseconds = now / 1000;
#endif

    return microseconds;
}

double timeMicroseconds(int64_t time) 
{
    return static_cast<double>(time);
}

double timeMilliseconds(int64_t time) 
{
    return static_cast<double>(time) / 1000.0;
}

double timeSeconds(int64_t time) 
{
    return static_cast<double>(time) / 1000000.0;
}

int64_t timeFrom(int64_t startTime)
{
    return timeNow() - startTime;
}

double timeFromMicroseconds(int64_t startingTime) 
{
    return timeMicroseconds(timeFrom(startingTime));
}

double timeFromMilliseconds(int64_t startingTime) 
{
    return timeMilliseconds(timeFrom(startingTime));
}

double timeFromSeconds(int64_t startingTime) 
{
    return timeSeconds(timeFrom(startingTime));
}

double timeDeltaSeconds(int64_t startingTime, int64_t endingTime) 
{
    return timeSeconds(endingTime - startingTime);
}

double timeDeltaMilliseconds(int64_t startingTime, int64_t endingTime) 
{
    return timeMilliseconds(endingTime - startingTime);
}