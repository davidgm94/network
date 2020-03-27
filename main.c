#include <stdint.h>
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <stdarg.h>

#define is_valid_socket(s) (s >= 0)

static inline struct timespec perfC(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts;
}

static inline struct timespec res(void) {
    struct timespec ts;
    clock_getres(CLOCK_MONOTONIC_RAW, &ts);
    return ts;
}

#define S_TO_NS(s) (s * 1000 * 1000 * 1000)
static inline u64 rs(struct timespec begin, struct timespec end) {
    u64 ns = (u64)end.tv_nsec - (u64)begin.tv_nsec;
    u64 s = (u64)end.tv_sec - (u64)begin.tv_sec;
    u64 result = S_TO_NS(s) + ns;
    return result;
}

typedef enum {
    INFO,
    ERROR,
    CRITICAL,
} ProgramError;

#define LOGS 1
void log(ProgramError program_error, const char* message_format, ...) {
#if LOGS
    const char* predefined_format = "[%s";
    FILE* stream = program_error == INFO ? stdout : stderr;

    va_list arg_list;
    va_start(arg_list, message_format);
    vfprintf(stream, message_format, arg_list);
    va_end(arg_list);

    if (program_error == CRITICAL)
        exit(-1);
#endif
}

void print_time(const char* benchmark_name, u64 time_in_ns) {
    u64 time;
    char* time_unit;
    char s[] = "s";
    char ms[] = "ms";
    char us[] = "us";
    char ns[] = "ns";

    if (time_in_ns > 1000 * 1000 * 1000) {
        time_unit = s;
        time = (u64)(time_in_ns/(1000.0 * 1000.0 * 1000.0));
    } else if (time_in_ns > 1000 * 1000) {
        time_unit = ms;
        time = (u64)(time_in_ns/(1000.0 * 1000.0));
    } else if (time_in_ns > 1000) {
        time_unit = us;
        time = (u64)(time_in_ns/1000.0);
    } else {
        time_unit = ns;
        time = time_in_ns;
    }
    log(INFO, "[%s benchmark] Time: %lu %s.\n", benchmark_name, time, time_unit);
}

#define benchmark_code(__name, __code) { struct timespec __begin = perfC(); __code struct timespec __end = perfC(); u64 __timeInNS = rs(__begin, __end); print_time(__name, __timeInNS); }
#include "name_resolution.h"
#include "http_client.h"
#include "http_server.h"
int main(int argc, char* argv[]) {
    benchmark_code("HTTP_CLIENT",
                   http_client(argc, argv);
            )
}
