//
// Created by david on 25/3/20.
//

#ifndef NETWORK_TRANSPORT_H
#define NETWORK_TRANSPORT_H
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

// @param: socket_type: either TCP (SOCK_STREAM) or UDP (SOCK_DGRAM)
static int client(int argc, char **argv, i32 socketType) {
    if (argc > 3) {
        fprintf(stderr, "usage: client hostname port\n");
        return -1;
    }

    const char* const addres_name = argv[1];
    const char* const service = argv[2];
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = socketType;
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

        for (i32 socket = 1; socket <= max_socket; ++socket) {
            if (FD_ISSET(socket, &reads)) {
                if (socket == socket_listen) {
                    struct sockaddr_storage client_address;
                    socklen_t client_len = sizeof(client_address);
                    i32 socket_client = accept(socket_listen, (struct sockaddr*) &client_address, &client_len);
                    if (!is_valid_socket(socket_client)) {
                        fprintf(stderr, "accept() failed. (%d)\n", errno);
                        return -1;
                    }

                    FD_SET(socket_client, &master);
                    if (socket_client > max_socket) {
                        max_socket = socket_client;
                    }

                    char address_buffer[100];
                    getnameinfo((struct sockaddr*)&client_address, client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
                    printf("New connection from %s\n", address_buffer);
                } else {
                    char read[1024];
                    i32 bytes_received = recv(socket, read, 1024, 0);
                    if (bytes_received < 1) {
                        FD_CLR(socket, &master);
                        close(socket);
                        continue;
                    }

                    for (i32 byte = 0; byte < bytes_received; ++byte) {
                        read[byte] = toupper(read[byte]);
                    }
                    send(socket, read, bytes_received, 0);
                }
            }
        }
    }

    printf("Closing listening socket...\n");
    close(socket_listen);

    printf("Finished!");
    return 0;
}

static int udp_server() {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);

    printf("Creating socket...\n");
    i32 socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (!is_valid_socket(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        return -1;
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", errno);
        return -1;
    }
    freeaddrinfo(bind_address);

    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    char read[1024];
    i32 bytes_received = recvfrom(socket_listen, read, 1024, 0, (struct sockaddr*)&client_address, &client_len);
    printf("Received (%d bytes): %.*s\n", bytes_received, bytes_received, read);

    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo((struct sockaddr*)&client_address, client_len, address_buffer, sizeof(address_buffer), service_buffer, sizeof(service_buffer), NI_NUMERICHOST | NI_NUMERICSERV);
    printf("%s %s\n", address_buffer, service_buffer);

    close(socket_listen);
    printf("Finished\n");

    return 1;
}

static int udp_client() {
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo* peer_address;
    if (getaddrinfo("127.0.0.1", "8080", &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", errno);
        return -1;
    }

    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];

    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer, sizeof(address_buffer), service_buffer, sizeof(service_buffer), NI_NUMERICHOST | NI_NUMERICSERV);
    printf("%s %s\n", address_buffer, service_buffer);

    printf("Creating socket...\n");
    i32 socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
    if (!is_valid_socket(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        return -1;
    }

    const char* message = "Hello world";
    printf("Sending: %s\n", message);
    i32 bytes_sent = sendto(socket_peer, message, strlen(message), 0, peer_address->ai_addr, peer_address->ai_addrlen);
    printf("Sent %d bytes.\n", bytes_sent);

    freeaddrinfo(peer_address);
    close(socket_peer);
    printf("Finished!\n");
}

int multiplex_udp_server() {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* bind_address;
    getaddrinfo("127.0.0.1", "8080", &hints, &bind_address);

    printf("Creating socket...\n");
    i32 socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (!is_valid_socket(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        return -1;
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", errno);
        return -1;
    }
    freeaddrinfo(bind_address);

    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    i32 max_socket = socket_listen;

    printf("Waiting for connections...\n");

    while (true) {
        fd_set reads = master;
        if (select(max_socket + 1, &reads, 0, 0, 0) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", errno);
            return -1;
        }

        if (FD_ISSET(socket_listen, &reads)) {
            struct sockaddr_storage client_address;
            socklen_t client_len = sizeof(client_address);

            char read[1024];
            i32 bytes_received = recvfrom(socket_listen, read, 1024, 0, (struct sockaddr*)&client_address, &client_len);
            if (bytes_received < 1) {
                fprintf(stderr, "Connection closed. (%d)\n", errno);
            }

            for (i32 byte = 0; byte < bytes_received; ++byte) {
                read[byte] = toupper(read[byte]);
            }
            sendto(socket_listen, read, bytes_received, 0, (struct sockaddr*)&client_address, client_len);
        }
    }

    printf("Closing listening socket...\n");
    close(socket_listen);

    printf("Finished\n");
    return 0;
}
#endif //NETWORK_TRANSPORT_H
