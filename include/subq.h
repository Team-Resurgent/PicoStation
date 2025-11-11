#pragma once

#include <stdint.h>

namespace picostation {
class DiscImage;

class SubQ {
  public:
    struct Data  // Mode 1 for DATA-Q
    {
        union {
            struct {
                uint8_t ctrladdr;  // [0] 4 bits control, 4 bits address
                uint8_t tno;       // [1] Track number
                union {
                    uint8_t point, x;  // [2]
                };
                uint8_t min;    // [3]
                uint8_t sec;    // [4]
                uint8_t frame;  // [5]
                uint8_t zero;   // [6]
                union {
                    uint8_t amin, pmin;  // [7]
                };
                union {
                    uint8_t asec, psec;  // [8]
                };
                union {
                    uint8_t aframe, pframe;  // [9]
                };
                uint16_t crc;  // [10],[11]
            };
            uint8_t raw[12];
        };
    };

    SubQ(DiscImage *discImage) : m_discImage(discImage) {}
    void start_subq(const int sector);
    //void stop_subq();

  private:
    void printf_subq(const uint8_t *data);

    DiscImage *m_discImage;
};
}  // namespace picostation
