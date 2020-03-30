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
#define COUNTOF(__arr) ((size_t)((sizeof(__arr))/(sizeof(__arr[0]))))

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
void logger(ProgramError program_error, const char* message_format, ...) {
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
    logger(INFO, "[%s benchmark] Time: %lu %s.\n", benchmark_name, time, time_unit);
}

#define benchmark_code(__name, __code) { struct timespec __begin = perfC(); __code struct timespec __end = perfC(); u64 __timeInNS = rs(__begin, __end); print_time(__name, __timeInNS); }
#include "name_resolution.h"
#include "http_client.h"
#include "http_server.h"
int main(int argc, char* argv[]) {
	i32 server = create_server_socket("127.0.0.1", "8080");
	while (true) {
		fd_set reads = wait_on_clients(server);
		if (FD_ISSET(server, &reads)) {
			ClientInfo* client = get_client(-1);
			client->socket = accept(server, (struct sockaddr*) &(client->address),
					&(client->address_len));
			if (!is_valid_socket(client->socket)) {
				logger(CRITICAL, "accept() failed. (%d)\n", errno);
			}
			
			logger(INFO, "New connection from %s.\n", get_client_address(client));
		}

		ClientInfo* client = g_client_list.head;
		while (client) {
			ClientInfo* next = client->next;
			if (FD_ISSET(client->socket, &reads)) {
				if (MAX_REQUEST_SIZE == client->received) {
					send_400(client);
					continue;
				}

				int r = recv(client->socket, client->request + client->received, MAX_REQUEST_SIZE - client->received, 0);

				if (r < 1) {
					logger(ERROR, "Unexpected disconnect from %s.\n", get_client_address(client));
					drop_client(client);
				} else {
					client->received += r;
					client->request[client->received] = 0;

					char* q = strstr(client->request, "\r\n\r\n");
					if (q) {
						if (strncmp("GET /", client->request, 5)) {
							send_400(client);
						} else {
							char* path = client->request + 4;
							char* end_path = strstr(path, " ");
							if (!end_path) {
								send_400(client);
							} else {
								*end_path = 0;
								serve_resource(client, path);
							}
						}
					}
				}
			}

			client = next;
		}
	}

	logger(INFO, "Closing socket...\n");
	close(server);
	logger(INFO, "Finished\n");
}
