//
//  bj_server.cpp
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_server.h"
#include "bj_util.h"
#include "u2_base.h"
#include "u2_dns.h"
#include "u2_dns_dump.h"
#include "u2_mdns.h"

Bj_server::Bj_server(std::string host_name, Bj_net& net) : host_name(host_name), net(net)
{
}

void Bj_server::start()
{
    if (running)
        throw std::logic_error("already started");

    running = true;

    Bj_net_rx_handler f = std::bind(&Bj_server::rx_handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    net.set_rx_handler(f);

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

void Bj_server::rx_handler(std::span<unsigned char> data, int interface_id, const std::vector<Bj_net_address>& addresses, Bj_net_send reply)
{
    bj_util::dump_data(data);
    u2_dns_msg_dump(data.data(), data.size(), 1);
    printf("\n");

    auto interface = interfaces[interface_id];
    if (!interface || interface->addresses != addresses) {
        interface = std::make_shared<Net_interface>(host_name, addresses, services);
        interfaces[interface_id] = interface;
    }

    unsigned char out_msg[1500];
    size_t out_size = u2_mdns_process_query(&interface->database, data.data(), data.size(), out_msg, sizeof(out_msg));
    if (out_size) {
        reply(std::span(out_msg, out_size));
    }
}

void Bj_server::send_unsolicited_announcements()
{
    /*
    const struct u2_dns_record *record_list[8];
    int record_count = 0;

    for (int i = 0; i < database.domain_count; i++) {
        const struct u2_dns_domain *domain = database.domain_list[i];
        for (int j = 0; j < domain->record_count; j++) {
            const struct u2_dns_record *record = domain->record_list[j];
            if (record->type == U2_DNS_RR_TYPE_PTR && record_count < U2_ARRAY_LEN(record_list)) {
                record_list[record_count] = record;
                record_count++;
            }
        }
    }

    if (record_count) {
        unsigned char data[1440];

        size_t size = u2_mdns_generate_unsolicited_announcement(record_list, record_count, false, data, sizeof(data));
        if (size) {
            net.send(std::span(data, size));
        }
    }
    */
}

Bj_server::Net_interface::Net_interface(std::string name, const std::vector<Bj_net_address>& addresses, std::vector<Bj_service_instance> services)
    : addresses(addresses), host(name, addresses), service_collection(services)
{
    auto host_domain = host.u2_dns_view();
    auto sercice_domains = service_collection.u2_dns_view();
    domains.push_back(host_domain);
    domains.insert(domains.end(), sercice_domains.begin(), sercice_domains.end());
    database.domain_list = domains.data();
    database.domain_count = (int)domains.size();
}
