#include "Numerics.hpp"

#include "Assert.hpp"
#include "Log.hpp"

#include <cmath>
#include <stdlib.h>


#if defined(VOID_MATH_OVERFLOW_CHECK)
uint32_t ceilU32(float value) 
{
    int64_t valueContainer = (int64_t)ceilf(value);
    if (abs(valueContainer) > UINT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT32_MAX);
    }
    return (uint32_t)valueContainer;
}

uint32_t ceilU32(double value) 
{
    int64_t valueContainer = (int64_t)ceil(value); 
    if (abs(valueContainer) > UINT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT32_MAX);
    }
    return (uint32_t)valueContainer;
}

uint16_t ceilU16(float value) 
{
    int64_t valueContainer = (int64_t)ceilf(value); 
    if (abs(valueContainer) > UINT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT16_MAX);
    }
    return (uint16_t)valueContainer;
} 
    
uint16_t ceilU16(double value)
{
    int64_t valueContainer = (int64_t)ceil(value); 
    if (abs(valueContainer) > UINT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT16_MAX);
    }
    return (uint16_t)valueContainer;
}

int32_t ceilI32(float value)
{
    int64_t valueContainer = (int64_t)ceilf(value);
    if (abs(valueContainer) > INT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT32_MAX);
    }
    return (int32_t)valueContainer;
}

int32_t ceilI32(double value)
{
    int64_t valueContainer = (int64_t)ceil(value);
    if (abs(valueContainer) > INT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT32_MAX);
    }
    return (int32_t)valueContainer;
}

int16_t ceilI16(float value)
{
    int64_t valueContainer = (int64_t)ceilf(value);
    if (abs(valueContainer) > UINT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT16_MAX);
    }
    return (int16_t)valueContainer;

}

int16_t ceilI16(double value)
{
    int64_t valueContainer = (int64_t)ceil(value);
    if (abs(valueContainer) > UINT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT16_MAX);
    }
    return (int16_t)valueContainer;
}

uint32_t floorU32(float value)
{
    int64_t valueContainer = (int64_t)floorf(value);
    if (abs(valueContainer) > UINT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT32_MAX);
    }
    return (uint32_t)valueContainer;
}

uint32_t floorU32(double value)
{
    int64_t valueContainer = (int64_t)floor(value);
    if (abs(valueContainer) > UINT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT32_MAX);
    }
    return (uint32_t)valueContainer;
}

uint16_t floorU16(float value)
{
    int64_t valueContainer = (int64_t)floorf(value);
    if (abs(valueContainer) > UINT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT16_MAX);
    }
    return (uint16_t)valueContainer;
}

uint16_t floorU16(double value)
{
    int64_t valueContainer = (int64_t)floor(value);
    if (abs(valueContainer) > UINT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT16_MAX);
    }
    return (uint16_t)valueContainer;
}

int32_t floorI32(float value)
{
    int64_t valueContainer = (int64_t)floorf(value);
    if (abs(valueContainer) > INT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT32_MAX);
    }
    return (int32_t)valueContainer;
}

int32_t floorI32(double value)
{
    int64_t valueContainer = (int64_t)floor(value);
    if (abs(valueContainer) > INT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT32_MAX);
    } 
    return (int32_t)valueContainer;
}

int16_t floorI16(float value)
{
    int64_t valueContainer = (int64_t)floorf(value);
    if (abs(valueContainer) > INT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT16_MAX);
    }
    return (int16_t)valueContainer;
}

int16_t floorI16(double value)
{
    int64_t valueContainer = (int64_t)floor(value);
    if (abs(valueContainer) > INT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT16_MAX);
    }
    return (int16_t)valueContainer;
}

uint32_t roundU32(float value)
{
    int64_t valueContainer = (int64_t)roundf(value);
    if (abs(valueContainer) > UINT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT32_MAX);
    }
    const uint32_t result = (uint32_t)valueContainer;
    return result;
}

uint32_t roundU32(double value)
{
    int64_t valueContainer = (int64_t)round(value);
    if (abs(valueContainer) > UINT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT32_MAX);
    }
    return (uint32_t)valueContainer;
}

uint16_t roundU16(float value)
{
    int64_t valueContainer = (int64_t)roundf(value);
    if (abs(valueContainer) > UINT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT32_MAX);
    }
    return (uint16_t)valueContainer;
}

uint16_t roundU16(double value)
{
    int64_t valueContainer = (int64_t)round(value);
    if (abs(valueContainer) > UINT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, UINT16_MAX);
    }
    return (uint16_t)valueContainer;
}

int32_t roundI32(float value)
{
    int64_t valueContainer = (int64_t)roundf(value);
    if (abs(valueContainer) > INT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT16_MAX);
    }
    return (int32_t)valueContainer;
}

int32_t roundI32(double value)
{
    int64_t valueContainer = (int64_t)round(value);
    if (abs(valueContainer) > INT32_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT32_MAX);
    }
    return (int32_t)valueContainer;
}

int16_t roundI16(float value)
{
    int64_t valueContainer = (int64_t)roundf(value);
    if (abs(valueContainer) > INT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT16_MAX);
    }
    return (int16_t)valueContainer;
}

int16_t roundI16(double value)
{
    int64_t valueContainer = (int64_t)round(value);
    if (abs(valueContainer) > INT16_MAX)
    {
        vprint("Overflow converting values %llu, %llu\n", valueContainer, INT16_MAX);
    }
    return (int16_t)valueContainer;
}
#else
uint32_t ceilU32(float value)  { return static_cast<uint32_t>(ceilf(value)); }
uint32_t ceilU32(double value) { return static_cast<uint32_t>(ceil(value));  }
uint16_t ceilU16(float value)  { return static_cast<uint16_t>(ceilf(value)); }
uint16_t ceilU16(double value) { return static_cast<uint16_t>(ceil(value));  }

int32_t ceilI32(float value)  { return static_cast<int32_t>(ceilf(value)); }
int32_t ceilI32(double value) { return static_cast<int32_t>(ceil(value));  }
int16_t ceilI16(float value)  { return static_cast<int16_t>(ceilf(value)); }
int16_t ceilI16(double value) { return static_cast<int16_t>(ceil(value));  }

uint32_t floorU32(float value)  { return static_cast<uint32_t>(floorf(value)); }
uint32_t floorU32(double value) { return static_cast<uint32_t>(floor(value));  }
uint16_t floorU16(float value)  { return static_cast<uint16_t>(floorf(value)); }
uint16_t floorU16(double value) { return static_cast<uint16_t>(floor(value));  }

int32_t floorI32(float value)  { return static_cast<int32_t>(floorf(value)); }
int32_t floorI32(double value) { return static_cast<int32_t>(floor(value));  }
int16_t floorI16(float value)  { return static_cast<int16_t>(floorf(value)); }
int16_t floorI16(double value) { return static_cast<int16_t>(floor(value));  }

uint32_t roundU32(float value)  { return static_cast<uint32_t>(roundf(value)); }
uint32_t roundU32(double value) { return static_cast<uint32_t>(round(value));  }
uint16_t roundU16(float value)  { return static_cast<uint16_t>(roundf(value)); }
uint16_t roundU16(double value) { return static_cast<uint16_t>(round(value));  }

int32_t roundI32(float value)  { return static_cast<int32_t>(roundf(value)); }
int32_t roundI32(double value) { return static_cast<int32_t>(round(value));  }
int16_t roundI16(float value)  { return static_cast<int16_t>(roundf(value)); }
int16_t roundI16(double value) { return static_cast<int16_t>(round(value));  }
#endif

float getRandomValue(float min, float max) 
{
    VOID_ASSERT(min < max);

    float random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    random = (max - min) * random + min;

    return random;
}