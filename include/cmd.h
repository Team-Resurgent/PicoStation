#pragma once

#include <stddef.h>
#include <stdint.h>

#include "drive_mechanics.h"
#include "pseudo_atomics.h"

namespace picostation {

class MechCommand {
  public:
    bool getSens(const size_t what) const;
    void setSens(const size_t what, const bool new_value);
    bool getSoct();
    void setSoct(const bool new_value);
    void processLatchedCommand();
    void updateMechSens();

    void updateAutoSeqTrack();

  private:
    enum TopLevelCommands : uint32_t {
        FOCUS_CONTROL = 0x0,
        TRACKING_MODE = 0x2,
        AUTO_SEQUENCE = 0x4,
        JUMP_COUNT = 0x7,
        MODE_SPEC = 0x8,
        FUNC_SPEC = 0x9,
        MONITOR_COUNT = 0xb,
        SPINDLE = 0xe,
        CUSTOM = 0xf
    };

    enum SpindleCommands : uint32_t {
        STOP = 0b0000,   // 0
        KICK = 0b1000,   // 8
        BRAKE = 0b1010,  // A
        CLVS = 0b1110,   // E
        CLVH = 0b1100,   // C
        CLVP = 0b1111,   // F
        CLVA = 0b0110,   // 6
    };

    void audioControl(const uint32_t latched);
    void autoSequence(const uint32_t latched);
    void customCommand(const uint32_t latched);
    void funcSpec(const uint32_t latched);
    void modeSpec(const uint32_t latched);
    void trackingMode(const uint32_t latched);
    void spindleControl(const uint32_t latched);

    alarm_id_t m_autoSeqAlarmID = 0;
    int m_autoSeqTrack = 0;
    int m_jumpTrack = 0;

    uint16_t ioCommand = 0;
    static const uint8_t gameIdLen = 100;
    char gameId[gameIdLen + 1];
    uint16_t gameIdIndex = 0;
    uint32_t m_latched = 0;  // Command latch

    size_t m_currentSens = 0;
    bool m_sensData[16] = {
        0,  // $0X - FZC
        0,  // $1X - AS
        0,  // $2X - TZC
        0,  // $3X - Misc.
        0,  // $4X - XBUSY
        1,  // $5X - FOK
        0,  // $6X - 0
        0,  // $7X - 0
        0,  // $8X - 0
        0,  // $9X - 0
        1,  // $AX - GFS
        0,  // $BX - COMP
        0,  // $CX - COUT
        0,  // $DX - 0
        0,  // $EX - OV64
        0   // $FX - 0
    };
    pseudoatomic<bool> m_soctEnabled;
};
}  // namespace picostation
