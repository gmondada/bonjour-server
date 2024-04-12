//
//  bj_net_apple.h
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include "bj_net.h"
#include "bj_net_executor_apple.h"
#include <dispatch/dispatch.h>
#include <mutex>
#include <condition_variable>
#include <netinet/in.h>
#include <optional>

class Bj_net_single_apple : public Bj_net {

public:
    Bj_net_single_apple(const Bj_net_address& bound_address, const std::vector<Bj_net_address>& interface_addresses, bool multicast, std::optional<Bj_net_executor_apple> executor = std::nullopt);
    ~Bj_net_single_apple();
    const Bj_net_executor& executor() const override;
    void set_rx_handler(Bj_net_rx_handler rx_handler) override;
    void open() override;
    void close(std::function<void()> completion) override;

    void send(std::span<unsigned char> data) override;

private:
    Bj_net_address bound_address;
    std::vector<Bj_net_address> interface_addresses;
    bool multicast;
    Bj_net_executor_apple exec;

    struct sockaddr_in grp;
    int socket = -1;
    dispatch_source_t rx_source = nullptr;

    bool opened = false;
    std::mutex opened_mutex;
    std::condition_variable opened_cv;

    Bj_net_rx_handler rx_handler;
    Bj_net_send reply_proxy;

    std::function<void()> close_completion;
    void reply(std::span<unsigned char> data);
    void handle_rx();
};
