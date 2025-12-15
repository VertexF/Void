#ifndef PROCESS_HDR
#define PROCESS_HDR

#include "Platform.hpp"

bool processExecute(const char* workingDirectory, const char* processFullPath, const char* arguments, const char* searchError = "");
const char* processGetOutput();

#endif // !PROCESS_HDR
