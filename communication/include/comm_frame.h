#ifndef COMM_FRAME_H
#define COMM_FRAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#ifndef COMM_FRAME_MAX_PAYLOAD_SIZE
#define COMM_FRAME_MAX_PAYLOAD_SIZE 256u
#endif
#if COMM_FRAME_MAX_PAYLOAD_SIZE < 1
#error "COMM_FRAME_MAX_PAYLOAD_SIZE must be positive"
#endif

/* 仅为任务间复制的消息容器，不代表线路格式。id 的含义由项目自行约定。 */
typedef struct {
    uint32_t id;
    size_t payload_length;
    uint8_t payload[COMM_FRAME_MAX_PAYLOAD_SIZE];
} comm_frame_t;

#ifdef __cplusplus
}
#endif

#endif
