#include "comm_link.h"
#include <string.h>

static int channel_valid(const comm_frame_channel_t *channel)
{
    return channel && channel->context && channel->push && channel->pop && channel->close;
}

comm_link_result_t comm_link_init(comm_link_t *link,
    const comm_protocol_ops_t *protocol, void *protocol_context,
    const comm_transport_ops_t *transport, void *transport_context,
    const comm_frame_channel_t *rx, const comm_frame_channel_t *tx)
{
    if (!link || !protocol || !protocol->decode || !protocol->encode ||
        !protocol->reset || !transport || !transport->read || !transport->write ||
        !channel_valid(rx) || !channel_valid(tx)) return COMM_LINK_ARGUMENT;
    memset(link, 0, sizeof(*link));
    link->protocol = *protocol;
    link->transport = *transport;
    link->protocol_context = protocol_context;
    link->transport_context = transport_context;
    link->rx = rx;
    link->tx = tx;
    if (comm_ringbuffer_init(&link->input, link->input_storage,
                            sizeof(link->input_storage)) != COMM_RINGBUFFER_OK)
        return COMM_LINK_ARGUMENT;
    link->initialized = 1;
    protocol->reset(protocol_context);
    return COMM_LINK_OK;
}

void comm_link_reset(comm_link_t *link)
{
    if (!link || !link->initialized) return;
    comm_ringbuffer_reset(&link->input);
    link->output_size = link->output_offset = 0;
    link->received_pending = 0;
    link->fault = COMM_LINK_OK;
    link->protocol.reset(link->protocol_context);
}

static comm_link_result_t receive_frames(comm_link_t *link, size_t budget)
{
    while (budget-- > 0) {
        if (!link->received_pending) {
            size_t before = comm_ringbuffer_size(&link->input);
            comm_decode_result_t decoded;
            if (!before) break;
            memset(&link->received, 0, sizeof(link->received));
            decoded = link->protocol.decode(link->protocol_context,
                                             &link->input, &link->received);
            if (decoded == COMM_DECODE_MORE) {
                if (!comm_ringbuffer_free_space(&link->input))
                    return COMM_LINK_RX_OVERFLOW;
                break;
            }
            if (decoded != COMM_DECODE_READY ||
                comm_ringbuffer_size(&link->input) >= before ||
                link->received.payload_length > sizeof(link->received.payload))
                return COMM_LINK_PROTOCOL_ERROR;
            link->received_pending = 1;
        }
        comm_frame_channel_result_t result =
            comm_frame_channel_push(link->rx, &link->received, 0);
        if (result == COMM_FRAME_CHANNEL_TIMEOUT) break;
        if (result != COMM_FRAME_CHANNEL_OK) return COMM_LINK_CHANNEL_ERROR;
        link->received_pending = 0;
    }
    return COMM_LINK_OK;
}

comm_link_result_t comm_link_poll(comm_link_t *link, size_t budget)
{
    uint8_t bytes[512];
    comm_frame_t frame;
    ptrdiff_t count;
    size_t available;
    comm_link_result_t result;
    comm_frame_channel_result_t queued;
    if (!link || !link->initialized || !budget) return COMM_LINK_ARGUMENT;
    if (link->fault != COMM_LINK_OK) return link->fault;

    /* 先处理积压，再读新数据，队列背压不会静默丢帧。 */
    result = receive_frames(link, budget);
    if (result != COMM_LINK_OK) return link->fault = result;
    available = comm_ringbuffer_free_space(&link->input);
    if (available > sizeof(bytes)) available = sizeof(bytes);
    if (available && !link->received_pending) {
        count = link->transport.read(link->transport_context, bytes, available);
        if (count < 0 || (size_t)count > available)
            return link->fault = COMM_LINK_TRANSPORT_ERROR;
        if (count) comm_ringbuffer_write(&link->input, bytes, (size_t)count);
    }

    if (link->output_offset == link->output_size) {
        queued = comm_frame_channel_pop(link->tx, &frame, 0);
        if (queued != COMM_FRAME_CHANNEL_OK && queued != COMM_FRAME_CHANNEL_TIMEOUT)
            return link->fault = COMM_LINK_CHANNEL_ERROR;
        if (queued == COMM_FRAME_CHANNEL_OK) {
            link->output_size = link->output_offset = 0;
            if (frame.payload_length > sizeof(frame.payload) ||
                link->protocol.encode(link->protocol_context, &frame, link->output,
                    sizeof(link->output), &link->output_size) != 0 ||
                !link->output_size || link->output_size > sizeof(link->output))
                return link->fault = COMM_LINK_PROTOCOL_ERROR;
        }
    }
    if (link->output_offset < link->output_size) {
        available = link->output_size - link->output_offset;
        count = link->transport.write(link->transport_context,
                                      link->output + link->output_offset, available);
        if (count < 0 || (size_t)count > available)
            return link->fault = COMM_LINK_TRANSPORT_ERROR;
        link->output_offset += (size_t)count;
    }
    return COMM_LINK_OK;
}
