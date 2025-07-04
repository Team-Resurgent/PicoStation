#pragma once

#include <stdint.h>

#include <array>
#include "ff.h"
#include "pseudo_atomics.h"
#include "disc_image.h"

namespace picostation {
class MechCommand;

class I2S {
  public:
    I2S() {};
    int getSectorSending() { return m_sectorSending.Load(); }
    uint64_t getLastSectorTime() { return m_lastSectorTime.Load(); }

    [[noreturn]] void start(MechCommand &mechCommand);

  private:
    static constexpr std::array<uint16_t, 1176> generateScramblingLUT();
    int initDMA(const volatile void *read_addr, unsigned int transfer_count);  // Returns DMA channel number
    void mountSDCard();
    void reset();
    pseudoatomic<int> m_sectorSending;
    pseudoatomic<uint64_t> m_lastSectorTime;
};
}  // namespace picostation
extern picostation::DiscImage::DataLocation s_dataLocation;