#include "Process.hpp"
#include "Assert.hpp"
#include "Log.hpp"

#include "Memory.hpp"
#include "String.hpp"

#include <stdio.h>

#if defined(_WIN32)
#include <Windows.h>
#else
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

//Static buffer to log the error coming from windows.
static const uint32_t PROCESS_LOG_BUFFER_SIZE = 256;
char PROCESS_LOG_BUFFER[PROCESS_LOG_BUFFER_SIZE];
static char PROCESS_OUTPUT_BUFFER[1025];

#if defined(_WIN32)
static void win32GetError(char* buffer, uint32_t size) 
{
    DWORD errorCode = GetLastError();

    char* errorString = nullptr;
    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, errorCode,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errorString, 0, nullptr) == false) 
    {
        return;
    }

    sprintf(buffer, "%s", errorString);

    LocalFree(errorString);
}

bool processExecute(const char* workingDirectory, const char* processFullPath, const char* arguments, const char* searchError)
{
    //This has been 'inspired' by a stackover flow answer about how to read stuff from a cmd exe process.
    HANDLE handleStdinPipeRead = nullptr;
    HANDLE handleStdinPipeWrite = nullptr;
    HANDLE handleStdoutPipeRead = nullptr;
    HANDLE handleStdPipeWrite = nullptr;

    SECURITY_ATTRIBUTES securityAttributes = { sizeof(SECURITY_ATTRIBUTES), nullptr, true };

    bool ok = CreatePipe(&handleStdinPipeRead, &handleStdinPipeWrite, &securityAttributes, 0);
    if (ok == false) 
    {
        return false;
    }

    ok = CreatePipe(&handleStdoutPipeRead, &handleStdPipeWrite, &securityAttributes, 0);
    if (ok == false) 
    {
        return false;
    }

    //Create startup information with std redirection.
    STARTUPINFOA startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startupInfo.hStdInput = handleStdinPipeRead;
    startupInfo.hStdError = handleStdPipeWrite;
    startupInfo.hStdOutput = handleStdPipeWrite;
    startupInfo.wShowWindow = SW_SHOW;

    bool executeSuccess = false;
    PROCESS_INFORMATION processInfo{};
    BOOL inheritHandles = true;
    if(CreateProcessA(processFullPath, (char*)arguments, 0, 0, inheritHandles, CREATE_NO_WINDOW, 0, workingDirectory, &startupInfo, &processInfo))
    {
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        executeSuccess = true;
    }
    else 
    {
        win32GetError(&PROCESS_LOG_BUFFER[0], PROCESS_LOG_BUFFER_SIZE);

        vprint("Execute process error.\n Exe \"%s\" - Args: \"%s\" - Work Dir: \"%s\"\n", processFullPath, arguments, workingDirectory);
        vprint("Message: %s\n", PROCESS_LOG_BUFFER);
    }
    CloseHandle(handleStdinPipeRead);
    CloseHandle(handleStdPipeWrite);

    //Output
    DWORD bytesRead;
    ok = ReadFile(handleStdoutPipeRead, PROCESS_OUTPUT_BUFFER, 1024, &bytesRead, nullptr);

    //Consume all output
    //Terinate current read initalise the next.
    while (ok == true) 
    {
        PROCESS_OUTPUT_BUFFER[bytesRead] = 0;
        vprint("%s", PROCESS_OUTPUT_BUFFER);

        ok = ReadFile(handleStdoutPipeRead, PROCESS_OUTPUT_BUFFER, 1024, &bytesRead, nullptr);
    }

    if (strlen(searchError) > 0 && strstr(PROCESS_OUTPUT_BUFFER, searchError))
    {
        executeSuccess = false;
    }

    vprint("\n");

    CloseHandle(handleStdoutPipeRead);
    CloseHandle(handleStdinPipeWrite);

    DWORD processExitCode = 0;
    GetExitCodeProcess(processInfo.hProcess, &processExitCode);

    return executeSuccess;
}

const char* processGetOutput() 
{
    return PROCESS_OUTPUT_BUFFER;
}

#else

bool processExecute(const char *workingDirectory, const char *processFullPath, const char *arguments, const char *searchError)
{
     char currentDir[4096];
    getcwd(currentDir, 4096);

    int result = chdir(workingDirectory);
    VOID_ASSERT(result == 0);

    Allocator* allocator = &MemoryService::instance()->systemAllocator;

    size_t fullCMDSize = strlen(processFullPath) + 1 + strlen(arguments) + 1;
    StringBuffer fullCMDBuffer;
    fullCMDBuffer.init(fullCMDSize, allocator);

    char* fullCMD = fullCMDBuffer.appendUseF("%s %s", processFullPath, arguments);

    //TODO(marco): this works only if one process is started at a time.
    FILE* cmdStream = popen(fullCMD, "r");
    bool executeSuccess = false;
    if (cmdStream != nullptr) 
    {
        result = wait(nullptr);

        size_t readChunkSize = 1024;
        if (result != -1) 
        {
            size_t byteRead = fread(PROCESS_OUTPUT_BUFFER, 1, readChunkSize, cmdStream);
            while (byteRead == readChunkSize) 
            {
                PROCESS_OUTPUT_BUFFER[byteRead] = 0;
                vprint("%s", PROCESS_OUTPUT_BUFFER);

                byteRead = fread(PROCESS_OUTPUT_BUFFER, 1, readChunkSize, cmdStream);
            }

            PROCESS_OUTPUT_BUFFER[byteRead] = 0;
            vprint("%s", PROCESS_OUTPUT_BUFFER);

            if (strlen(searchError) > 0 && strstr(PROCESS_OUTPUT_BUFFER, searchError))
            {
                executeSuccess = false;
            }

            vprint("\n");
        }
        else 
        {
            int error = errno;

            vprint("Execute process error.\n Exe: \"%s\" - Args: \"%s\" - Working Dir: \"%s\"\n", processFullPath, arguments, workingDirectory);
            vprint("Error: %d\n", error);

            executeSuccess = false;
        }

        pclose(cmdStream);
    }
    else 
    {
        int error = errno;

        vprint("Execute process error.\n Exe: \"%s\" - Args: \"%s\" - Working Dir: \"%s\"\n", processFullPath, arguments, workingDirectory);
        vprint("Error: %d\n", error);

        executeSuccess = false;
    }

    chdir(currentDir);
    fullCMDBuffer.shutdown();
    return executeSuccess;   
}

const char *processGetOutput()
{
    return PROCESS_OUTPUT_BUFFER;
}

#endif