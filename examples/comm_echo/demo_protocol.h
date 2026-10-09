#ifndef DEMO_PROTOCOL_H
#define DEMO_PROTOCOL_H

#include "comm_link.h"

/* 仅供本案例使用：payload 后追加换行符，内存消息 id 固定为 0。 */
extern const comm_protocol_ops_t demo_line_protocol;

#endif
