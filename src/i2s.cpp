#include "i2s.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>

#include "cmd.h"
#include "directory_listing.h"
#include "disc_image.h"
#include "drive_mechanics.h"
#include "f_util.h"
#include "ff.h"
#include "global.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hw_config.h"
#include "logging.h"
#include "main.pio.h"
#include "modchip.h"
#include "pico/stdlib.h"
#include "picostation.h"
#include "pseudo_atomics.h"
#include "subq.h"
#include "values.h"

#if DEBUG_I2S
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) while (0)
#endif

pseudoatomic<int> g_imageIndex;
pseudoatomic<int> g_listOffset;
pseudoatomic<int> g_directoryIndex;
pseudoatomic<picostation::FileListingStates> needFileCheckAction;
pseudoatomic<int> listReadyState;
pseudoatomic<int> g_entryOffset;

picostation::DiscImage::DataLocation s_dataLocation = picostation::DiscImage::DataLocation::RAM;
static FATFS s_fatFS;

constexpr std::array<uint16_t, 1176> picostation::I2S::generateScramblingLUT() {
    std::array<uint16_t, 1176> cdScramblingLUT = {0};
    int shift = 1;

    for (size_t i = 6; i < 1176; i++) {
        uint8_t upper = shift & 0xFF;
        for (size_t j = 0; j < 8; j++) {
            unsigned bit = ((shift & 1) ^ ((shift & 2) >> 1)) << 15;
            shift = (bit | shift) >> 1;
        }

        uint8_t lower = shift & 0xFF;

        cdScramblingLUT[i] = (lower << 8) | upper;

        for (size_t j = 0; j < 8; j++) {
            unsigned bit = ((shift & 1) ^ ((shift & 2) >> 1)) << 15;
            shift = (bit | shift) >> 1;
        }
    }

    return cdScramblingLUT;
}

void picostation::I2S::mountSDCard() {
    FRESULT fr = f_mount(&s_fatFS, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }
}
const unsigned int c_userDataSize = 2324;

#define MAX_LINES 2000
#define MAX_LENGTH 255

int picostation::I2S::initDMA(const volatile void *read_addr, unsigned int transfer_count) {
    int channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    const unsigned int i2sDREQ = PIOInstance::I2S_DATA == pio0 ? DREQ_PIO0_TX0 : DREQ_PIO1_TX0;
    channel_config_set_dreq(&c, i2sDREQ);
    dma_channel_configure(channel, &c, &PIOInstance::I2S_DATA->txf[SM::I2S_DATA], read_addr, transfer_count, false);

    return channel;
}

[[noreturn]] void __time_critical_func(picostation::I2S::start)(MechCommand &mechCommand) {
    picostation::ModChip modChip;

    static constexpr size_t c_sectorCacheSize = 1;
    int cachedSectors[c_sectorCacheSize];
    int roundRobinCacheIndex = 0;
    static uint16_t cdSamples[c_cdSamplesBytes / sizeof(uint16_t)];  // Make static to move off stack

    static uint32_t pioSamples[2][(c_cdSamplesBytes * 2) / sizeof(uint32_t)];
    static constexpr auto cdScramblingLUT = generateScramblingLUT();

    int bufferForDMA = 1;
    int bufferForSDRead = 0;
    int loadedSector[2];
    int currentSector = -1;
    m_sectorSending = -1;
    int loadedImageIndex = -1;
    int filesinDir = 0;
    int coverOpen = 0;

    g_imageIndex = 0;
    g_directoryIndex = 0;

        printf("start\n");

    int dmaChannel = initDMA(pioSamples[0], c_cdSamplesSize * 2);

    g_coreReady[1] = true;          // Core 1 is ready
    while (!g_coreReady[0].Load())  // Wait for Core 0 to be ready
    {
        tight_loop_contents();
    }

    modChip.init();

#if DEBUG_I2S
    uint64_t startTime = time_us_64();
    uint64_t endTime;
    uint64_t totalTime = 0;
    uint64_t shortestTime = UINT64_MAX;
    uint64_t longestTime = 0;
    unsigned sectorCount = 0;
    unsigned cacheHitCount = 0;
#endif

    char lines[MAX_LINES][MAX_LENGTH];
    int lineCount = 0;
    int sectorNumber;
    mountSDCard();
    printf("mounted SD card!\n");

    int firstboot = 1;
    needFileCheckAction = picostation::FileListingStates::IDLE;
    g_directoryIndex = -1;
    listReadyState = 1;
    g_entryOffset = 0;

    g_discImage.makeDummyCue();
    printf("get from ram!\n");
    uint8_t *emptyBuffer = new uint8_t[2340];
    uint8_t *emptyBuffer2 = new uint8_t[2340];
    memset(emptyBuffer, 0, 2340);
    memset(emptyBuffer2, 0, 2340);

    emptyBuffer[0] = 0;
    emptyBuffer[1] = 0;
    //g_discImage.buildSector(100 + c_preGap, (uint8_t *)&emptyBuffer2, emptyBuffer);


    // this need to be moved to diskimage
    picostation::DirectoryListing::init();
    picostation::DirectoryListing::gotoRoot();
    picostation::DirectoryListing::getDirectoryEntries(0);
    // printf("Directorylisting Entry count: %i", directoryDetails.fileEntryCount);

    while (true) {
        // Update latching, output SENS

        // Sector could change during the loop, so we need to keep track of it
        currentSector = g_driveMechanics.getSector();
        modChip.sendLicenseString(currentSector, mechCommand);

        // Data sent via DMA, load the next sector
        if (bufferForDMA != bufferForSDRead) {
#if DEBUG_I2S
            //    startTime = time_us_64();
#endif

            // Copy CD samples to PIO buffer
            sectorNumber = currentSector - c_leadIn - c_preGap;
            g_discImage.readSector(cdSamples, currentSector - c_leadIn, s_dataLocation);

            if (needFileCheckAction.Load() != picostation::FileListingStates::IDLE) {
                switch (needFileCheckAction.Load()) {
                    case picostation::FileListingStates::GOTO_ROOT: {
                        printf("Processing GOTO_ROOT\n");
                        picostation::DirectoryListing::gotoRoot();
                        g_entryOffset = 0;
                        needFileCheckAction = picostation::FileListingStates::PROCESS_FILES;
                        break;
                    }
                    case picostation::FileListingStates::GOTO_PARENT: {
                        printf("Processing GOTO_PARENT\n");
                        picostation::DirectoryListing::gotoParentDirectory();
                        g_entryOffset = 0;
                        needFileCheckAction = picostation::FileListingStates::PROCESS_FILES;
                        break;
                    }
                    case picostation::FileListingStates::GOTO_DIRECTORY: {
                        printf("Processing GOTO_DIRECTORY %i\n", g_fileArg.Load());
                        picostation::DirectoryListing::gotoDirectory(g_fileArg.Load());
                        g_entryOffset = 0;
                        needFileCheckAction = picostation::FileListingStates::PROCESS_FILES;
                        break;
                    }
                    case picostation::FileListingStates::GET_NEXT_CONTENTS: {
                        printf("Processing GET_NEXT_CONTENTS\n");
                        g_entryOffset = g_fileArg.Load();
                        needFileCheckAction = picostation::FileListingStates::PROCESS_FILES;
                        break;
                    }
                    case picostation::FileListingStates::MOUNT_FILE: {
                        printf("Processing MOUNT_FILE\n");
                        s_dataLocation = picostation::DiscImage::DataLocation::SDCard;
                        char filePath[c_maxFilePathLength + 1];
                        picostation::DirectoryListing::getPath(g_fileArg.Load(), filePath);
                        printf("image cue name:%s\n", filePath);
                        g_discImage.load(filePath);
                        needFileCheckAction = picostation::FileListingStates::IDLE;
                        break;
                    }
                }

                if (needFileCheckAction.Load() == picostation::FileListingStates::PROCESS_FILES) {
                    if (listReadyState.Load() == 0) {
                        g_discImage.buildSector(sectorNumber + c_preGap, (uint8_t *)&cdSamples, emptyBuffer2);
                    } else if (sectorNumber == 100) {
                        printf("sector 100 game read\n");
                        uint8_t *temp = picostation::DirectoryListing::getFileListingData();
                        g_discImage.buildSector(sectorNumber + c_preGap, (uint8_t *)&cdSamples, temp);
                        needFileCheckAction = picostation::FileListingStates::IDLE;
                    }
                }
            }

            int16_t const *sectorData = reinterpret_cast<int16_t *>(cdSamples);

            // Copy CD samples to PIO buffer
            for (size_t i = 0; i < c_cdSamplesSize * 2; i++) {
                uint32_t i2sData;

                if (g_discImage.isCurrentTrackData()) {
                    // Scramble the data
                    i2sData = (sectorData[i] ^ cdScramblingLUT[i]) << 8;
                } else {
                    // Audio track, just copy the data
                    i2sData = (sectorData[i]) << 8;
                }

                if (i2sData & 0x100) {
                    i2sData |= 0xFF;
                }

                pioSamples[bufferForSDRead][i] = i2sData;
            }

#if DEBUG_I2S
            loadedSector[bufferForSDRead] = currentSector;
            bufferForSDRead = (bufferForSDRead + 1) % 2;
            /*    endTime = time_us_64();
                totalTime = endTime - startTime;
                if (totalTime < shortestTime) {
                    shortestTime = totalTime;
                }
                if (totalTime > longestTime) {
                    longestTime = totalTime;
                }*/
            sectorCount++;
#endif
        }

        // Start the next transfer if the DMA channel is not busy
        if (!dma_channel_is_busy(dmaChannel)) {
            bufferForDMA = (bufferForDMA + 1) % 2;
            m_sectorSending = loadedSector[bufferForDMA];
            m_lastSectorTime = time_us_64();

            dma_hw->ch[dmaChannel].read_addr = (uint32_t)pioSamples[bufferForDMA];

            // Sync with the I2S clock
            while (gpio_get(Pin::LRCK) == 1) {
                tight_loop_contents();
            }
            while (gpio_get(Pin::LRCK) == 0) {
                tight_loop_contents();
            }

            dma_channel_start(dmaChannel);
        }

#if DEBUG_I2S
        if (sectorCount >= 100) {
            // DEBUG_PRINT("min: %lluus, max: %lluus cache hits: %u/%u\n", shortestTime, longestTime, cacheHitCount,
            //             sectorCount);
            sectorCount = 0;
            shortestTime = UINT64_MAX;
            longestTime = 0;
            cacheHitCount = 0;
        }
#endif
    }
    __builtin_unreachable();
}
