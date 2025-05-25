#include "directory_listing.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "f_util.h"
#include "ff.h"
#include "listingBuilder.h"
#include "debug.h"

//#if DEBUG_FILEIO
//#define DEBUG_PRINT(...) picostation::debug::print(__VA_ARGS__)
// #else
// #define DEBUG_PRINT(...) while (0)
// #endif

namespace picostation {

namespace {
    char currentDirectory[c_maxFilePathLength + 1];
    char currentFilter[c_maxFilePathLength + 1];
    listingBuilder* fileListing;
}  // namespace

void DirectoryListing::init() {
    fileListing = new listingBuilder();
    gotoRoot();
    setFilter("");
}

void DirectoryListing::gotoRoot() { 
    currentDirectory[0] = '/';
    currentDirectory[1] = '\0';
}

bool DirectoryListing::gotoDirectory(const uint32_t index) { 
    picostation::debug::print("gotoDirectory: enter\n");
    bool result = getDirectoryEntry(index, currentDirectory); 
    picostation::debug::print("gotoDirectory: %s\n", currentDirectory);
    return result;
}

void DirectoryListing::gotoParentDirectory() {
    uint32_t length = strnlen(currentDirectory, c_maxFilePathLength);
    if (length <= 1) {
        return;
    }

    uint32_t position = length - 1;

    // Remove any trailing /
    if (currentDirectory[position] == '/') {
        currentDirectory[position] = '\0';
        position--;
    }

    // Remove chars until hit /
    while (position > 0) {
        if (currentDirectory[position] == '/') {
            break;
        }
        currentDirectory[position] = '\0';
        position--;
    }

    picostation::debug::print("gotoParentDirectory: %s\n", currentDirectory);
}

void DirectoryListing::setFilter(const char* filter) {
    uint32_t length = strnlen(filter, c_maxFilePathLength);
    strncpy(currentFilter, filter, length);
    currentFilter[length] = '\0';
}

void DirectoryListing::getExtension(const char* filePath, char* extension) {
    const char* lastDotPtr = strrchr(filePath, '.');
    if (lastDotPtr != nullptr && lastDotPtr != filePath) {
        size_t extensionLen = strlen(filePath) - (lastDotPtr - filePath);
        memcpy(extension, lastDotPtr, extensionLen);
        extension[extensionLen] = '\0';
    } else {
        extension[0] = '\0';
    }
}

void DirectoryListing::getPathWithoutExtension(const char* filePath, char* newPath) {
    char extension[c_maxFilePathLength + 1];
    DirectoryListing::getExtension(filePath, extension);
    size_t extensionPos = strlen(filePath) - strlen(extension);
    if (extensionPos > 0) {
        memcpy(newPath, filePath, extensionPos);
        newPath[extensionPos] = '\0';
    } else {
        newPath[0] = '\0';
    }
}

bool DirectoryListing::pathContainsFilter(const char* filePath) {
    if (strlen(currentFilter) == 0) {
        return true;
    }
    char pathWithoutExtension[c_maxFilePathLength + 1];
    getPathWithoutExtension(filePath, pathWithoutExtension);
    return strstr(pathWithoutExtension, currentFilter) != nullptr;
}

bool DirectoryListing::getDirectoryEntry(const uint32_t index, char* filePath) {

    picostation::debug::print("getDirectoryEntry 1 %s\n", currentDirectory);

    DIR dir;
    FILINFO currentEntry;
    FILINFO nextEntry;
    FRESULT res = f_opendir(&dir, currentDirectory);
    if (res != FR_OK) {
       // picostation::debug::print("f_opendir error: %s (%d)\n", FRESULT_str(res), res);
        picostation::debug::print("getDirectoryEntry: failed\n");
        return false;
    }

    picostation::debug::print("getDirectoryEntry 2\n");

    uint32_t filesProcessed = 0;

    res = f_readdir(&dir, &currentEntry);
    picostation::debug::print("getDirectoryEntry 3\n");
    if (res == FR_OK && currentEntry.fname[0] != '\0') {
        res = f_readdir(&dir, &nextEntry);
        bool hasNext = (res == FR_OK && nextEntry.fname[0] != '\0');
        while (true) {
            picostation::debug::print("getDirectoryEntry loop %i\n", filesProcessed);
            if (!(currentEntry.fattrib & AM_HID)) {
                picostation::debug::print("getDirectoryEntry bot hid\n");
                if (pathContainsFilter(currentEntry.fname)) {
                    picostation::debug::print("getDirectoryEntry bot filtered\n");
                    if (filesProcessed == index) {
                        picostation::debug::print("getDirectoryEntry found\n");
                        uint8_t length = strnlen(currentEntry.fname, 255);
                        strncpy(filePath, currentEntry.fname, length);
                        filePath[length] = '\0';
                        picostation::debug::print("Getting directory entry %s\n", filePath);
                        f_closedir(&dir);
                        return true;
                    }
                    picostation::debug::print("getDirectoryEntry not found\n");
                    filesProcessed++;
                }
            }
            if (!hasNext) {
                picostation::debug::print("getDirectoryEntry no next\n");
                break;
            }
            picostation::debug::print("getDirectoryEntry continue\n");
            currentEntry = nextEntry;
            res = f_readdir(&dir, &nextEntry);
            hasNext = (res == FR_OK && nextEntry.fname[0] != '\0');
        }
    }

    f_closedir(&dir);
    picostation::debug::print("getDirectoryEntry: not found\n");
    return false;
}

bool DirectoryListing::getDirectoryEntries(const uint32_t offset) {
    DIR dir;
    FILINFO currentEntry;
    FILINFO nextEntry;
    FRESULT res = f_opendir(&dir, currentDirectory);
    if (res != FR_OK) {
        //picostation::debug::print("f_opendir error: %s (%d)\n", FRESULT_str(res), res);
        picostation::debug::print("getDirectoryEntries: failed\n");
        return false;
    }

    fileListing->clear();

    uint32_t fileEntryCount = 0;
    uint32_t filesProcessed = 0;
    uint8_t hasNext = 1;

    res = f_readdir(&dir, &currentEntry);
    if (res == FR_OK && currentEntry.fname[0] != '\0') {
        res = f_readdir(&dir, &nextEntry);
        bool hasNext = (res == FR_OK && nextEntry.fname[0] != '\0');
        while (true) {
            if (!(currentEntry.fattrib & AM_HID)) {
                if (pathContainsFilter(currentEntry.fname)) {
                    if (filesProcessed >= offset) {
                        if (fileListing->addString(currentEntry.fname, currentEntry.fattrib & AM_DIR ? 1 : 0) ==
                            false) {
                            break;
                        }
                        fileEntryCount++;
                    }
                    filesProcessed++;
                }
            }
            if (!hasNext) {
                break;
            }
            currentEntry = nextEntry;
            res = f_readdir(&dir, &nextEntry);
            hasNext = (res == FR_OK && nextEntry.fname[0] != '\0');
        }
    }
    fileListing->addTerminator(hasNext);
    f_closedir(&dir);
    picostation::debug::print("Files found %i\n", fileEntryCount);
    return true;
}

uint8_t* DirectoryListing::getFileListingData() {
    return fileListing->getData();
}

}  // namespace picostation