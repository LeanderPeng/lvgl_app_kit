#include "demo_protocol.h"
#include "comm_frame_channel_pthread.h"
#include "comm_transport_fd.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

enum { QUEUE_CAPACITY = 4, RUN_TIMEOUT_MS = 2000 };

typedef struct {
    comm_frame_queue_pthread_t rx_queue;
    comm_frame_queue_pthread_t tx_queue;
    comm_frame_t rx_storage[QUEUE_CAPACITY];
    comm_frame_t tx_storage[QUEUE_CAPACITY];
    comm_frame_channel_t rx;
    comm_frame_channel_t tx;
    comm_transport_fd_t transport;
    comm_link_t link;
    bool rx_ready;
    bool tx_ready;
    bool fragment;
} endpoint_t;

/* 分段模式只限制每次读写的字节数，底层仍使用真实非阻塞 socket。 */
static ptrdiff_t read_bytes(void *context, uint8_t *data, size_t size)
{
    endpoint_t *endpoint = context;
    if (endpoint->fragment && size > 2) size = 2;
    return comm_transport_fd_ops.read(&endpoint->transport, data, size);
}

static ptrdiff_t write_bytes(void *context, const uint8_t *data, size_t size)
{
    endpoint_t *endpoint = context;
    if (endpoint->fragment && size > 2) size = 2;
    return comm_transport_fd_ops.write(&endpoint->transport, data, size);
}

static const comm_transport_ops_t transport_ops = {read_bytes, write_bytes};

static bool endpoint_init(endpoint_t *endpoint, int fd, bool fragment)
{
    endpoint->fragment = fragment;
    if (comm_frame_queue_pthread_init(&endpoint->rx_queue, endpoint->rx_storage,
                                     QUEUE_CAPACITY) != COMM_FRAME_QUEUE_PTHREAD_OK) return false;
    endpoint->rx_ready = true;
    if (comm_frame_queue_pthread_init(&endpoint->tx_queue, endpoint->tx_storage,
                                     QUEUE_CAPACITY) != COMM_FRAME_QUEUE_PTHREAD_OK) return false;
    endpoint->tx_ready = true;
    if (comm_frame_channel_pthread_bind(&endpoint->rx, &endpoint->rx_queue) != COMM_FRAME_CHANNEL_OK ||
        comm_frame_channel_pthread_bind(&endpoint->tx, &endpoint->tx_queue) != COMM_FRAME_CHANNEL_OK ||
        comm_transport_fd_init(&endpoint->transport, fd, 1) != 0) return false;

    /* 两端分别持有自己的 link，通过同一套案例协议建立连接。 */
    return comm_link_init(&endpoint->link, &demo_line_protocol, NULL,
        &transport_ops, endpoint, &endpoint->rx, &endpoint->tx) == COMM_LINK_OK;
}

static bool endpoint_destroy(endpoint_t *endpoint)
{
    bool ok = true;
    /* 本案例只在主线程调度，退出循环后即可关闭并销毁队列。 */
    if (endpoint->tx_ready) {
        if (comm_frame_queue_pthread_close(&endpoint->tx_queue) != COMM_FRAME_QUEUE_PTHREAD_OK) ok = false;
        if (comm_frame_queue_pthread_destroy(&endpoint->tx_queue) != COMM_FRAME_QUEUE_PTHREAD_OK) ok = false;
    }
    if (endpoint->rx_ready) {
        if (comm_frame_queue_pthread_close(&endpoint->rx_queue) != COMM_FRAME_QUEUE_PTHREAD_OK) ok = false;
        if (comm_frame_queue_pthread_destroy(&endpoint->rx_queue) != COMM_FRAME_QUEUE_PTHREAD_OK) ok = false;
    }
    return ok;
}

static bool send_text(endpoint_t *endpoint, const char *name, const char *text)
{
    comm_frame_t frame = {0};
    frame.payload_length = strlen(text);
    if (frame.payload_length > sizeof(frame.payload)) return false;
    memcpy(frame.payload, text, frame.payload_length);
    comm_frame_channel_result_t result = comm_frame_channel_push(&endpoint->tx, &frame, 0);
    if (result != COMM_FRAME_CHANNEL_OK) {
        fprintf(stderr, "[%s] TX queue failed: %d\n", name, result);
        return false;
    }
    printf("[%s] TX queued: %s\n", name, text);
    return true;
}

/* 返回 0 表示暂未收到，1 表示匹配成功，-1 表示通道或内容错误。 */
static int receive_text(endpoint_t *endpoint, const char *name, const char *expected)
{
    comm_frame_t frame;
    comm_frame_channel_result_t result = comm_frame_channel_pop(&endpoint->rx, &frame, 0);
    if (result == COMM_FRAME_CHANNEL_TIMEOUT) return 0;
    if (result != COMM_FRAME_CHANNEL_OK) {
        fprintf(stderr, "[%s] RX queue failed: %d\n", name, result);
        return -1;
    }
    size_t expected_size = strlen(expected);
    if (frame.id != 0 || frame.payload_length != expected_size ||
        memcmp(frame.payload, expected, expected_size) != 0) {
        fprintf(stderr, "[%s] Unexpected message\n", name);
        return -1;
    }
    printf("[%s] RX decoded: %s\n", name, expected);
    return 1;
}

static bool monotonic_ms(uint64_t *now)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return false;
    *now = (uint64_t)time.tv_sec * 1000u + (uint64_t)time.tv_nsec / 1000000u;
    return true;
}

int main(int argc, char **argv)
{
    bool fragment = argc == 2 && strcmp(argv[1], "--fragment") == 0;
    if (argc > 1 && !fragment) {
        fprintf(stderr, "Usage: %s [--fragment]\n", argv[0]);
        return 2;
    }

    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
        perror("socketpair");
        return 1;
    }
    endpoint_t host = {0};
    endpoint_t device = {0};
    int exit_code = 1;
    if (!endpoint_init(&host, sockets[0], fragment) ||
        !endpoint_init(&device, sockets[1], fragment)) {
        fprintf(stderr, "Endpoint initialization failed\n");
        goto cleanup;
    }
    printf("Local socket pair; example-only line protocol; I/O: %s\n",
           fragment ? "at most 2 bytes per call" : "normal");
    if (!send_text(&host, "host", "PING")) goto cleanup;

    uint64_t started;
    if (!monotonic_ms(&started)) goto cleanup;
    bool replied = false;
    for (;;) {
        uint64_t now;
        if (!monotonic_ms(&now)) goto cleanup;
        if (now - started >= RUN_TIMEOUT_MS) {
            fprintf(stderr, "Round trip timed out\n");
            goto cleanup;
        }
        /* 先推进字节流，再由应用从 RX 队列取消息；协议层不处理 PING/PONG 业务。 */
        comm_link_result_t result = comm_link_poll(&host.link, 4);
        if (result == COMM_LINK_OK) result = comm_link_poll(&device.link, 4);
        if (result != COMM_LINK_OK) {
            fprintf(stderr, "Communication link failed: %d\n", result);
            goto cleanup;
        }
        if (!replied) {
            int received = receive_text(&device, "device", "PING");
            if (received < 0) goto cleanup;
            if (received > 0) {
                if (!send_text(&device, "device", "PONG")) goto cleanup;
                replied = true;
            }
        }
        int received = receive_text(&host, "host", "PONG");
        if (received < 0) goto cleanup;
        if (received > 0) {
            puts("Round trip complete.");
            exit_code = 0;
            break;
        }
        struct timespec delay = {0, 1000000L};
        while (nanosleep(&delay, &delay) != 0) {
            if (errno != EINTR) goto cleanup;
        }
    }

cleanup:
    if (!endpoint_destroy(&device)) exit_code = 1;
    if (!endpoint_destroy(&host)) exit_code = 1;
    if (close(sockets[0]) != 0) exit_code = 1;
    if (close(sockets[1]) != 0) exit_code = 1;
    return exit_code;
}
