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
}

Bj_net_single_apple::~Bj_net_single_apple()
{
    assert(!opened);
}

const Bj_net_executor& Bj_net_single_apple::executor() const
{
    return exec;
}

void Bj_net_single_apple::set_rx_handler(Bj_net_rx_handler rx_handler)
{
    if (opened)
        throw std::logic_error("rx handler must be set before opening");

    this->rx_handler = rx_handler;
}

void Bj_net_single_apple::open()
{
    if (opened)
        throw std::logic_error("already open");

    // create socket
    socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket < 0)
        throw Bj_net_open_error("cannot create dns-sd socket");

    // allow reusing the socket from multiple programs
    int reuse = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) != 0) {
        ::close(socket);
        throw Bj_net_open_error("cannot configure the socket to be reused");
    }

    // bind port
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_len = sizeof(saddr);
    saddr.sin_port = htons(5353);
    if (bound_address.is_any()) {
        saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        assert(bound_address.protocol == Bj_net_protocol::ipv4);
        std::copy(bound_address.ipv4.begin(), bound_address.ipv4.end(), reinterpret_cast<unsigned char*>(&saddr.sin_addr.s_addr));
    }
    if (bind(socket, (struct sockaddr *)&saddr, sizeof(saddr)) != 0) {
        ::close(socket);
        throw Bj_net_open_error("cannot bind port to socket");
    }

    // join multicast group
    if (multicast) {

        // build group address
        Bj_net_address group_addr = { 224, 0, 0, 251 };
        memset(&grp, 0, sizeof(grp));
        grp.sin_family = AF_INET;
        grp.sin_len = sizeof(grp);
        std::copy(group_addr.ipv4.begin(), group_addr.ipv4.end(), reinterpret_cast<unsigned char*>(&grp.sin_addr.s_addr));
        grp.sin_port = htons(5353);

        // join multicast group
        struct ip_mreq mreq;
        memset(&mreq, 0, sizeof(mreq));
        mreq.imr_multiaddr.s_addr = grp.sin_addr.s_addr;
        if (bound_address.is_any()) {
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        } else {
            assert(bound_address.protocol == Bj_net_protocol::ipv4);
            std::copy(bound_address.ipv4.begin(), bound_address.ipv4.end(), reinterpret_cast<unsigned char*>(&mreq.imr_interface.s_addr));
        }
        if (setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq, sizeof(mreq)) != 0) {
            ::close(socket);
            throw Bj_net_open_error("cannot join multicast group");
        }

        // define on which interface to send multicast telegrams
        if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_IF, &mreq.imr_interface, sizeof(mreq.imr_interface)) != 0) {
            ::close(socket);
            throw Bj_net_open_error("cannot set multicast output interface");
        }

        // set multicast output TTL
        unsigned char ttl = 10;
        if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) != 0) {
            ::close(socket);
            throw Bj_net_open_error("cannot set multicast ttl");
        }
    }

    // setup listening queue
    rx_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, socket, 0, exec.queue);
    dispatch_set_context(rx_source, this);
    dispatch_source_set_event_handler_f(rx_source, [](void *ctx) {
        Bj_net_single_apple* me = static_cast<Bj_net_single_apple*>(ctx);
        me->handle_rx();
    });
    dispatch_resume(rx_source);

    opened = true;
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

        ::close(me->socket);
        me->socket = -1;

        me->opened = false;

        me->close_completion();
        me->close_completion = nullptr;
    });
    dispatch_source_cancel(rx_source);
    dispatch_release(rx_source);
    rx_source = nullptr;
}

void Bj_net_single_apple::send(std::span<unsigned char> data)
{
    sendto(socket, data.data(), data.size(), 0, (struct sockaddr *)&grp, grp.sin_len);
}

void Bj_net_single_apple::reply(std::span<unsigned char> data)
{
    sendto(socket, data.data(), data.size(), 0, (struct sockaddr *)&grp, grp.sin_len);
}

void Bj_net_single_apple::handle_rx()
{
    unsigned char rx_buf[1440];
    size_t rv = recv(socket, rx_buf, sizeof(rx_buf), 0);
    if (rv > 0 && rx_handler)
        rx_handler(std::span(rx_buf, rv), 0, interface_addresses, reply_proxy);
}
