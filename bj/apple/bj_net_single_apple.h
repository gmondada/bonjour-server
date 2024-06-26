//
//  bj_net_apple.h
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
#include "bj_net.h"
#include "bj_net_executor_apple.h"
#include <dispatch/dispatch.h>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <netinet/in.h>
#include <optional>

class Bj_net_single_apple : public Bj_net {

public:
    Bj_net_single_apple(const Bj_net_address& bound_address, const std::vector<Bj_net_address>& interface_addresses, bool multicast, std::optional<Bj_net_executor_apple> executor = std::nullopt);
    ~Bj_net_single_apple();
    const Bj_net_executor& executor() const override;
    void set_rx_begin_handler(Bj_net_rx_begin_handler rx_handler) override;
    void set_rx_data_handler(Bj_net_rx_data_handler rx_handler) override;
    void set_rx_end_handler(Bj_net_rx_end_handler rx_handler) override;
    void set_log_level(int log_level) override;
    void open() override;
    void close(std::function<void()> completion) override;

    void send(std::span<unsigned char> data) override;

private:
    Bj_net_address bound_address;
    std::vector<Bj_net_address> interface_addresses;
    bool multicast;
    Bj_net_executor_apple exec;

    struct sockaddr_in multicast_group;
    int rx_socket = -1;
    int tx_socket = -1;
    dispatch_source_t rx_source = nullptr;
    const size_t rx_buf_size = 65536;
    std::unique_ptr<unsigned char[]> rx_buf;

    bool opened = false;
    std::mutex opened_mutex;
    std::condition_variable opened_cv;

    Bj_net_rx_begin_handler rx_begin_handler;
    Bj_net_rx_data_handler rx_data_handler;
    Bj_net_rx_end_handler rx_end_handler;
    Bj_net_send reply_proxy;

    std::function<void()> close_completion;

    void open_multicast();
    void reply(std::span<unsigned char> data);
    void handle_rx_data();
};
