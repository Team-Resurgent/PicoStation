#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "picostation.h"
#include "pseudo_atomics.h"
#include <cstdio>
#include <csignal>

void crash_handler(int sig) {
    printf("Crash detected: Signal %d\n", sig);
    while (1)
    {
        tight_loop_contents();
    }
    
}

int main() {

    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);

    vreg_set_voltage(VREG_VOLTAGE_1_15);  // Increase the VREG voltage to 1.15V for the faster clock speed
    sleep_ms(100); // Wait for the voltage to stabilize

    set_sys_clock_khz(271200, true);

    initPseudoAtomics();

    picostation::initHW();
    multicore_launch_core1(picostation::core1Entry);  // I2S Thread

    picostation::core0Entry();  // Reset, playback speed, Sled, soct, subq
    __builtin_unreachable();
}
