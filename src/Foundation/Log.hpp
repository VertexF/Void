#ifndef LOG_HDR
#define LOG_HDR

#include "Platform.hpp"
#include "File.hpp"
#include "Memory.hpp"

#if defined(_MSC_VER)
#include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <mutex>

static constexpr int32_t STRING_BUFFER_SIZE = 1024 * 1024;
static char LOG_BUFFER[STRING_BUFFER_SIZE];
inline std::mutex logLock;

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

        int stringLength = vsnprintf(LOG_BUFFER, ArraySize(LOG_BUFFER), format, args);


        LOG_BUFFER[ArraySize(LOG_BUFFER) - 1] = '\0';
        stringLength += 1;
        va_end(args);

        //Can be used to write to disk if needed, commented for speed reasons.
        //fileWriteText("allocationTest.txt", LOG_BUFFER, ArraySize(LOG_BUFFER), stringLength);

        outputConsole(LOG_BUFFER);

#if defined(_MSC_VER)
        outputVisualStudio(LOG_BUFFER);
#endif //_MSC_VER

        if (printCallback)
        {
            printCallback(LOG_BUFFER);
        }
    }

    void threadedPrintFormat(const char* format ...) const
    {
        const std::lock_guard<std::mutex> lockGuard{ logLock };
        va_list args;
        va_start(args, format);

        int stringLength = vsnprintf(LOG_BUFFER, ArraySize(LOG_BUFFER), format, args);

        LOG_BUFFER[ArraySize(LOG_BUFFER) - 1] = '\0';
        stringLength += 1;
        va_end(args);

        //Can be used to write to disk if needed, commented for speed reasons.
        //fileWriteText("allocationTest.txt", LOG_BUFFER, ArraySize(LOG_BUFFER), stringLength);

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
#define vprint(format, ...)       LogService::instance()->printFormat(format, __VA_ARGS__);
#define vprintret(format, ...)    LogService::instance()->printFormat(format, __VA_ARGS__); instance()->printFormat("\n");
#define vthreadprint(format, ...) LogService::instance()->threadedPrintFormat(format, __VA_ARGS__);
#else
#define vprint(format, ...)       LogService::instance()->printFormat(format, ##__VA_ARGS__);
#define vprintret(format, ...)    LogService::instance()->printFormat(format, ##__VA_ARGS__); instance()->printFormat("\n");
#define vthreadprint(format, ...) LogService::instance()->threadedPrintFormat(format, ##__VA_ARGS__);
#endif
#else
#define vprint(format, ...)
#define vprintret(format, ...)
#endif

#endif // !LOG_HDR
