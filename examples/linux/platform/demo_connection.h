#ifndef DEMO_CONNECTION_H
#define DEMO_CONNECTION_H

#include "comm_frame_channel_pthread.h"
#include "comm_transport_fd.h"
#include "comm_link.h"
#include <stdbool.h>

enum { DEMO_QUEUE_CAPACITY = 8 };
typedef struct {
    comm_frame_queue_pthread_t rx_queue;
    comm_frame_queue_pthread_t tx_queue;
    comm_frame_t rx_storage[DEMO_QUEUE_CAPACITY];
    comm_frame_t tx_storage[DEMO_QUEUE_CAPACITY];
    comm_frame_channel_t rx;
    comm_frame_channel_t tx;
    comm_transport_fd_t transport;
    comm_link_t link;
    bool rx_ready;
    bool tx_ready;
    bool fragment;
} demo_endpoint_t;

typedef struct {
    demo_endpoint_t host;
    demo_endpoint_t device;
    int sockets[2];
    bool initialized;
} demo_connection_t;

bool demo_connection_init(demo_connection_t *connection,
    const comm_protocol_ops_t *protocol, bool fragment);
bool demo_connection_poll(demo_connection_t *connection);
bool demo_connection_deinit(demo_connection_t *connection);

#endif
