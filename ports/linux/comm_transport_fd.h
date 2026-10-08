#ifndef COMM_TRANSPORT_FD_H
#define COMM_TRANSPORT_FD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "comm_link.h"

/* fd 由调用者负责打开、配置及关闭。串口需配置 raw 模式和 VMIN=1。 */
typedef struct {
    int fd;
    int is_socket;
} comm_transport_fd_t;

/* 设置 O_NONBLOCK；socket 发送使用 MSG_NOSIGNAL 避免断连导致进程退出。 */
int comm_transport_fd_init(comm_transport_fd_t *transport, int fd, int is_socket);
extern const comm_transport_ops_t comm_transport_fd_ops;

#ifdef __cplusplus
}
#endif

#endif
