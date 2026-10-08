#ifndef COMM_LINK_H
#define COMM_LINK_H

#include "comm_frame_channel.h"
#include "comm_ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMM_LINK_BUFFER_SIZE
#define COMM_LINK_BUFFER_SIZE 4096u
#endif
#if COMM_LINK_BUFFER_SIZE < 1
#error "COMM_LINK_BUFFER_SIZE must be positive"
#endif

typedef enum {
    COMM_DECODE_READY,
    COMM_DECODE_MORE,
    COMM_DECODE_ERROR
} comm_decode_result_t;

/* READY 必须从输入环中消费字节；MORE 可以丢弃噪声，保留不完整数据。 */
typedef struct {
    comm_decode_result_t (*decode)(void *, comm_ringbuffer_t *, comm_frame_t *);
    int (*encode)(void *, const comm_frame_t *, uint8_t *, size_t, size_t *);
    void (*reset)(void *);
} comm_protocol_ops_t;

/* 非阻塞接口：正数为实际字节数，0 为暂不可读写，负数为断开或错误。 */
typedef struct {
    ptrdiff_t (*read)(void *, uint8_t *, size_t);
    ptrdiff_t (*write)(void *, const uint8_t *, size_t);
} comm_transport_ops_t;

typedef enum {
    COMM_LINK_OK,
    COMM_LINK_ARGUMENT,
    COMM_LINK_PROTOCOL_ERROR,
    COMM_LINK_TRANSPORT_ERROR,
    COMM_LINK_CHANNEL_ERROR,
    COMM_LINK_RX_OVERFLOW
} comm_link_result_t;

/* 单一通信任务拥有 link；GUI 只通过具有同步能力的 RX/TX 通道交接消息。 */
typedef struct {
    comm_protocol_ops_t protocol;
    comm_transport_ops_t transport;
    void *protocol_context;
    void *transport_context;
    const comm_frame_channel_t *rx;
    const comm_frame_channel_t *tx;
    comm_ringbuffer_t input;
    uint8_t input_storage[COMM_LINK_BUFFER_SIZE];
    uint8_t output[COMM_LINK_BUFFER_SIZE];
    size_t output_size;
    size_t output_offset;
    comm_frame_t received;
    int received_pending;
    int initialized;
    comm_link_result_t fault;
} comm_link_t;

/* 所有回调必须注入；未配置协议时拒绝初始化，不默认发送任何线路格式。 */
comm_link_result_t comm_link_init(comm_link_t *link,
    const comm_protocol_ops_t *protocol, void *protocol_context,
    const comm_transport_ops_t *transport, void *transport_context,
    const comm_frame_channel_t *rx, const comm_frame_channel_t *tx);
/* 每轮最多解析 budget 帧并进行一次读写，GUI 队列满时保留待投递帧。 */
comm_link_result_t comm_link_poll(comm_link_t *link, size_t budget);
/* 先停止旧连接并排空旧队列，再重置协议和流缓冲。此函数不会清空通道。 */
void comm_link_reset(comm_link_t *link);

#ifdef __cplusplus
}
#endif
#endif
