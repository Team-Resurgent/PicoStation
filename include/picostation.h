#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "pseudo_atomics.h"

namespace picostation {

/*class ODE {
  public:
  private:
    MechCommand m_mechCommand;
};*/

namespace audioControlModes {
enum : unsigned int {
    NORMAL = 0b00,
    LEVELMETER = 0b01,
    PEAKMETER = 0b10,
    ALTNORMAL = 0b11,
};
}

enum class Command {
    COMMAND_NONE = 0x0,
    COMMAND_GOTO_ROOT = 0x1,
    COMMAND_GOTO_PARENT = 0x2,
    COMMAND_GOTO_DIRECTORY = 0x3,
    COMMAND_GET_NEXT_CONTENTS = 0x4,
    COMMAND_MOUNT_FILE = 0x5,
    COMMAND_IO_COMMAND = 0x6,
    COMMAND_IO_DATA = 0x7,
    COMMAND_BOOTLOADER = 0xA
};

enum class FileListingStates {
    IDLE,
    GOTO_ROOT,
    GOTO_PARENT,
    GOTO_DIRECTORY,
    GET_NEXT_CONTENTS,
    MOUNT_FILE,
    PROCESS_FILES,
};

extern pseudoatomic<FileListingStates> g_fileListingState;
extern pseudoatomic<uint32_t> g_fileArg;

struct PWMSettings {
    const unsigned int gpio;
    unsigned int sliceNum;
    pwm_config config;
    const uint16_t wrap;
    const unsigned int clkdiv;
    const bool invert;
    const uint16_t level;
};

extern mutex_t g_mechaconMutex;
extern pseudoatomic<bool> g_coreReady[2];

extern unsigned int g_soctOffset;
extern unsigned int g_subqOffset;

extern bool g_subqDelay;
extern int g_targetPlaybackSpeed;
extern unsigned int g_audioCtrlMode;

// To-do: Implement audio level/peak meters
// extern pseudoatomic<int32_t> g_audioPeak;
// extern pseudoatomic<int32_t> g_audioLevel;

[[noreturn]] void core0Entry();  // Reset, playback speed, Sled, soct, subq
[[noreturn]] void core1Entry();  // I2S, sdcard, modchip

void initHW();
void updatePlaybackSpeed();
void reset();
}  // namespace picostation