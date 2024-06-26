//
//  bj_net.h
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the “Software”),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//

#pragma once
#include <functional>
#include <span>

enum class Bj_net_protocol {
    undefined,
    ipv4,
    ipv6,
};

struct Bj_net_address {
    Bj_net_protocol protocol;
    union {
        std::array<unsigned char, 4> ipv4;
        std::array<unsigned char, 16> ipv6;
    };

    constexpr Bj_net_address() : Bj_net_address(Bj_net_protocol::undefined) {}

    constexpr Bj_net_address(Bj_net_protocol protocol) {
        this->protocol = protocol;
        ipv6.fill(0);
    }

    constexpr Bj_net_address(std::initializer_list<unsigned char> bytes) {
        switch (bytes.size()) {
            case 4:
                std::copy(bytes.begin(), bytes.end(), ipv4.begin());
                break;
            case 16:
                std::copy(bytes.begin(), bytes.end(), ipv6.begin());
                break;
            default:
                throw std::invalid_argument("unsupported address size");
        }
    }

    constexpr bool operator==(const Bj_net_address& ref) const {
        if (ref.protocol != protocol)
            return false;
        switch (protocol) {
            case Bj_net_protocol::ipv4:
                return ref.ipv4 == ipv4;
            case Bj_net_protocol::ipv6:
                return ref.ipv6 == ipv6;
            default:
                return true;
        }
    }

    std::string as_str() const;
    bool is_any() const;
    bool is_valid() const {
        return protocol == Bj_net_protocol::undefined;
    };
};

struct Bj_net_mtu {
    size_t mtu;
    size_t ip_header_size;
    size_t udp_header_size;
};

class Bj_net_executor {
public:
    virtual ~Bj_net_executor() {}
    virtual void invoke_async(std::function<void()>) const = 0;
};

using Bj_net_send = std::function<void(std::span<unsigned char> data)>;

// TODO: should we group these 3 handlers in a single delegate?
using Bj_net_rx_begin_handler = std::function<void(int interface_id, const std::vector<Bj_net_address>& addresses, Bj_net_mtu mtu)>;
using Bj_net_rx_data_handler = std::function<void(int interface_id, std::span<unsigned char> data, Bj_net_send reply)>;
using Bj_net_rx_end_handler = std::function<void(int interface_id)>;

class Bj_net {
public:
    virtual ~Bj_net() {};

    virtual const Bj_net_executor& executor() const = 0;

    virtual void set_rx_begin_handler(Bj_net_rx_begin_handler rx_begin_handler) = 0;
    virtual void set_rx_data_handler(Bj_net_rx_data_handler rx_data_handler) = 0;
    virtual void set_rx_end_handler(Bj_net_rx_end_handler rx_end_handler) = 0;

    virtual void set_log_level(int log_level) = 0;

    virtual void open() = 0;
    virtual void close(std::function<void()> completion) = 0;

    // send to multicast group
    virtual void send(std::span<unsigned char> data) = 0;
};

class Bj_net_open_error : public std::exception {
public:
    Bj_net_open_error(std::string_view message) {
        this->message = message;
    }

    const char* what() const noexcept override {
        return message.c_str();
    }

private:
    std::string message;
};
