//
//  bj_net_apple.cpp
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_net_single_apple.h"
#include <netdb.h>
#include <stdexcept>
#include <cassert>

Bj_net_single_apple::Bj_net_single_apple(const Bj_net_address& bound_address, const std::vector<Bj_net_address>& interface_addresses, bool multicast, std::optional<Bj_net_executor_apple> executor) : exec(executor.has_value() ? executor.value() : Bj_net_executor_apple())
{
    this->multicast = multicast;
    this->bound_address = bound_address;
    this->interface_addresses = interface_addresses;
    this->reply_proxy = std::bind(&Bj_net_single_apple::reply, this, std::placeholders::_1);
    this->rx_buf = std::make_unique<uint8_t[]>(rx_buf_size);
}

Bj_net_single_apple::~Bj_net_single_apple()
{
    assert(!opened);
}

const Bj_net_executor& Bj_net_single_apple::executor() const
{
    return exec;
}

void Bj_net_single_apple::set_rx_begin_handler(Bj_net_rx_begin_handler rx_begin_handler)
{
    if (opened)
        throw std::logic_error("rx handlers must be set before opening");

    this->rx_begin_handler = rx_begin_handler;
}

void Bj_net_single_apple::set_rx_data_handler(Bj_net_rx_data_handler rx_data_handler)
{
    if (opened)
        throw std::logic_error("rx handlers must be set before opening");

    this->rx_data_handler = rx_data_handler;
}

void Bj_net_single_apple::set_rx_end_handler(Bj_net_rx_end_handler rx_end_handler)
{
    if (opened)
        throw std::logic_error("rx handlers must be set before opening");

    this->rx_end_handler = rx_end_handler;
}

void Bj_net_single_apple::set_log_level(int log_level)
{
}

void Bj_net_single_apple::open()
{
    if (opened)
        throw std::logic_error("already open");

    if (multicast)
        open_multicast();
    else
        throw std::logic_error("unicast not supported yet");

    opened = true;

    if (rx_begin_handler)
        rx_begin_handler(0, interface_addresses);
}

void Bj_net_single_apple::close(std::function<void()> completion)
{
    if (!opened)
        throw std::logic_error("not open");

    if (close_completion)
        throw std::logic_error("already closing");

    close_completion = completion;
    dispatch_source_set_cancel_handler_f(rx_source, [](void *ctx) {
        Bj_net_single_apple* me = static_cast<Bj_net_single_apple*>(ctx);

        if (me->rx_end_handler)
            me->rx_end_handler(0);

        if (me->rx_socket != me->tx_socket)
            ::close(me->tx_socket);
        ::close(me->rx_socket);
        me->rx_socket = -1;
        me->tx_socket = -1;

        me->opened = false;

        auto f = me->close_completion;
        me->close_completion = nullptr;
        f();
    });
    dispatch_source_cancel(rx_source);
    dispatch_release(rx_source);
    rx_source = nullptr;
}

void Bj_net_single_apple::send(std::span<unsigned char> data)
{
    sendto(tx_socket, data.data(), data.size(), 0, (struct sockaddr *)&multicast_group, multicast_group.sin_len);
}

void Bj_net_single_apple::open_multicast()
{
    try {
        // build group address
        Bj_net_address group_addr = { 224, 0, 0, 251 };
        memset(&multicast_group, 0, sizeof(multicast_group));
        multicast_group.sin_family = AF_INET;
        multicast_group.sin_len = sizeof(multicast_group);
        std::copy(group_addr.ipv4.begin(), group_addr.ipv4.end(), reinterpret_cast<unsigned char*>(&multicast_group.sin_addr.s_addr));
        multicast_group.sin_port = htons(5353);

        // create tx socket
        tx_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (tx_socket < 0)
            throw Bj_net_open_error("cannot create dns-sd tx socket");

        // allow reusing tx socket from multiple programs
        int reuse = 1;
        if (setsockopt(tx_socket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) != 0)
            throw Bj_net_open_error("cannot configure the tx socket to be reused");

        // bind tx socket
        struct sockaddr_in tx_addr = { 0 };
        tx_addr.sin_family = AF_INET;
        tx_addr.sin_len = sizeof(tx_addr);
        tx_addr.sin_port = htons(5353);
        tx_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(tx_socket, (struct sockaddr *)&tx_addr, sizeof(tx_addr)) != 0)
            throw Bj_net_open_error("cannot bind the tx socket, errno=" + std::to_string(errno));

        // define on which interface to send multicast telegrams
        if (setsockopt(tx_socket, IPPROTO_IP, IP_MULTICAST_IF, bound_address.ipv4.data(), (int)bound_address.ipv4.size()) != 0)
            throw Bj_net_open_error("cannot set multicast output interface");

        // set multicast output TTL
        unsigned char ttl = 10;
        if (setsockopt(tx_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) != 0)
            throw Bj_net_open_error("cannot set multicast ttl");

        // create rx socket
        rx_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (rx_socket < 0)
            throw Bj_net_open_error("cannot create dns-sd rx socket");

        // allow reusing rx socket from multiple programs
        if (setsockopt(rx_socket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) != 0)
            throw Bj_net_open_error("cannot configure the rx socket to be reused");

        /*
         * We put the rx socket in non-blocking mode.
         * Actually, it's not sure that this is really needed because the disptach source
         * probably ensures that the handler is invoked only when there is some data
         * to be read.
         */
        int flags = fcntl(rx_socket, F_GETFL);
        if (flags == -1)
            throw Bj_net_open_error("cannot get rx socket flags");
        flags |= O_NONBLOCK;
        int rv = fcntl(rx_socket, F_SETFL, flags);
        if (rv == -1)
            throw Bj_net_open_error("cannot put rx socket in non-blocking mode");

        /*
         * Bind port.
         * This could sound strange, but it's correct: We do not bind the a local
         * interface, but to the multicast group IP.
         * This ensure that we only get telegrams received from the multicast group.
         * Please note few things:
         * 1. The network interface is specified later, in the IP_ADD_MEMBERSHIP
         *    parameters.
         * 2. If we put INADDR_ANY, the socket will listen to any incoming packet,
         *    not just to multicast ones.
         * 3. If we put the interface IP, bind() returns error EADDRINUSE (48),
         *    not sure why, especialy because INADDR_ANY is accepted.
         */
        struct sockaddr_in rx_addr = { 0 };
        rx_addr.sin_family = AF_INET;
        rx_addr.sin_len = sizeof(rx_addr);
        rx_addr.sin_port = htons(5353);
        std::copy(group_addr.ipv4.begin(), group_addr.ipv4.end(), reinterpret_cast<unsigned char*>(&rx_addr.sin_addr.s_addr));
        if (bind(rx_socket, (struct sockaddr *)&rx_addr, sizeof(rx_addr)) != 0)
            throw Bj_net_open_error("cannot bind port to socket, errno=" + std::to_string(errno));

        // join multicast group
        struct ip_mreq mreq = { 0 };
        mreq.imr_multiaddr.s_addr = multicast_group.sin_addr.s_addr;
        std::copy(bound_address.ipv4.begin(), bound_address.ipv4.end(), reinterpret_cast<unsigned char*>(&mreq.imr_interface.s_addr));
        if (setsockopt(rx_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq, sizeof(mreq)) != 0)
            throw Bj_net_open_error("cannot join multicast group");

        // setup listening queue
        rx_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, rx_socket, 0, exec.queue);
        dispatch_set_context(rx_source, this);
        dispatch_source_set_event_handler_f(rx_source, [](void *ctx) {
            Bj_net_single_apple* me = static_cast<Bj_net_single_apple*>(ctx);
            me->handle_rx_data();
        });
        dispatch_resume(rx_source);

    } catch (Bj_net_open_error exc) {
        if (tx_socket != -1)
            ::close(tx_socket);
        tx_socket = -1;
        if (rx_socket != -1)
            ::close(rx_socket);
        rx_socket = -1;
        throw exc;
    }
}

void Bj_net_single_apple::reply(std::span<unsigned char> data)
{
    sendto(tx_socket, data.data(), data.size(), 0, (struct sockaddr *)&multicast_group, multicast_group.sin_len);
}

void Bj_net_single_apple::handle_rx_data()
{
    /*
     * EINTR should not happen in non-blocking mode. Probably, it should not happen
     * in the rx handler either. Same for EAGAIN.
     * We just ignore them, as well as all other errors.
     */
    size_t rv = recv(rx_socket, rx_buf.get(), rx_buf_size, 0);
    if (rv > 0 && rx_data_handler)
        rx_data_handler(0, std::span(rx_buf.get(), rv), reply_proxy);
}
