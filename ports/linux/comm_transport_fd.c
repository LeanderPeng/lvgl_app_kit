#include "comm_transport_fd.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

int comm_transport_fd_init(comm_transport_fd_t *transport, int fd, int is_socket)
{
    int flags;
    if (!transport || fd < 0) return -1;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return -1;
    transport->fd = fd;
    transport->is_socket = is_socket;
    return 0;
}

static ptrdiff_t read_bytes(void *context, uint8_t *data, size_t size)
{
    comm_transport_fd_t *transport = context;
    ssize_t result = read(transport->fd, data, size);
    if (result > 0) return result;
    if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return 0;
    return -1;
}

static ptrdiff_t write_bytes(void *context, const uint8_t *data, size_t size)
{
    comm_transport_fd_t *transport = context;
    ssize_t result = transport->is_socket ?
        send(transport->fd, data, size, MSG_NOSIGNAL) : write(transport->fd, data, size);
    if (result >= 0) return result;
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
    return -1;
}

const comm_transport_ops_t comm_transport_fd_ops = {read_bytes, write_bytes};
