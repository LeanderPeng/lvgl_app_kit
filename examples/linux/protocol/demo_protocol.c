#include "demo_protocol.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void demo_protocol_make_query(comm_frame_t *frame)
{
    memset(frame, 0, sizeof(*frame));
    frame->id = DEMO_MESSAGE_QUERY;
}

bool demo_protocol_make_temperature(comm_frame_t *frame, int32_t temperature)
{
    memset(frame, 0, sizeof(*frame));
    frame->id = DEMO_MESSAGE_TEMPERATURE;
    int length = snprintf((char *)frame->payload, sizeof(frame->payload), "%ld", (long)temperature);
    if (length < 0 || (size_t)length >= sizeof(frame->payload)) return false;
    frame->payload_length = (size_t)length;
    return true;
}

bool demo_protocol_read_temperature(const comm_frame_t *frame, int32_t *temperature)
{
    if (!frame || !temperature || frame->id != DEMO_MESSAGE_TEMPERATURE ||
        !frame->payload_length || frame->payload_length > 11 ||
        frame->payload_length > sizeof(frame->payload)) return false;
    char number[12];
    memcpy(number, frame->payload, frame->payload_length);
    number[frame->payload_length] = '\0';
    size_t first = number[0] == '-' ? 1u : 0u;
    if (first == frame->payload_length) return false;
    for (size_t i = first; i < frame->payload_length; ++i)
        if (number[i] < '0' || number[i] > '9') return false;
    errno = 0;
    char *end;
    long parsed = strtol(number, &end, 10);
    if (errno || *end || parsed < INT32_MIN || parsed > INT32_MAX) return false;
    *temperature = (int32_t)parsed;
    return true;
}

static int encode(void *context, const comm_frame_t *frame, uint8_t *output,
                  size_t capacity, size_t *size)
{
    (void)context;
    if (!size) return -1;
    *size = 0;
    if (!frame || !output) return -1;
    if (frame->id == DEMO_MESSAGE_QUERY && frame->payload_length == 0) {
        if (capacity < 6) return -1;
        memcpy(output, "QUERY\n", 6);
        *size = 6;
        return 0;
    }
    int32_t temperature;
    if (!demo_protocol_read_temperature(frame, &temperature)) return -1;
    char encoded[18];
    int length = snprintf(encoded, sizeof(encoded), "TEMP:%ld\n", (long)temperature);
    if (length < 0 || (size_t)length >= sizeof(encoded) || (size_t)length > capacity) return -1;
    /* 线路输出是字节序列，不要求调用者为字符串终止符额外留空间。 */
    memcpy(output, encoded, (size_t)length);
    *size = (size_t)length;
    return 0;
}

static comm_decode_result_t decode(void *context, comm_ringbuffer_t *input, comm_frame_t *frame)
{
    (void)context;
    if (!input || !frame) return COMM_DECODE_ERROR;
    /* 最长线路消息为 TEMP: 加 11 位有符号整数和换行符。 */
    uint8_t line[17];
    size_t available = comm_ringbuffer_size(input);
    for (size_t i = 0; i < available && i < sizeof(line); ++i) {
        if (comm_ringbuffer_peek(input, i, &line[i], 1) != COMM_RINGBUFFER_OK)
            return COMM_DECODE_ERROR;
        if (line[i] != '\n') continue;
        comm_frame_t decoded = {0};
        if (i == 5 && memcmp(line, "QUERY", 5) == 0) {
            demo_protocol_make_query(&decoded);
        } else if (i > 5 && memcmp(line, "TEMP:", 5) == 0) {
            decoded.id = DEMO_MESSAGE_TEMPERATURE;
            decoded.payload_length = i - 5;
            if (decoded.payload_length > sizeof(decoded.payload)) return COMM_DECODE_ERROR;
            memcpy(decoded.payload, line + 5, decoded.payload_length);
            int32_t temperature;
            if (!demo_protocol_read_temperature(&decoded, &temperature)) return COMM_DECODE_ERROR;
        } else {
            return COMM_DECODE_ERROR;
        }
        if (comm_ringbuffer_discard(input, i + 1) != COMM_RINGBUFFER_OK) return COMM_DECODE_ERROR;
        *frame = decoded;
        return COMM_DECODE_READY;
    }
    return available >= sizeof(line) ? COMM_DECODE_ERROR : COMM_DECODE_MORE;
}

static void reset(void *context)
{
    /* 半帧由 link 的输入缓冲保存，示例协议没有额外状态。 */
    (void)context;
}

const comm_protocol_ops_t demo_device_protocol = {decode, encode, reset};
