//
//  bj_net_apple_group.cpp
//
//  Created by Gabriele Mondada on 05.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_net_group_apple.h"
#include <vector>
#include <ifaddrs.h>
//#include <net/if.h>
//#include <iostream>
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
        if (iface->ifa_addr != NULL && iface->ifa_name != NULL &&
//            (iface->ifa_flags & IFF_UP) != 0 &&
            interface_name == iface->ifa_name)
        {
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

void Bj_net_group_apple::set_rx_handler(Bj_net_rx_handler rx_handler)
{
    if (opened)
        throw std::logic_error("rx handler must be set before opening");

    this->rx_handler = rx_handler;
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

//    nw_path_status_t status = nw_path_get_status(path);
//    std::cout << "path " << path << ": status=" << status << "\n";

    nw_path_enumerate_interfaces(net_path.path, ^(nw_interface_t interface) {
        auto addresses = get_ip_addresses(nw_interface_get_name(interface));

//        std::cout << "  interface: index=" << (int)nw_interface_get_index(interface) << " name=" << nw_interface_get_name(interface) << "\n";
//        for (auto& address : addresses) {
//            std::cout << "   " << address.as_str() << "\n";
//        }

        int interface_id = ++interface_id_generator;

        bool first_ipv4 = true;
        bool first_ipv6 = true;

        for (auto &address : addresses) {
            if (address.protocol != Bj_net_protocol::ipv4)
                continue; // not supporting ipv6 now

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
            auto net = std::make_shared<Bj_net_single_apple>(address, addresses, multicast, exec);
            net->set_rx_handler([this, interface_id](std::span<unsigned char> data, int sublayer_interface_id, const std::vector<Bj_net_address>& addresses, Bj_net_send reply) {
                if (this->rx_handler)
                    this->rx_handler(data, interface_id, addresses, reply);
            });
            try {
                net->open();
                Net_endpoint endpoint = {
                    .multicast = true,
                    .net_path = net_path,
                    .interface_id = interface_id,
                    .interface_name = nw_interface_get_name(interface),
                    .addresses = addresses,
                    .net = net
                };
                endpoints.push_back(endpoint);
            } catch (Bj_net_open_error error) {
                // do nothing
            }
        }

        return true;
    });
}

void Bj_net_group_apple::cancel()
{
#warning TODO
    // TODO: close all sub-net
    opened = false;
    close_completion();
    close_completion = nullptr;
}
