//
//  bj_demo.cpp
//
//  Created by Gabriele Mondada on 14.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_demo.h"
#include "bj_net.h"
#include "bj_net_group_apple.h"
#include "bj_server.h"
#include "bj_util.h"

void bj_demo()
{
    Bj_net_group_apple net;
    net.set_log_level(1);

    Bj_server server("TotoHost", net);
    server.set_log_level(2);

    auto txt = bj_util::dns_name("hello=123");
    std::span<const char> txt_span(txt.data(), txt.size());
    server.register_service("Toto", "_toto._udp", 1234, std::span<char>());
    server.register_service("Toto2", "_toto._udp", 1235, std::span<char>());
    server.register_service("Toto3", "_toto-rapid._tcp", 123, txt_span);

    server.start();

//    sleep(5);
//    server.stop();

    for (;;) {
        sleep(3600);
    }
}
