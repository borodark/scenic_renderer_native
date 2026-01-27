/*
 * Unix domain socket transport implementation
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <errno.h>

#include "scenic_transport.h"

typedef struct {
    int fd;
    char path[256];
} unix_socket_data_t;

static int unix_connect(scenic_transport_t* t, const char* address) {
    unix_socket_data_t* data = (unix_socket_data_t*)t->impl_data;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, address, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    data->fd = fd;
    strncpy(data->path, address, sizeof(data->path) - 1);
    t->connected = true;

    return 0;
}

static void unix_disconnect(scenic_transport_t* t) {
    unix_socket_data_t* data = (unix_socket_data_t*)t->impl_data;
    if (data->fd >= 0) {
        close(data->fd);
        data->fd = -1;
    }
    t->connected = false;
}

static int unix_send(scenic_transport_t* t, const void* buf, size_t len) {
    unix_socket_data_t* data = (unix_socket_data_t*)t->impl_data;
    if (data->fd < 0) return -1;

    ssize_t sent = send(data->fd, buf, len, 0);
    return (int)sent;
}

static int unix_recv(scenic_transport_t* t, void* buf, size_t max_len, int timeout_ms) {
    unix_socket_data_t* data = (unix_socket_data_t*)t->impl_data;
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

static bool unix_data_available(scenic_transport_t* t, int timeout_ms) {
    unix_socket_data_t* data = (unix_socket_data_t*)t->impl_data;
    if (data->fd < 0) return false;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(data->fd, &fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    return select(data->fd + 1, &fds, NULL, NULL, &tv) > 0;
}

static int unix_get_fd(scenic_transport_t* t) {
    unix_socket_data_t* data = (unix_socket_data_t*)t->impl_data;
    return data->fd;
}

static void unix_destroy(scenic_transport_t* t) {
    if (t) {
        unix_disconnect(t);
        free(t->impl_data);
        free(t);
    }
}

static const scenic_transport_ops_t unix_socket_ops = {
    .connect = unix_connect,
    .disconnect = unix_disconnect,
    .send = unix_send,
    .recv = unix_recv,
    .data_available = unix_data_available,
    .get_fd = unix_get_fd,
    .destroy = unix_destroy
};

scenic_transport_t* scenic_transport_unix_socket_create(void) {
    scenic_transport_t* t = calloc(1, sizeof(scenic_transport_t));
    if (!t) return NULL;

    unix_socket_data_t* data = calloc(1, sizeof(unix_socket_data_t));
    if (!data) {
        free(t);
        return NULL;
    }

    data->fd = -1;
    t->ops = &unix_socket_ops;
    t->impl_data = data;
    t->connected = false;

    return t;
}
