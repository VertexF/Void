#ifndef LOG_HDR
#define LOG_HDR

#include "Platform.hpp"

#if defined(_MSC_VER)
#include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>

static constexpr int32_t STRING_BUFFER_SIZE = 1024 * 1024;
static char LOG_BUFFER[STRING_BUFFER_SIZE];

static void outputConsole(char* logBuffer)
{
    printf("%s", logBuffer);
}

#if defined(_MSC_VER)
static void outputVisualStudio(char* logBuffer)
{
    OutputDebugStringA(logBuffer);
}
#endif

typedef void (*PrintCallback)(const char*);

struct LogService
{
    static LogService* instance()
    {
        static LogService sLogService;
        return &sLogService;
    }

    void printFormat(const char* format ...) const
    {
        va_list args;
        va_start(args, format);

        vsnprintf(LOG_BUFFER, ArraySize(LOG_BUFFER), format, args);

        LOG_BUFFER[ArraySize(LOG_BUFFER) - 1] = '\0';
        va_end(args);

        outputConsole(LOG_BUFFER);

#if defined(_MSC_VER)
        outputVisualStudio(LOG_BUFFER);
#endif //_MSC_VER

        if (printCallback)
        {
            printCallback(LOG_BUFFER);
        }
    }

    //Mostly for imGui
    void setCallback(PrintCallback callback)
    {
        printCallback = callback;
    }

    PrintCallback printCallback = nullptr;
};

//This enables/disables printing vprint();
#define DEBUG_PRINTING
//This enable/disabled check(); for general error checking.
#define DEBUG_CHECKING
//This enable/disables the vulkan validation layers.
#define VULKAN_DEBUG_REPORT

#if defined(DEBUG_PRINTING)
#if defined(_MSC_VER)
#define vprint(format, ...)    LogService::instance()->printFormat(format, __VA_ARGS__);
#define vprintret(format, ...) LogService::instance()->printFormat(format, __VA_ARGS__); instance()->printFormat("\n");
#else
#define vprint(format, ...)    LogService::instance()->printFormat(format, ##__VA_ARGS__);
#define vprintret(format, ...) LogService::instance()->printFormat(format, ##__VA_ARGS__); instance()->printFormat("\n");
#endif
#else
#define vprint(format, ...)
#define vprintret(format, ...)
#endif

#endif // !LOG_HDR
