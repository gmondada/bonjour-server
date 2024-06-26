//
//  bj_net_apple_group.h
//
//  Created by Gabriele Mondada on 05.04.2024.
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
#include <Network/Network.h>
#include <dispatch/dispatch.h>
#include <mutex>
#include <condition_variable>
#include "bj_net.h"
#include "bj_net_single_apple.h"

class Bj_net_group_apple : public Bj_net {
public:
    Bj_net_group_apple();
    ~Bj_net_group_apple();

    const Bj_net_executor& executor() const override;
    void set_rx_begin_handler(Bj_net_rx_begin_handler rx_begin_handler) override;
    void set_rx_data_handler(Bj_net_rx_data_handler rx_data_handler) override;
    void set_rx_end_handler(Bj_net_rx_end_handler rx_end_handler) override;
    void set_log_level(int log_level) override;
    void open() override;
    void close(std::function<void()> completion) override;
    void send(std::span<unsigned char> data) override;

private:
    struct Net_path {
        nw_path_t path;

        Net_path(nw_path_t path) : path(path) {}

        bool operator== (const Net_path& ref) const {
            return nw_path_is_equal(path, ref.path);
        }
    };

    struct Net_endpoint {
        bool multicast;
        Net_path net_path;
        int interface_id;
        std::string interface_name;
        std::vector<Bj_net_address> addresses;
        std::shared_ptr<Bj_net_single_apple> net;
    };

    int log_level = 0;
    bool opened = false;
    Bj_net_executor_apple exec;
    nw_path_monitor_t path_monitor = nullptr;
    int interface_id_generator = 0;

    Bj_net_rx_begin_handler rx_begin_handler;
    Bj_net_rx_data_handler rx_data_handler;
    Bj_net_rx_end_handler rx_end_handler;

    std::vector<Net_endpoint> endpoints;
    std::function<void()> close_completion;
    size_t close_step_count = 0;

    void update(Net_path net_path);
    void cancel();
};
