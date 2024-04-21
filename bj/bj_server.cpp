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

const size_t udp_msg_size_max = 1472;

Bj_server::Bj_server(std::string_view host_name, Bj_net& net) : host_name(host_name), net(net)
{
    domain_name = "local";
}

void Bj_server::start()
{
    if (running)
        throw std::logic_error("already started");

    running = true;

    Bj_net_rx_begin_handler f1 = std::bind(&Bj_server::rx_begin_handler, this, std::placeholders::_1, std::placeholders::_2);
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

void Bj_server::register_service(std::string_view instance_name, std::string_view service_name, std::string_view domain_name, uint16_t port, std::span<const char> txt_record)
{
    auto service = Bj_service_instance(host_name, instance_name, service_name, domain_name, port, txt_record);
    net.executor().invoke_async([this, service]() {
        service_instances.push_back(service);
        interfaces.clear();
    });
}

void Bj_server::rx_begin_handler(int interface_id, const std::vector<Bj_net_address>& addresses)
{
    assert(!interfaces[interface_id]);
    auto interface = std::make_shared<Net_interface>(host_name, domain_name, addresses, service_instances);
    interfaces[interface_id] = interface;
    u2_dns_database_dump(&interface->database, 0);
    send_unsolicited_announcements(*interface);
}

void Bj_server::rx_data_handler(int interface_id, std::span<unsigned char> data, Bj_net_send reply)
{
    auto interface = interfaces[interface_id];
    assert(interface);

    bj_util::dump_data(data);
    u2_dns_msg_dump(data.data(), data.size(), 1);
    printf("\n");

    unsigned char out_msg[udp_msg_size_max];
    size_t out_size = u2_mdns_process_query(&interface->database, data.data(), data.size(), out_msg, sizeof(out_msg));
    if (out_size)
        reply(std::span(out_msg, out_size));
}

void Bj_server::rx_end_handler(int interface_id)
{
    interfaces.erase(interface_id);
}

void Bj_server::send_unsolicited_announcements()
{
    for (auto [_, interface] : interfaces) {
        send_unsolicited_announcements(*interface);
    }
}

void Bj_server::send_unsolicited_announcements(Net_interface& interface)
{
    std::vector<const u2_dns_record*> records;

    auto service_domains = interface.service_collection.domains_view();
    for (auto& domain : service_domains) {
        for (int i = 0; i < domain->record_count; i++) {
            const u2_dns_record *record = domain->record_list[i];
            if (record->type == U2_DNS_RR_TYPE_PTR) {
                records.push_back(record);
            }
        }
    }

    if (!records.empty()) {
        unsigned char out_msg[udp_msg_size_max];
        size_t out_size = u2_mdns_generate_unsolicited_announcement(records.data(), (int)records.size(), false, out_msg, sizeof(out_msg));
        if (out_size) {
            net.send(std::span(out_msg, out_size));
        }
    }
}

Bj_server::Net_interface::Net_interface(std::string_view host_name, std::string_view domain_name, const std::vector<Bj_net_address>& addresses, std::vector<Bj_service_instance> service_instances)
    : addresses(addresses), host(host_name, domain_name, addresses), service_collection(host_name, domain_name, service_instances)
{
    auto host_domain = host.u2_dns_view();
    auto service_domains = service_collection.domains_view();
    domains.push_back(host_domain);
    domains.insert(domains.end(), service_domains.begin(), service_domains.end());
    database.domain_list = domains.data();
    database.domain_count = (int)domains.size();
}
