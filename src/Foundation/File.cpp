#include "File.hpp"

#include "Memory.hpp"
#include "Assert.hpp"
#include "String.hpp"

#if defined(_WIN64)
#include <windows.h>
#else
#define MAX_PATH 65536
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <string.h>

static uint32_t getFileSize(FileHandle file) 
{
    uint32_t fileSizeSigned;

    fseek(file, 0, SEEK_END);
    fileSizeSigned = ftell(file);
    fseek(file, 0, SEEK_SET);

    return fileSizeSigned;
}

static bool stringEndsWithChar(const char* string, char character) 
{
    const char* lastEntry = strchr(string, character);
    const size_t index = lastEntry - string;
    return index == (strlen(string) - 1);
}

//Read file and allocate memory from. User is responsible for freeing the memory.
char* fileReadBinary(const char* filename, Allocator* alloc, size_t* size) 
{
    char* outData = 0;

    FILE* file = fopen(filename, "rb");
    if (file) 
    {
        //TODO: Maybe use filesize or read result.
        size_t filesize = getFileSize(file);

        outData = (char*)void_alloca(filesize + 1, alloc);
        fread(outData, filesize, 1, file);
        outData[filesize] = 0;

        if (size) 
        {
            *size = filesize;
        }

        fclose(file);
    }

    return outData;
}

char* fileReadText(const char* filename, Allocator* alloc, size_t* size)
{
    char* text = 0;
    FILE* file = fopen(filename, "r");

    if (file) 
    {
        size_t filesize = getFileSize(file);
        text = (char*)void_alloca(filesize + 1, alloc);
        //Correct: use element count as filesize, byteRead becomes the actual bytes read.
        //AFTER the end of the line conversation for Windows (it uses \r\n)
        size_t bytesRead = fread(text, 1, filesize, file);

        text[bytesRead] = 0;

        if (size) 
        {
            *size = filesize;
        }

        fclose(file);
    }

    return text;
}

FileReadResult fileReadBinary(const char* filename, Allocator* alloc)
{
    FileReadResult result{ nullptr, 0 };

    FILE* file = fopen(filename, "rb");
    if (file) 
    {
        //TODO: Use filesize or read results.
        uint32_t filesize = getFileSize(file);

        result.data = (char*)void_alloca(filesize, alloc);
        fread(result.data, filesize, 1, file);

        result.size = filesize;

        fclose(file);
    }

    return result;
}

FileReadResult fileReadText(const char* filename, Allocator* alloc)
{
    FileReadResult result{nullptr, 0};

    FILE* file = fopen(filename, "r");
    if (file) 
    {
        size_t filesize = getFileSize(file);
        result.data = (char*)void_alloca(filesize + 1, alloc);
        //Correct: use element count as filesize, byteRead becomes the actual bytes read.
        //AFTER the end of the line conversation for Windows (it uses \r\n)
        size_t byteRead = fread(result.data, 1, filesize, file);

        result.data[byteRead] = 0;
        result.size = byteRead;
        fclose(file);
    }

    return result;
}

void fileWriteBinary(const char* filename, void* memory, size_t size)
{
    FILE* file = fopen(filename, "wb");
    fwrite(memory, size, 1, file);
    fclose(file);
}

bool fileExists(const char* path)
{
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
#else
    int result = access(path, F_OK);
    return (result == 0);
#endif
}

void fileOpen(const char* filename, const char* mode, FileHandle* file)
{
    *file = fopen(filename, mode);
}

void fileClose(FileHandle file)
{
    if (file)
    {
        fclose(file);
    }
}

size_t fileWrite(uint8_t* memory, uint32_t elementSize, uint32_t count, FileHandle file)
{
    return fwrite(memory, elementSize, count, file);
}

bool fileDelete(const char* path)
{
#if defined(_WIN64)
    int result = remove(path);
    return result != 0;
#else
    int result = remove(path);
    return result != 0;
#endif
}

#if defined(_WIN64)
FileTime fileLastWriteTime(const char* filename)
{
    FileTime lastWriteTime{};

    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExA(filename, GetFileExInfoStandard, &data)) 
    {
        lastWriteTime.dwHighDataTime = data.ftLastWriteTime.dwHighDateTime;
        lastWriteTime.dwLowDataTime = data.ftLastWriteTime.dwLowDateTime;
    }

    return lastWriteTime;
}
#endif

//Try tro resolve path to non-relative version.
uint32_t fileResolveToFullPath(const char* path, char* outFullPath, uint32_t maxSize)
{
#if defined(_WIN64)
    return GetFullPathNameA(path, maxSize, outFullPath, nullptr);
#else
    return readlink(path, outFullPath, maxSize);
#endif
}

//Input path methods
//Retrieve path without the filename. Path is a pre-allocated string buffer.
//It moves the terminator before the name of the file.
void fileDirectoryFromPath(char* path)
{
    char* lastPoint = strrchr(path, '.');
    char* lastSeparator = strrchr(path, '/');
    if (lastSeparator != nullptr && lastPoint > lastSeparator) 
    {
        *(lastSeparator + 1) = 0;
    }
    else 
    {
        lastSeparator = strrchr(path, '\\');
        if (lastSeparator != nullptr && lastPoint > lastSeparator)
        {
            *(lastSeparator + 1) = 0;
        }
        else
        {
            VOID_ASSERTM(false, "Malformed path %s", path);
        }
    }
}

void fileNameFromPath(char* path)
{
    char* lastSeparator = strrchr(path, '/');
    if (lastSeparator == nullptr)
    {
        lastSeparator = strrchr(path, '\\');
    }

    if (lastSeparator != nullptr) 
    {
        size_t nameLength = strlen(lastSeparator + 1);
        memoryCopy(path, lastSeparator + 1, nameLength);
        path[nameLength] = 0;
    }
}

char* fileExtensionFromPath(char* path)
{
    char* lastSeparator = strrchr(path, '.');
    return lastSeparator + 1;
}

bool directoryExists(const char* path)
{
#if defined(_WIN64)
    WIN32_FILE_ATTRIBUTE_DATA unused;
    return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
#else
    int result = access(path, F_OK);
    return (result == 0);
#endif
}

bool directoryCreate(const char* path)
{
#if defined(_WIN64)
    int result = CreateDirectoryA(path, nullptr);
    return result != 0;
#else
    int result = mkdir(path, S_IRWXU | S_IRWXG);
    return result != 0;
#endif
}

bool directoryDelete(const char* path)
{
#if defined(_WIN64)
    int result = RemoveDirectoryA(path);
    return result != 0;
#else
    int result = rmdir(path);
    return result != 0;
#endif
}

void directoryCurrent(Directory* directory)
{
#if defined(_WIN64)
    DWORD writtenChars = GetCurrentDirectoryA(MAX_FILE_PATH, directory->path);
    directory->path[writtenChars] = 0;
#else
    getcwd(directory->path, MAX_FILE_PATH);
#endif
}

void directoryChange(const char* path)
{
#if defined(_WIN64)
    if (SetCurrentDirectoryA(path) == false)
    {
        vprint("Cannot change current directory to %s\n", path);
    }
#else
    if (chdir(path) != 0) 
    {
        vprint("Cannot change current directory to %s\n", path);
    }
#endif
}

void fileOpenDirectory(const char* path, Directory* outDirectory)
{
    //Open file trying to cover to full path instead of relative. If an error occurs, just copy the name.
    if (fileResolveToFullPath(path, outDirectory->path, MAX_PATH) == 0) 
    {
        strcpy(outDirectory->path, path);
    }

    //Add '\\' if missing.
    if (stringEndsWithChar(path, '\\') == false)
    {
        strcat(outDirectory->path, "\\");
    }

    if (stringEndsWithChar(outDirectory->path, '*') == false)
    {
        strcat(outDirectory->path, "*");
    }

#if defined(_WIN64)
    outDirectory->osHandle = nullptr;

    WIN32_FIND_DATAA findData;
    HANDLE foundHandle = FindFirstFileA(outDirectory->path, &findData);
    if (foundHandle != INVALID_HANDLE_VALUE) 
    {
        outDirectory->osHandle = foundHandle;
    }
    else 
    {
        vprint("Could not open directory %s\n", outDirectory->path);
    }
#else
    VOID_ASSERTM(false, "TODO: implemet none windows version.");
#endif
}

void fileCloseDirectory(Directory* directory)
{
#if defined(_WIN64)
    if (directory->osHandle) 
    {
        FindClose(directory->osHandle);
    }
#else
    VOID_ASSERTM(false, "TODO: implemet none windows version.");
#endif
}

void fileParentDirectory(Directory* directory)
{
    Directory newDirectory;

    const char* lastDirectorySeparator = strrchr(directory->path, '\\');
    size_t index = lastDirectorySeparator - directory->path;

    if (index > 0) 
    {
        strncpy(newDirectory.path, directory->path, index);
        newDirectory.path[index] = 0;

        lastDirectorySeparator = strrchr(newDirectory.path, '\\');
        size_t secondIndex = lastDirectorySeparator - newDirectory.path;

        if (lastDirectorySeparator)
        {
            newDirectory.path[secondIndex] = 0;
        }
        else 
        {
            newDirectory.path[index] = 0;
        }

        fileOpenDirectory(newDirectory.path, &newDirectory);

#if defined(_WIN64)
        //Update directory 
        if (newDirectory.osHandle) 
        {
            *directory = newDirectory;
        }
#else
        VOID_ASSERTM(false, "Not yet implemented.");
#endif
    }
}

void fileSubDirectory(Directory* directory, const char* subDirectoryName)
{
    //Remove the last '*' from the. It will be re-added by the fileOpen
    if (stringEndsWithChar(directory->path, '*'))
    {
        directory->path[strlen(directory->path) - 1] = 0;
    }

    strcat(directory->path, subDirectoryName);
    fileOpenDirectory(directory->path, directory);
}

//Searches files matching filePatterns and puts them in files.
//Examples: "..\\data\\*, "*.bin", "*.*"
void fileFindFilesInPath(const char* filePattern, StringArray& files)
{
    files.clear();

#if defined(_WIN64)
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(filePattern, &findData);
    if (hFind != INVALID_HANDLE_VALUE) 
    {
        do 
        {
            files.intern(findData.cFileName);
        } while (FindNextFileA(hFind, &findData) != 0);
    }
    else 
    {
        vprint("Cannot find file %s\n", filePattern);
    }
#else
    VOID_ASSERTM(false, "Not yet implemented.");
#endif
}

//Searches files and directories using searchPatterns 
void fileFindFileInPath(const char* extension, const char* searchPattern, StringArray& files, StringArray& directories)
{
    files.clear();
    directories.clear();

#if defined(_WIN64)
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPattern, &findData);
    if (hFind != INVALID_HANDLE_VALUE) 
    {
        do 
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
            {
                directories.intern(findData.cFileName);
            }
            else 
            {
                //If the filename contains the extension add it.
                if (strstr(findData.cFileName, extension)) 
                {
                    files.intern(findData.cFileName);
                }
            }
        } while (FindNextFileA(hFind, &findData) != 0);
        FindClose(hFind);
    }
    else 
    {
        vprint("Cannot find directory %s\n", searchPattern);
    }
#else
    VOID_ASSERTM(false, "Not yet implemented.");
#endif
}

//TODO: Apprently this is a bad place to get enviroment variables.
void getEnvironmnetVariable(const char* name, char* output, uint32_t outputSize)
{
#if defined(_WIN64)
    ExpandEnvironmentStringsA(name, output, outputSize);
#else
    const char* realOutput = getenv(name);
    strncpy(output, realOutput, outputSize);
#endif
}

ScopedFile::ScopedFile(const char* filename, const char* mode)
{
    fileOpen(filename, mode, &file);
}

ScopedFile::~ScopedFile()
{
    fileClose(file);
}
