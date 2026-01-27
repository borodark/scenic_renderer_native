/*
 * TCP transport implementation
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <errno.h>
#include <stdio.h>

#include "scenic_transport.h"

typedef struct {
    int fd;
    int listen_fd;  /* For server mode */
    char host[256];
    int port;
    bool is_server;
} tcp_data_t;

/* Client connect: address format is "host:port" */
static int tcp_connect(scenic_transport_t* t, const char* address) {
    tcp_data_t* data = (tcp_data_t*)t->impl_data;

    /* Parse host:port */
    char host[256];
    int port;
    const char* colon = strrchr(address, ':');
    if (!colon) return -1;

    size_t host_len = colon - address;
    if (host_len >= sizeof(host)) return -1;
    memcpy(host, address, host_len);
    host[host_len] = '\0';
    port = atoi(colon + 1);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return -1;
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    /* Disable Nagle's algorithm for lower latency */
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    data->fd = fd;
    strncpy(data->host, host, sizeof(data->host) - 1);
    data->port = port;
    t->connected = true;

    return 0;
}

/* Server connect: address format is "bind_addr:port" */
static int tcp_server_connect(scenic_transport_t* t, const char* address) {
    tcp_data_t* data = (tcp_data_t*)t->impl_data;

    /* Parse host:port */
    char host[256] = "0.0.0.0";
    int port;
    const char* colon = strrchr(address, ':');
    if (colon) {
        size_t host_len = colon - address;
        if (host_len < sizeof(host)) {
            memcpy(host, address, host_len);
            host[host_len] = '\0';
        }
        port = atoi(colon + 1);
    } else {
        port = atoi(address);
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return -1;

    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 1) < 0) {
        close(listen_fd);
        return -1;
    }

    data->listen_fd = listen_fd;
    strncpy(data->host, host, sizeof(data->host) - 1);
    data->port = port;

    /* Accept connection (blocking) */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        close(listen_fd);
        return -1;
    }

    int flag = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    data->fd = client_fd;
    t->connected = true;

    return 0;
}

static void tcp_disconnect(scenic_transport_t* t) {
    tcp_data_t* data = (tcp_data_t*)t->impl_data;
    if (data->fd >= 0) {
        close(data->fd);
        data->fd = -1;
    }
    if (data->listen_fd >= 0) {
        close(data->listen_fd);
        data->listen_fd = -1;
    }
    t->connected = false;
}

static int tcp_send(scenic_transport_t* t, const void* buf, size_t len) {
    tcp_data_t* data = (tcp_data_t*)t->impl_data;
    if (data->fd < 0) return -1;

    ssize_t sent = send(data->fd, buf, len, 0);
    return (int)sent;
}

static int tcp_recv(scenic_transport_t* t, void* buf, size_t max_len, int timeout_ms) {
    tcp_data_t* data = (tcp_data_t*)t->impl_data;
    if (data->fd < 0) return -1;

    if (timeout_ms > 0) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(data->fd, &fds);

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int result = select(data->fd + 1, &fds, NULL, NULL, &tv);
        if (result <= 0) return result;
    }

    ssize_t received = recv(data->fd, buf, max_len, 0);
    return (int)received;
}

static bool tcp_data_available(scenic_transport_t* t, int timeout_ms) {
    tcp_data_t* data = (tcp_data_t*)t->impl_data;
    if (data->fd < 0) return false;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(data->fd, &fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    return select(data->fd + 1, &fds, NULL, NULL, &tv) > 0;
}

static int tcp_get_fd(scenic_transport_t* t) {
    tcp_data_t* data = (tcp_data_t*)t->impl_data;
    return data->fd;
}

static void tcp_destroy(scenic_transport_t* t) {
    if (t) {
        tcp_disconnect(t);
        free(t->impl_data);
        free(t);
    }
}

static const scenic_transport_ops_t tcp_client_ops = {
    .connect = tcp_connect,
    .disconnect = tcp_disconnect,
    .send = tcp_send,
    .recv = tcp_recv,
    .data_available = tcp_data_available,
    .get_fd = tcp_get_fd,
    .destroy = tcp_destroy
};

static const scenic_transport_ops_t tcp_server_ops = {
    .connect = tcp_server_connect,
    .disconnect = tcp_disconnect,
    .send = tcp_send,
    .recv = tcp_recv,
    .data_available = tcp_data_available,
    .get_fd = tcp_get_fd,
    .destroy = tcp_destroy
};

scenic_transport_t* scenic_transport_tcp_create(void) {
    scenic_transport_t* t = calloc(1, sizeof(scenic_transport_t));
    if (!t) return NULL;

    tcp_data_t* data = calloc(1, sizeof(tcp_data_t));
    if (!data) {
        free(t);
        return NULL;
    }

    data->fd = -1;
    data->listen_fd = -1;
    data->is_server = false;
    t->ops = &tcp_client_ops;
    t->impl_data = data;
    t->connected = false;

    return t;
}

scenic_transport_t* scenic_transport_tcp_server_create(void) {
    scenic_transport_t* t = calloc(1, sizeof(scenic_transport_t));
    if (!t) return NULL;

    tcp_data_t* data = calloc(1, sizeof(tcp_data_t));
    if (!data) {
        free(t);
        return NULL;
    }

    data->fd = -1;
    data->listen_fd = -1;
    data->is_server = true;
    t->ops = &tcp_server_ops;
    t->impl_data = data;
    t->connected = false;

    return t;
}
