//
//  bj_net_apple_group.cpp
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

#include "bj_net_group_apple.h"
#include <vector>
#include <ifaddrs.h>
#include <iostream>
#include <netinet/in.h>
#include <cstring>

static std::vector<Bj_net_address> get_ip_addresses(std::string_view interface_name)
{
    std::vector<Bj_net_address> list;

    struct ifaddrs *head = NULL;
    int rv = getifaddrs(&head);
    if (rv != 0)
        return list;

    struct ifaddrs *iface = head;
    while (iface) {
        if (iface->ifa_addr != NULL && iface->ifa_name != NULL && interface_name == iface->ifa_name) {
            switch (iface->ifa_addr->sa_family) {
                case AF_INET: {
                    struct sockaddr_in ipv4_addr = {};
                    std::memcpy(&ipv4_addr, iface->ifa_addr, iface->ifa_addr->sa_len);
                    std::span addr_bytes((unsigned char*)&ipv4_addr.sin_addr, sizeof(ipv4_addr.sin_addr));

                    Bj_net_address addr;
                    addr.protocol = Bj_net_protocol::ipv4;
                    assert(addr_bytes.size() ==  addr.ipv4.size());
                    std::copy(addr_bytes.begin(), addr_bytes.end(), addr.ipv4.begin());
                    list.push_back(addr);
                    break;
                }
                case AF_INET6: {
                    struct sockaddr_in6 ipv6_addr = {};
                    std::memcpy(&ipv6_addr, iface->ifa_addr, iface->ifa_addr->sa_len);
                    std::span addr_bytes((unsigned char*)&ipv6_addr.sin6_addr, sizeof(ipv6_addr.sin6_addr));

                    Bj_net_address addr;
                    addr.protocol = Bj_net_protocol::ipv6;
                    assert(addr_bytes.size() ==  addr.ipv6.size());
                    std::copy(addr_bytes.begin(), addr_bytes.end(), addr.ipv6.begin());
                    list.push_back(addr);
                    break;
                }
            }
        }
        iface = iface->ifa_next;
    }

    freeifaddrs(head);

    return list;
}

Bj_net_group_apple::Bj_net_group_apple()
{
    path_monitor = nw_path_monitor_create();
    nw_path_monitor_set_queue(path_monitor, exec.queue);
    nw_path_monitor_set_update_handler(path_monitor, ^(nw_path_t path) {
        update(Net_path(path));
    });
    nw_path_monitor_set_cancel_handler(path_monitor, ^(void) {
        cancel();
    });
}

Bj_net_group_apple::~Bj_net_group_apple()
{
    assert(!opened);
    nw_release(path_monitor);
    path_monitor = nullptr;
}

const Bj_net_executor& Bj_net_group_apple::executor() const
{
    return exec;
}

void Bj_net_group_apple::set_rx_begin_handler(Bj_net_rx_begin_handler rx_begin_handler)
{
    if (opened)
        throw std::logic_error("rx handlers must be set before opening");

    this->rx_begin_handler = rx_begin_handler;
}

void Bj_net_group_apple::set_rx_data_handler(Bj_net_rx_data_handler rx_data_handler)
{
    if (opened)
        throw std::logic_error("rx handlers must be set before opening");

    this->rx_data_handler = rx_data_handler;
}

void Bj_net_group_apple::set_rx_end_handler(Bj_net_rx_end_handler rx_end_handler)
{
    if (opened)
        throw std::logic_error("rx handlers must be set before opening");

    this->rx_end_handler = rx_end_handler;
}

void Bj_net_group_apple::set_log_level(int log_level)
{
    this->log_level = log_level;
}

void Bj_net_group_apple::open()
{
    if (opened)
        throw std::logic_error("already open");

    opened = true;
    nw_path_monitor_start(path_monitor);
}

void Bj_net_group_apple::close(std::function<void()> completion)
{
    if (!opened)
        throw std::logic_error("not open");

    if (close_completion)
        throw std::logic_error("already closing");

    close_completion = completion;

    nw_path_monitor_cancel(path_monitor);
}

void Bj_net_group_apple::send(std::span<unsigned char> data)
{
    for (auto& endpoint : endpoints) {
        if (endpoint.multicast) {
            endpoint.net->send(data);
        }
    }
}

void Bj_net_group_apple::update(Net_path net_path)
{
    if (log_level >= 1) {
        nw_path_status_t status = nw_path_get_status(net_path.path);
        std::cout << "path: ptr=" << net_path.path << " status=" << status << "\n";
    }

    for (int i = 0; i < endpoints.size(); ++i) {
        if (endpoints[i].net_path == net_path) {
            std::shared_ptr<Bj_net_single_apple> net = endpoints[i].net;
            endpoints[i].net->close([net]() mutable {
                net.reset();
            });
            endpoints.erase(endpoints.begin() + i);
            i--;
        }
    }

    nw_path_enumerate_interfaces(net_path.path, ^(nw_interface_t interface) {
        auto addresses = get_ip_addresses(nw_interface_get_name(interface));

        if (log_level >= 1) {
            std::cout << "  interface: index=" << (int)nw_interface_get_index(interface) << " name=" << nw_interface_get_name(interface) << "\n";
            for (auto& address : addresses) {
                std::cout << "   " << address.as_str() << "\n";
            }
        }

        int interface_id = ++interface_id_generator;

        bool first_ipv4 = true;
        bool first_ipv6 = true;

        for (auto &address : addresses) {
            // not supporting ipv6 yet
            if (address.protocol != Bj_net_protocol::ipv4)
                continue;

            // we only subscribe one address per protocol to the multicast group
            bool multicast = false;
            if (address.protocol == Bj_net_protocol::ipv4 && first_ipv4) {
                first_ipv4 = false;
                multicast = true;
            }
            if (address.protocol == Bj_net_protocol::ipv6 && first_ipv6) {
                first_ipv6 = false;
                multicast = true;
            }

            // not supporting queries sent directly to this host yet
            if (!multicast)
                continue;

            auto net = std::make_shared<Bj_net_single_apple>(address, addresses, multicast, exec);
            net->set_rx_begin_handler([this, interface_id](int sublayer_interface_id, const std::vector<Bj_net_address>& addresses, Bj_net_mtu mtu) {
                if (this->rx_begin_handler)
                    this->rx_begin_handler(interface_id, addresses, mtu);
            });
            net->set_rx_data_handler([this, interface_id](int sublayer_interface_id, std::span<unsigned char> data, Bj_net_send reply) {
                if (this->rx_data_handler)
                    this->rx_data_handler(interface_id, data, reply);
            });
            net->set_rx_end_handler([this, interface_id](int sublayer_interface_id) {
                if (this->rx_end_handler)
                    this->rx_end_handler(interface_id);
            });

            Net_endpoint endpoint = {
                .multicast = true,
                .net_path = net_path,
                .interface_id = interface_id,
                .interface_name = nw_interface_get_name(interface),
                .addresses = addresses,
                .net = net
            };
            endpoints.push_back(endpoint);

            try {
                net->open();
            } catch (Bj_net_open_error error) {
                // error while opening - this interface cannot be used
                std::cout << address.as_str() << " cannot be used (" << error.what() << ")\n";

                endpoints.pop_back();
            }
        }

        return true;
    });
}

void Bj_net_group_apple::cancel()
{
    close_step_count = endpoints.size();
    for (auto& endpoint : endpoints) {
        endpoint.net->close([this]() {
            close_step_count--;
            if (close_step_count == 0) {
                endpoints.clear();
                opened = false;
                auto f = close_completion;
                close_completion = nullptr;
                f();
                return;
            }
        });
    }
}
