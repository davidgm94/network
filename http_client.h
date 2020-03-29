#pragma once

#define TIMEOUT 5.0
typedef struct {
    char hostname[1024];
    char path[1024];
    char port[8];
} URL;

URL parse_url(const char* raw_url) {
    struct timespec begin = perfC();
    char urlBuffer[2048];
    strcpy(urlBuffer, raw_url);
    char* p = strstr(urlBuffer, "://");
    char* protocol = NULL;
    if (p) {
        protocol = urlBuffer;
        *p = 0;
        p += 3;
    } else {
        p = urlBuffer;
    }

    if (protocol && strcmp(protocol, "http")) {
        logger(CRITICAL, "Unknown protocol '%s'. Only 'http' is supported.\n", protocol);
    }

    char* hostname = p;

    while (*p && *p != ':' && *p != '/' && *p != '#') ++p;

    char* port = "80";
    if (*p == ':') {
        *p++ = 0;
        port = p;
    }

    while (*p && *p != '/' && *p != '#') ++p;

    char* path = p;
    if (*p == '/') {
        path = p + 1;
    }
    *p = 0;

    while (*p && *p != '#') ++p;
    if (*p == '#') *p = 0;
    struct timespec end = perfC();
    u64 timeInNS = rs(begin, end);
    printf("Time: %lu ns.\n", timeInNS);
    printf("hostname: %s\n", hostname);
    printf("port: %s\n", port);
    printf("path: %s\n", path);

    URL url;
    strcpy(url.hostname, hostname);
    strcpy(url.path, path);
    strcpy(url.port, port);
    return url;
}

typedef enum { GET, POST} HTTPMethod;
void send_request(i32 socket, const URL* const url, HTTPMethod method, const char* content_type, i32 content_length, const char* body) {
    char* method_name;
    const char* post = "POST";
    const char* get = "GET";
    if (method == GET) {
        method_name = get;
    } else if (method == POST) {
        method_name = post;
    }
    char buffer[2048];
    if (method == GET) {
        sprintf(buffer,
                "GET /%s HTTP/1.1\r\n"
                "Host: %s:%s\r\n"
                "User-Agent: honpwc web_get 1.0\r\n"
                "Connection: close\r\n"
                "\r\n",
                url->path, url->hostname, url->port);
    } else if (method == POST) {
        sprintf(buffer,
                "POST /%s HTTP/1.1\r\n"
                "Host: %s:%s\r\n"
                "User-Agent: honpwc web_get 1.0\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                // Body
                "%s",
                url->path, url->hostname, url->port, content_type, content_length, body);
    }
    send(socket, buffer, strlen(buffer), 0);
    printf("Sent headers:\n%s", buffer);
}

i32 connect_to_host(char* hostname, char* port) {
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* peer_address;
    if (getaddrinfo(hostname, port, &hints, &peer_address)) {
        logger(CRITICAL, "getaddrinfo() failed. (%d)\n", errno);
    }

    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];

    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer, sizeof(address_buffer), service_buffer, sizeof(service_buffer), NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);

    printf("Creating socket...\n");
    i32 server = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
    if (!is_valid_socket(server)) {
        logger(CRITICAL, "socket() failed. (%d)\n", errno);
    }

    printf("Connecting...\n");
    if (connect(server, peer_address->ai_addr, peer_address->ai_addrlen)) {
        logger(CRITICAL, "connect() failed. (%d)\n", errno);
        exit(-1);
    }
    freeaddrinfo(peer_address);

    logger(INFO, "Connected...\n");

    return server;
}

void http_client(int argc, char** argv) {
    if (argc < 2) {
        logger(CRITICAL, "usage: http_client url\n");
    }

    const char* raw_url = argv[1];
    URL url = parse_url(raw_url);
    i32 server = connect_to_host(url.hostname, url.port);
    send_request(server, &url, GET, NULL, NULL, NULL);

    const clock_t start_time = clock();
#define RESPONSE_SIZE 8192
    char response[RESPONSE_SIZE];
    char* p = response, *q;
    char* end = response + RESPONSE_SIZE;
    char* body = NULL;

    enum { length, chunked, connection };
    i32 encoding = 0;
    i32 remaining = 0;

    while (true) {
        if ((clock() - start_time) / CLOCKS_PER_SEC > TIMEOUT) {
            logger(CRITICAL, "Timeout after %.02f seconds\n", TIMEOUT);
        }

        if (p == end) {
            logger(CRITICAL, "Out of buffer space\n");
        }

        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(server, &reads);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        if (select(server + 1, &reads, 0, 0, &timeout) < 0) {
            logger(CRITICAL, "select() failed. (%d)\n", errno);
        }

        if (FD_ISSET(server, &reads)) {
            i32 bytes_received = recv(server, p, end - p, 0);
            if (bytes_received < 1) {
                if (encoding == connection && body) {
                    logger(INFO, "%.*s", (i32) (end - body), body);
                }
                logger(INFO, "\nConnection closed by peer.\n");
                break;
            }

            p += bytes_received;
            *p = 0;
        }

        if (!body && (body = strstr(response, "\r\n\r\n"))) {
            *body = 0;
            body += 4;
        }

        logger(INFO, "Received headers:\n%s\n", response);

        q = strstr(response, "\nContent-Length: ");
        if (q) {
            encoding = length;
            q = strchr(q, ' ');
            q += 1;
            remaining = strtol(q, NULL, 10);
        } else {
            q = strstr(response, "\nTransfer-Encoding: chunked");
            if (q) {
                encoding = chunked;
                remaining = 0;
            } else {
                encoding = connection;
            }
        }
        logger(INFO, "\nReceived body:\n");

        if (body) {
            if (encoding == length) {
                if (p - body >= remaining) {
                    logger(INFO, "%.*s", remaining, body);
                    break;
                }
            } else if (encoding == chunked) {
                do {
                    if (remaining == 0) {
                        if ((q = strstr(body, "\r\n"))) {
                            remaining = strtol(body, 0, 16);
                            if (!remaining) goto finish;
                            body = q + 2;
                        } else break;
                    }
                    if (remaining && p - body >= remaining) {
                        logger(INFO, "%.*s", remaining, body);
                        body += remaining + 2;
                        remaining = 0;
                    }
                } while (!remaining);
            }
        }
    }

    finish:
    logger(INFO, "\nClosing socket...\n");
    close(server);
    logger(INFO, "Finished\n");
}
