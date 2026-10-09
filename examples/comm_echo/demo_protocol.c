#include "demo_protocol.h"
#include <string.h>

static comm_decode_result_t decode_line(void *context, comm_ringbuffer_t *input,
                                       comm_frame_t *frame)
{
    (void)context;
    if (!input || !frame) return COMM_DECODE_ERROR;
    size_t available = comm_ringbuffer_size(input);
    for (size_t length = 0; length < available && length <= sizeof(frame->payload); ++length) {
        uint8_t byte;
        if (comm_ringbuffer_peek(input, length, &byte, 1) != COMM_RINGBUFFER_OK)
            return COMM_DECODE_ERROR;
        if (byte != '\n') continue;

        /* 找到完整分隔符后才消费数据；没有换行时保留半帧等待下一轮。 */
        comm_frame_t decoded = {0};
        decoded.payload_length = length;
        if (comm_ringbuffer_read(input, decoded.payload, length) != COMM_RINGBUFFER_OK ||
            comm_ringbuffer_discard(input, 1) != COMM_RINGBUFFER_OK)
            return COMM_DECODE_ERROR;
        *frame = decoded;
        return COMM_DECODE_READY;
    }
    /* 超长行是本案例的协议错误；真实项目可在这里设计丢弃和重新同步策略。 */
    return available > sizeof(frame->payload) ? COMM_DECODE_ERROR : COMM_DECODE_MORE;
}

static int encode_line(void *context, const comm_frame_t *frame,
                       uint8_t *output, size_t capacity, size_t *encoded_size)
{
    (void)context;
    if (!encoded_size) return -1;
    *encoded_size = 0;
    if (!frame || !output || frame->id != 0 ||
        frame->payload_length > sizeof(frame->payload) ||
        capacity <= frame->payload_length) return -1;
    /* 分隔符不允许出现在负载中，否则一条内存消息会被拆成多条线路消息。 */
    if (memchr(frame->payload, '\n', frame->payload_length)) return -1;
    memcpy(output, frame->payload, frame->payload_length);
    output[frame->payload_length] = '\n';
    *encoded_size = frame->payload_length + 1;
    return 0;
}

static void reset_line(void *context)
{
    /* 所有半帧都保存在框架的 RingBuffer 中，本协议没有额外状态。 */
    (void)context;
}

const comm_protocol_ops_t demo_line_protocol = {decode_line, encode_line, reset_line};
