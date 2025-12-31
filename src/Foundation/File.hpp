#ifndef FILE_HDR
#define FILE_HDR

#include "Platform.hpp"
#include <stdio.h>

struct Allocator;
struct StringArray;

#if defined(_WIN64)

typedef struct __FILETIME 
{
    unsigned long dwLowDataTime;
    unsigned long dwHighDataTime;
} FILE_TIME, *PFILE_TIME, *LPFILE_TIME;

using FileTime = __FILETIME;

#endif

using FileHandle = FILE*;
static const uint32_t MAX_FILE_PATH = 512;
struct Directory 
{
    char path[MAX_FILE_PATH];
#if defined(_WIN64)
    void* osHandle;
#endif
};

struct FileReadResult 
{
    char* data = nullptr;
    uint32_t size = 0;
};

//Read file and allocate memory from. User is responsible for freeing the memory.
char* fileReadBinary(const char* filename, Allocator* alloc, size_t* size);
char* fileReadText(const char* filename, Allocator* alloc, size_t* size);

FileReadResult fileReadBinary(const char* filename, Allocator* alloc);
FileReadResult fileReadText(const char* filename, Allocator* alloc);

void fileWriteBinary(const char* filename, void* memory, size_t size);

bool fileExists(const char* path);
void fileOpen(const char* filename, const char* mode, FileHandle* file);
void fileClose(FileHandle file);
size_t fileWrite(uint8_t* memory, uint32_t elementSize, uint32_t count, FileHandle file);
bool fileDelete(const char* path);

#if defined(_WIN64)
FileTime fileLastWriteTime(const char* filename);
#endif

//Try tro resolve path to non-relative version.
uint32_t fileResolveToFullPath(const char* path, char* outFullPath, uint32_t maxSize);

//Input path methods
//Retrieve path without the filename. Path is a pre-allocated string buffer.
//It moves the terminator before the name of the file.
void fileDirectoryFromPath(char* path);
void fileNameFromPath(char* path);
char* fileExtensionFromPath(char* path);

bool directoryExists(const char* path);
bool directoryCreate(const char* path);
bool directoryDelete(const char* path);

void directoryCurrent(Directory* directory);
void directoryChange(const char* path);

void fileOpenDirectory(const char* path, Directory* outDirectory);
void fileCloseDirectory(Directory* directory);
void fileParentDirectory(Directory* directory);
void fileSubDirectory(Directory* directory, const char* subDirectoryName);

//Searches files matching filePatterns and puts them in files.
//Examples: "..\\data\\*, "*.bin", "*.*"
void fileFindFilesInPath(const char* filePattern, StringArray& files);

//Searches files and directories using searchPatterns 
void fileFindFileInPath(const char* extension, const char* searchPattern, StringArray& files, StringArray& directories);

//TODO: Apprently this is a bad place to get enviroment variables.
void getEnvironmnetVariable(const char* name, char* output, uint32_t outputSize);

struct ScopedFile 
{
    ScopedFile(const char* filename, const char* mode);
    ~ScopedFile();

    FileHandle file{};
};


#endif // !FILE_HDR
