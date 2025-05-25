#include "debug.h"

#include <malloc.h>

#include <cstdarg>
#include <cstdio>
#include <pico/mutex.h>

namespace picostation {

void debug::print(const char* format, ...) {

    //static mutex_t mutex;
    //static bool initialized = (mutex_init(&mutex), true);
    //(void)initialized;

    //mutex_enter_blocking(&mutex);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    //sleep_ms(100);

    //mutex_exit(&mutex);
}
}  // namespace picostation