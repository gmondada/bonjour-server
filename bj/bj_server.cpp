//
//  bj_server.cpp
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

#include <cassert>
#include "bj_server.h"
#include "bj_util.h"
#include "u2_base.h"
#include "u2_dns.h"
#include "u2_dns_dump.h"
#include "u2_mdns.h"

const size_t mdns_msg_size_max = U2_MDNS_MSG_SIZE_MAX;

Bj_server::Bj_server(std::string_view host_name, Bj_net& net) : host_name(host_name), net(net)
{
    domain_name = "local";
}

void Bj_server::set_log_level(int log_level)
{
    this->log_level = log_level;
}

void Bj_server::start()
{
    if (running)
        throw std::logic_error("already started");

    running = true;

    Bj_net_rx_begin_handler f1 = std::bind(&Bj_server::rx_begin_handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    net.set_rx_begin_handler(f1);

    Bj_net_rx_data_handler f2 = std::bind(&Bj_server::rx_data_handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    net.set_rx_data_handler(f2);

    Bj_net_rx_end_handler f3 = std::bind(&Bj_server::rx_end_handler, this, std::placeholders::_1);
    net.set_rx_end_handler(f3);

    net.executor().invoke_async([this]() {
        net.open();
        send_unsolicited_announcements();
    });
}

void Bj_server::stop()
{
    if (!running)
        throw std::logic_error("not started");

    std::mutex mutex;
    std::condition_variable cv;
    bool stopped = false;

    net.close([&]() {
        std::unique_lock<std::mutex> lock(mutex);
        stopped = true;
        cv.notify_one();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        while (!stopped) {
            cv.wait(lock);
        }
    }

    running = false;
}

void Bj_server::register_service(std::string_view instance_name, std::string_view service_name, uint16_t port, std::span<const char> txt_record)
{
    auto service = Bj_service_instance(host_name, instance_name, service_name, domain_name, port, txt_record);
    net.executor().invoke_async([this, service]() {
        service_instances.push_back(service);
        Bj_service_collection service_collection(host_name, domain_name, service_instances);
        for (auto& [_, interface] : interfaces) {
            interface.database->set_service_collection(service_collection);
        }
    });
}

void Bj_server::rx_begin_handler(int interface_id, const std::vector<Bj_net_address>& addresses, Bj_net_mtu mtu)
{
    assert(!interfaces.contains(interface_id));
    Bj_host host(host_name, domain_name, addresses);
    Bj_service_collection service_collection(host_name, domain_name, service_instances);
    auto interface_db = std::make_shared<Bj_net_interface_database>(host, service_collection);
    Interface interface = {
        .database = interface_db,
        .mtu = mtu
    };
    interfaces[interface_id] = interface;
    if (log_level >= 1) {
        u2_dns_database_dump(interface_db->database_view(), 0);
        printf("\n");
    }
    send_unsolicited_announcements(interface);
}

void Bj_server::rx_data_handler(int interface_id, std::span<unsigned char> data, Bj_net_send reply)
{
    assert(interfaces.contains(interface_id));
    auto interface = interfaces[interface_id];

    if (log_level >= 2) {
        printf("### INPUT MSG\n");
        u2_dns_data_dump(data.data(), data.size(), 2);
        u2_dns_msg_dump(data.data(), data.size(), 1);
        printf("\n");
    }

    size_t msg_mtu = U2_MIN(mdns_msg_size_max, interface.mtu.mtu);
    size_t msg_header_size = interface.mtu.ip_header_size + interface.mtu.udp_header_size;
    assert(msg_header_size < msg_mtu);

    size_t msg_ideal_size = msg_mtu - msg_header_size;
    size_t msg_max_size = mdns_msg_size_max - msg_header_size;

    struct u2_mdns_query_proc proc;
    u2_mdsn_query_proc_init(&proc, data.data(), data.size(), interface.database->database_view());
    for (;;) {
        unsigned char out_msg[mdns_msg_size_max];
        size_t out_size = u2_mdns_query_proc_run(&proc, out_msg, msg_ideal_size, msg_max_size);
        if (out_size == 0)
            break;
        reply(std::span(out_msg, out_size));
        if (log_level >= 1) {
            printf("### OUTPUT MSG - REPLY\n");
            u2_dns_data_dump(out_msg, out_size, 2);
            u2_dns_msg_dump(out_msg, out_size, 1);
            printf("\n");
        }
    }
}

void Bj_server::rx_end_handler(int interface_id)
{
    interfaces.erase(interface_id);
}

void Bj_server::send_unsolicited_announcements()
{
    for (auto& [_, interface] : interfaces) {
        send_unsolicited_announcements(interface);
    }
}

void Bj_server::send_unsolicited_announcements(Interface& interface)
{
    std::vector<u2_mdns_response_record> records;

    auto service_domains = interface.database->get_service_collection().domains_view();
    for (auto& domain : service_domains) {
        for (int i = 0; i < domain->record_count; i++) {
            const u2_dns_record *record = domain->record_list[i];
            if (record->type == U2_DNS_RR_TYPE_PTR) {
                struct u2_mdns_response_record r = {
                    .category = U2_DNS_RR_CATEGORY_ANSWER,
                    .record = record,
                };
                records.push_back(r);
            }
        }
    }

    size_t msg_mtu = U2_MIN(mdns_msg_size_max, interface.mtu.mtu);
    size_t msg_header_size = interface.mtu.ip_header_size + interface.mtu.udp_header_size;
    assert(msg_header_size < msg_mtu);

    size_t msg_ideal_size = msg_mtu - msg_header_size;
    size_t msg_max_size = mdns_msg_size_max - msg_header_size;

    if (!records.empty()) {
        struct u2_mdns_emitter emitter;
        u2_mdns_emitter_init(&emitter, records.data(), (int)records.size(), 0, false);
        unsigned char out_msg[mdns_msg_size_max];
        for (;;) {
            size_t out_size = u2_mdns_emitter_run(&emitter, out_msg, msg_ideal_size, msg_max_size);
            if (!out_size)
                break;
            net.send(std::span(out_msg, out_size));
            if (log_level >= 1) {
                printf("### OUTPUT MSG - UNSOLICITED\n");
                u2_dns_data_dump(out_msg, out_size, 2);
                u2_dns_msg_dump(out_msg, out_size, 1);
                printf("\n");
            }
        }
    }
}
