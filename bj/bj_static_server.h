//
//  bj_static_server.h
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include "bj_net.h"
#include "u2_mdns.h"

class Bj_static_server {
public:
    constexpr Bj_static_server(Bj_net& net, const struct u2_dns_database& database): net(net), database(database) {}
    void start();
    void stop();

private:
    bool running = false;
    const struct u2_dns_database& database;
    Bj_net& net;

    void rx_handler(std::span<unsigned char> data, int interface_id, const std::vector<Bj_net_address>& addresses, Bj_net_send reply);
    void send_unsolicited_announcements();
};
