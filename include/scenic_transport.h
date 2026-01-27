/*
 * Scenic Transport Interface
 *
 * Abstract transport layer for Scenic renderer communication.
 */

#ifndef SCENIC_TRANSPORT_H
#define SCENIC_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct scenic_transport scenic_transport_t;

/* Transport operations vtable */
typedef struct {
    int (*connect)(scenic_transport_t* t, const char* address);
    void (*disconnect)(scenic_transport_t* t);
    int (*send)(scenic_transport_t* t, const void* data, size_t len);
    int (*recv)(scenic_transport_t* t, void* buf, size_t max_len, int timeout_ms);
    bool (*data_available)(scenic_transport_t* t, int timeout_ms);
    int (*get_fd)(scenic_transport_t* t);  /* -1 if not supported */
    void (*destroy)(scenic_transport_t* t);
} scenic_transport_ops_t;

/* Base transport structure */
struct scenic_transport {
    const scenic_transport_ops_t* ops;
    void* impl_data;
    bool connected;
};

/* Factory functions for built-in transports */
scenic_transport_t* scenic_transport_unix_socket_create(void);
scenic_transport_t* scenic_transport_tcp_create(void);
scenic_transport_t* scenic_transport_tcp_server_create(void);

/* Convenience macros */
#define scenic_transport_connect(t, addr)    ((t)->ops->connect((t), (addr)))
#define scenic_transport_disconnect(t)       ((t)->ops->disconnect((t)))
#define scenic_transport_send(t, d, l)       ((t)->ops->send((t), (d), (l)))
#define scenic_transport_recv(t, b, m, to)   ((t)->ops->recv((t), (b), (m), (to)))
#define scenic_transport_data_available(t, to) ((t)->ops->data_available((t), (to)))
#define scenic_transport_get_fd(t)           ((t)->ops->get_fd((t)))
#define scenic_transport_destroy(t)          ((t)->ops->destroy((t)))

#ifdef __cplusplus
}
#endif

#endif /* SCENIC_TRANSPORT_H */
