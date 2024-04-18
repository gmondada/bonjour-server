//
//  bj_server.h
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include <map>
#include <string>
#include <vector>
#include "bj_net.h"
#include "bj_host.h"
#include "bj_service_collection.h"
#include "u2_mdns.h"

class Bj_server {
public:
    Bj_server(std::string_view host_name, Bj_net& net);
    void start();
    void stop();
    void register_service(std::string_view instance_name, std::string_view service_name, std::string_view domain_name, uint16_t port, std::span<const char> txt_record);

private:
    struct Net_interface {
        std::vector<Bj_net_address> addresses;
        Bj_host host;
        Bj_service_collection service_collection;
        std::vector<const u2_dns_domain*> domains;
        u2_dns_database database;

        Net_interface(std::string_view host_name, std::string_view domain_name, const std::vector<Bj_net_address>& addresses, std::vector<Bj_service_instance> service_instances);

        // WARNING: `domains` amd `database` contain pointers to other class members. Moving this object makes them dangling.
        Net_interface(const Net_interface&) = delete;
        Net_interface& operator= (const Net_interface&) = delete;
    };

    std::string host_name;
    std::string domain_name;
    bool running = false;
    Bj_net& net;
    std::vector<Bj_service_instance> service_instances;

    std::map<int, std::shared_ptr<Net_interface>> interfaces; // key = interface_id

    void rx_begin_handler(int interface_id, const std::vector<Bj_net_address>& addresses);
    void rx_data_handler(int interface_id, std::span<unsigned char> data, Bj_net_send reply);
    void rx_end_handler(int interface_id);
    void send_unsolicited_announcements();
    void send_unsolicited_announcements(Net_interface& interface);
};
