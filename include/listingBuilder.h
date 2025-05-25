#pragma once

#include <string.h>
#include <cstdio>

#define LISTING_SIZE 2324 

class listingBuilder {
  public:
    listingBuilder() {
        clear();
    }

    bool addString(const char* value, uint8_t flags) {
        uint8_t pathLen = strnlen(value, 255);
        uint16_t sizeToAdd = 2 + pathLen;
        if ((mSize + sizeToAdd + 2) > LISTING_SIZE) {
            return false;
        }
        mValuesContainer[mSize] = pathLen;
        mValuesContainer[mSize + 1] = flags;
        memcpy(mValuesContainer + mSize + 2, value, pathLen);
        mSize += sizeToAdd;
        return true;
    }

    bool addTerminator(uint8_t hasNext) {
        if ((mSize + 2) > LISTING_SIZE) {
            return false;
        }
        mValuesContainer[mSize] = 0;
        mValuesContainer[mSize + 1] = hasNext;
        mSize += 2;
        return true;
    }

    uint8_t* getData() { return mValuesContainer; }

    uint32_t size() { return mSize; }

    char* getString(uint16_t index)
    {
        static char result[256];

        uint16_t offset = 0;
        uint16_t currentPos = 0;
        while (offset < LISTING_SIZE)
        {
            if (currentPos == index)
            {
                uint16_t length = mValuesContainer[offset];
                strncpy(result, (char*)&mValuesContainer[offset + 2], length);
                result[length] = '\0';
                return result;
            }
            uint8_t strLen = mValuesContainer[offset];
            if (strLen == 0)
            {
                break;
            }
            offset += strLen + 2;
            currentPos++;
        }
        return nullptr;
    }

    void clear() {
        mSize = 0;
        memset(mValuesContainer, 0, LISTING_SIZE);
    }

  private:
    uint8_t mValuesContainer[LISTING_SIZE];
    uint32_t mSize;
};