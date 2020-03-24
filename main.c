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

#define is_valid_socket(s) (s >= 0)

static int showAddresses(void)
{
    struct ifaddrs* addresses;
    if (getifaddrs(&addresses) == -1) {
        printf("getifaddrs call failed!");
        return -1;
    } else {
        printf("Succeeded!\n");
    }

    struct ifaddrs* address = addresses;
    while (address) {
        int family = address->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            printf("%s\t", address->ifa_name);
            printf("%s\t", family == AF_INET ? "IPv4" : "IPv6");
            const int family_size = family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

            char ap[100];
            getnameinfo(address->ifa_addr, family_size, ap, sizeof(ap), NULL, 0, NI_NUMERICHOST);
            printf("\t%s\n", ap);
        }
        address = address->ifa_next;
    }

    return 1;
}

static int first() {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);

    printf("Creating socket...\n");
    int socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);

    if (!is_valid_socket(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        return -1;
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed (%d)\n", errno);
        return -1;
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed (%d)\n", errno);
        return -1;
    }

    printf("Waiting for connection...\n");
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    int socket_client = accept(socket_listen, (struct sockaddr*) &client_address, &client_len);

    if (!is_valid_socket(socket_client)) {
        fprintf(stderr, "accept() failed (%d)\n", errno);
        return -1;
    }

    printf("Client is connected... ");
    char address_buffer[100];
    getnameinfo(( struct sockaddr*) &client_address, client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
    printf("%s\n", address_buffer);

    printf("Reading request...\n");
    char request[1024];
    int bytes_received = recv(socket_client, request, 1024, 0);
    printf("Received %d bytes.\n", bytes_received);
    printf("%.*s", bytes_received, request);

    printf("Sending response...\n");
    const char* response = "HTTP/1.1 200 OK\r\n"
                           "Connection: close\r\n"
                           "Content-Type: text/plain\r\n\r\n"
                           "Local time is: ";
    int bytes_sent = send(socket_client, response, strlen(response), 0);
    printf("Sent %d of %d bytes.\n", bytes_sent, (int)strlen(response));

    time_t timer;
    time(&timer);
    char* time_msg = ctime(&timer);
    bytes_sent = send(socket_client, time_msg, strlen(time_msg), 0);
    printf("Sent %d bytes of %d bytes.\n", bytes_sent, (int)strlen(time_msg));

    printf("Closing connection...\n");
    close(socket_client);
    printf("Closing listening socket...\n");
    close(socket_listen);

    printf("Finished!\n");

    return 1;
}

static inline struct timespec perfC(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    printf("[PERFORMANCE] %ld s, %ld ns\n", ts.tv_sec, ts.tv_nsec);
    return ts;
}

static inline struct timespec res(void) {
    struct timespec ts;
    clock_getres(CLOCK_MONOTONIC_RAW, &ts);
    printf("[RESOLUTION] %ld s, %ld ns\n", ts.tv_sec, ts.tv_nsec);
    return ts;
}

#define S_TO_NS(s) (s * 1000 * 1000 * 1000)
static inline u64 rs(struct timespec begin, struct timespec end) {
    u64 ns = (u64)end.tv_nsec - (u64)begin.tv_nsec;
    u64 s = (u64)end.tv_sec - (u64)begin.tv_sec;
    u64 result = S_TO_NS(s) + ns;
    return result;
}

static int tcp_client(int argc, char* argv[]) {
    if (argc > 3) {
        fprintf(stderr, "usage: tcp_client hostname port\n");
        return -1;
    }

    const char* const addres_name = argv[1];
    const char* const service = argv[2];
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* peer_address;
    if (getaddrinfo(addres_name, service, &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", errno);
        return -1;
    }

    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];

    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer, sizeof(address_buffer), service_buffer, sizeof(service_buffer), NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);

    printf("Creating socket...\n");
    i32 socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
    if (!is_valid_socket(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        return -1;
    }

    printf("Connecting...\n");
    if (connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", errno);
        return -1;
    }
    freeaddrinfo(peer_address);

    printf("Connected.\n");
    printf("To send data, enter text followed by enter\n");

    while (true) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if (select(socket_peer + 1, &reads, NULL, NULL, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", errno);
            return -1;
        }

        if (FD_ISSET(socket_peer, &reads)) {
            char read[4046];
            i32 bytes_received = recv(socket_peer, (void*)read, (size_t)4096, 0);

            if (bytes_received < 1) {
                printf("Connection closed by peer\n");
                break;
            }
            printf("Received (%d bytes): %.*s", bytes_received, bytes_received, read);
        }

        if (FD_ISSET(0, &reads)) {
            char read[4096];
            if (!fgets(read, 4096, stdin)) break;
            printf("Sending. %s", read);
            int bytes_sent = send(socket_peer, read, strlen(read), 0);
            printf("Sent %d bytes.\n", bytes_sent);
        }
    }

    printf("Closing socket...\n");
    close(socket_peer);
    printf("Finished!\n");
    return 1;
}

static i32 tcp_server(void) {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);

    printf("Creating socket...\n");
    i32 socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (!is_valid_socket(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        return -1;
    }
    freeaddrinfo(bind_address);

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", errno);
        return -1;
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    i32 max_socket = socket_listen;

    printf("Waiting for connections...\n");

    while (true) {
        fd_set reads;
        reads = master;
        if (select(max_socket + 1, &reads, NULL, NULL, NULL) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", errno);
            return -1;
        }

       for (i32 socket; socket <= max_socket; ++socket) {
           if (FD_ISSET(socket, &reads)) {
               if (socket == socket_listen) {
                   struct sockaddr_storage client_address;
               }
           }
       }
    }
};
int main(int argc, char* argv[]) {
//    tcp_client(argc, argv);
//    res();
//    struct timespec begin = perfC();
//    showAddresses();
//    struct timespec middle = perfC();
//    first();
//    struct timespec end = perfC();
//
//    printf("First: %lu\n", rs(begin, middle));
//    printf("Second: %lu\n", rs(middle, end));
}
