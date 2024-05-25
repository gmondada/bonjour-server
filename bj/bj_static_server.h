//
//  bj_static_server.h
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
#include "u2_mdns.h"

class Bj_static_server {
public:
    constexpr Bj_static_server(Bj_net& net, const struct u2_dns_database& database): net(net), database(database) {}
    void set_log_level(int log_level);
    void start();
    void stop();

private:
    int log_level = 0;
    bool running = false;
    const struct u2_dns_database& database;
    Bj_net& net;
    Bj_net_mtu mtu;

    void rx_begin_handler(int interface_id, const std::vector<Bj_net_address>& addresses, Bj_net_mtu mtu);
    void rx_data_handler(int interface_id, std::span<unsigned char> data, Bj_net_send reply);
    void send_unsolicited_announcements();
};
