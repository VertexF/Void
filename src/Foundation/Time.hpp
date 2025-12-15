#ifndef TIME_HDR
#define TIME_HDR

#include "Platform.hpp"

void timeServiceInit();
void timeServiceShutdown();

int64_t timeNow();

double timeMicroseconds(int64_t time);
double timeMilliseconds(int64_t time);
double timeSeconds(int64_t time);

int64_t timeFrom(int64_t startTime);
double timeFromMicroseconds(int64_t startingTime);
double timeFromMilliseconds(int64_t startingTime);
double timeFromSeconds(int64_t startingTime);

double timeDeltaSeconds(int64_t startingTime, int64_t endingTime);
double timeDeltaMilliseconds(int64_t startingTime, int64_t endingTime);

#endif // !TIME_HDR
