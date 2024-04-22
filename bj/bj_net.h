//
//  bj_net.h
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
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

class Bj_net_executor {
public:
    virtual ~Bj_net_executor() {}
    virtual void invoke_async(std::function<void()>) const = 0;
};

using Bj_net_send = std::function<void(std::span<unsigned char> data)>;

// TODO: should we group these 3 handlers in a single delegate?
using Bj_net_rx_begin_handler = std::function<void(int interface_id, const std::vector<Bj_net_address>& addresses)>;
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
