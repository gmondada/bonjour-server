//
//  bj_server.cpp
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
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
    if (log_level >= 1)
        u2_dns_database_dump(interface_db->database_view(), 0);
    send_unsolicited_announcements(interface);
}

void Bj_server::rx_data_handler(int interface_id, std::span<unsigned char> data, Bj_net_send reply)
{
    assert(interfaces.contains(interface_id));
    auto interface = interfaces[interface_id];

    if (log_level >= 2) {
        bj_util::dump_data(data);
        u2_dns_msg_dump(data.data(), data.size(), 1);
        printf("\n");
    }

    size_t msg_mtu = U2_MIN(mdns_msg_size_max, interface.mtu.mtu);
    size_t msg_header_size = interface.mtu.ip_header_size + interface.mtu.udp_header_size;
    assert(msg_header_size < msg_mtu);

    size_t msg_ideal_size = msg_mtu - msg_header_size;
    size_t msg_max_size = mdns_msg_size_max - msg_header_size;

    // TODO: manage buffer overflow; generate multiple messages; adopt msg_ideal_size
    unsigned char out_msg[mdns_msg_size_max];
    size_t out_size = u2_mdns_process_query(interface.database->database_view(), data.data(), data.size(), out_msg, msg_max_size);
    assert(out_size <= msg_max_size);
    if (out_size)
        reply(std::span(out_msg, out_size));
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
    std::vector<const u2_dns_record*> records;

    auto service_domains = interface.database->get_service_collection().domains_view();
    for (auto& domain : service_domains) {
        for (int i = 0; i < domain->record_count; i++) {
            const u2_dns_record *record = domain->record_list[i];
            if (record->type == U2_DNS_RR_TYPE_PTR)
                records.push_back(record);
        }
    }

    size_t msg_size_max = mdns_msg_size_max - interface.mtu.ip_header_size - interface.mtu.udp_header_size;

    if (!records.empty()) {
        unsigned char out_msg[mdns_msg_size_max];
        size_t out_size = u2_mdns_generate_unsolicited_announcement(records.data(), (int)records.size(), false, out_msg, msg_size_max);
        if (out_size)
            net.send(std::span(out_msg, out_size));
    }
}
