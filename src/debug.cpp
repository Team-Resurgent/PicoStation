#include "debug.h"

#include <malloc.h>

#include <cstdarg>
#include <cstdio>
#include <pico/mutex.h>

namespace picostation {

void debug::print(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

}  // namespace picostation