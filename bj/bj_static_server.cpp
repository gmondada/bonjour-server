//
//  bj_static_server.cpp
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include <cassert>
#include "bj_static_server.h"
#include "bj_util.h"
#include "u2_base.h"
#include "u2_dns.h"
#include "u2_dns_dump.h"
#include "u2_mdns.h"

const size_t mdns_msg_size_max = U2_MDNS_MSG_SIZE_MAX;

void Bj_static_server::set_log_level(int log_level)
{
    this->log_level = log_level;
}

void Bj_static_server::start()
{
    if (running)
        return;

    running = true;

    Bj_net_rx_begin_handler f1 = std::bind(&Bj_static_server::rx_begin_handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    net.set_rx_begin_handler(f1);

    Bj_net_rx_data_handler f2 = std::bind(&Bj_static_server::rx_data_handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    net.set_rx_data_handler(f2);

    net.executor().invoke_async([this]() {
        net.open();
        send_unsolicited_announcements();
    });
}

void Bj_static_server::stop()
{
    if (!running)
        return;

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

void Bj_static_server::rx_begin_handler(int interface_id, const std::vector<Bj_net_address>& addresses, Bj_net_mtu mtu)
{
    assert(this->mtu.mtu == 0);
    this->mtu = mtu;
}

void Bj_static_server::rx_data_handler(int interface_id, std::span<unsigned char> data, Bj_net_send reply)
{
    if (log_level >= 1) {
        printf("### INPUT MSG\n");
        u2_dns_data_dump(data.data(), data.size(), 2);
        u2_dns_msg_dump(data.data(), data.size(), 1);
        printf("\n");
    }

    assert(mtu.mtu > 0);
    size_t msg_mtu = U2_MIN(mdns_msg_size_max, mtu.mtu);
    size_t msg_header_size = mtu.ip_header_size + mtu.udp_header_size;
    assert(msg_header_size < msg_mtu);

    size_t msg_ideal_size = msg_mtu - msg_header_size;
    size_t msg_max_size = mdns_msg_size_max - msg_header_size;

    struct u2_mdns_query_proc proc;
    u2_mdsn_query_proc_init(&proc, data.data(), data.size(), &database);
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

void Bj_static_server::send_unsolicited_announcements()
{
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

    assert(mtu.mtu > 0);
    size_t msg_size_max = mdns_msg_size_max - mtu.ip_header_size - mtu.udp_header_size;

    if (record_count) {
        unsigned char out_msg[mdns_msg_size_max];
        size_t out_size = u2_mdns_generate_unsolicited_announcement(record_list, record_count, false, out_msg, msg_size_max);
        if (out_size) {
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
