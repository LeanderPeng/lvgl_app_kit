#include "demo_connection.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static ptrdiff_t read_bytes(void *context, uint8_t *data, size_t size)
{
    demo_endpoint_t *endpoint = context;
    if (endpoint->fragment && size > 2) size = 2;
    return comm_transport_fd_ops.read(&endpoint->transport, data, size);
}

static ptrdiff_t write_bytes(void *context, const uint8_t *data, size_t size)
{
    demo_endpoint_t *endpoint = context;
    if (endpoint->fragment && size > 2) size = 2;
    return comm_transport_fd_ops.write(&endpoint->transport, data, size);
}

static const comm_transport_ops_t transport = {read_bytes, write_bytes};

static bool endpoint_init(demo_endpoint_t *endpoint, int fd,
    const comm_protocol_ops_t *protocol, bool fragment)
{
    endpoint->fragment = fragment;
    if (comm_frame_queue_pthread_init(&endpoint->rx_queue, endpoint->rx_storage,
        DEMO_QUEUE_CAPACITY) != COMM_FRAME_QUEUE_PTHREAD_OK) return false;
    endpoint->rx_ready = true;
    if (comm_frame_queue_pthread_init(&endpoint->tx_queue, endpoint->tx_storage,
        DEMO_QUEUE_CAPACITY) != COMM_FRAME_QUEUE_PTHREAD_OK) return false;
    endpoint->tx_ready = true;
    if (comm_frame_channel_pthread_bind(&endpoint->rx, &endpoint->rx_queue) != COMM_FRAME_CHANNEL_OK ||
        comm_frame_channel_pthread_bind(&endpoint->tx, &endpoint->tx_queue) != COMM_FRAME_CHANNEL_OK ||
        comm_transport_fd_init(&endpoint->transport, fd, 1) != 0) return false;
    return comm_link_init(&endpoint->link, protocol, NULL,
        &transport, endpoint, &endpoint->rx, &endpoint->tx) == COMM_LINK_OK;
}

static bool endpoint_deinit(demo_endpoint_t *endpoint)
{
    bool ok = true;
    if (endpoint->tx_ready) {
        if (comm_frame_queue_pthread_close(&endpoint->tx_queue) != COMM_FRAME_QUEUE_PTHREAD_OK) ok = false;
        if (comm_frame_queue_pthread_destroy(&endpoint->tx_queue) != COMM_FRAME_QUEUE_PTHREAD_OK) ok = false;
        endpoint->tx_ready = false;
    }
    if (endpoint->rx_ready) {
        if (comm_frame_queue_pthread_close(&endpoint->rx_queue) != COMM_FRAME_QUEUE_PTHREAD_OK) ok = false;
        if (comm_frame_queue_pthread_destroy(&endpoint->rx_queue) != COMM_FRAME_QUEUE_PTHREAD_OK) ok = false;
        endpoint->rx_ready = false;
    }
    return ok;
}

bool demo_connection_init(demo_connection_t *connection,
    const comm_protocol_ops_t *protocol, bool fragment)
{
    memset(connection, 0, sizeof(*connection));
    connection->sockets[0] = connection->sockets[1] = -1;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, connection->sockets) != 0) return false;
    if (!endpoint_init(&connection->host, connection->sockets[0], protocol, fragment) ||
        !endpoint_init(&connection->device, connection->sockets[1], protocol, fragment)) {
        demo_connection_deinit(connection);
        return false;
    }
    connection->initialized = true;
    return true;
}

bool demo_connection_poll(demo_connection_t *connection)
{
    if (!connection->initialized) return false;
    comm_link_result_t result = comm_link_poll(&connection->host.link, 4);
    if (result == COMM_LINK_OK) result = comm_link_poll(&connection->device.link, 4);
    if (result != COMM_LINK_OK) {
        fprintf(stderr, "[connection] Link failed: %d\n", result);
        return false;
    }
    return true;
}

bool demo_connection_deinit(demo_connection_t *connection)
{
    /* 单线程示例先停止轮询及 GUI 消费者，再由组合根调用本函数。 */
    bool ok = endpoint_deinit(&connection->device);
    if (!endpoint_deinit(&connection->host)) ok = false;
    for (size_t i = 0; i < 2; ++i) {
        if (connection->sockets[i] >= 0 && close(connection->sockets[i]) != 0) ok = false;
        connection->sockets[i] = -1;
    }
    connection->initialized = false;
    return ok;
}
