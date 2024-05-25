//
//  bj_demo.cpp
//
//  Created by Gabriele Mondada on 14.04.2024.
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

#include "bj_demo.h"
#include "bj_net.h"
#include "bj_net_group_apple.h"
#include "bj_server.h"
#include "bj_util.h"

void bj_demo()
{
    Bj_net_group_apple net;
    net.set_log_level(1);

    Bj_server server("ServiceHost", net);
    server.set_log_level(2);

    auto txt = bj_util::dns_name("foo=123");
    std::span<const char> txt_span(txt.data(), txt.size());
    server.register_service("Service Instance 1A", "_service1._udp", 1234, std::span<char>());
    server.register_service("Service Instance 1B", "_service1._udp", 1235, std::span<char>());
    server.register_service("Service Instance 2", "_service2._tcp", 2345, txt_span);

    server.start();

//    sleep(5);
//    server.stop();

    for (;;) {
        sleep(3600);
    }
}
