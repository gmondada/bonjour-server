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
#include "bj_net_interface_database.h"
#include "u2_mdns.h"

class Bj_server {
public:
    Bj_server(std::string_view host_name, Bj_net& net);
    void set_log_level(int log_level);
    void start();
    void stop();
    void register_service(std::string_view instance_name, std::string_view service_name, uint16_t port, std::span<const char> txt_record);

private:
    struct Interface {
        std::shared_ptr<Bj_net_interface_database> database;
        Bj_net_mtu mtu;
    };

    int log_level = 0;
    std::string host_name;
    std::string domain_name;
    bool running = false;
    Bj_net& net;
    std::vector<Bj_service_instance> service_instances;

    std::map<int, Interface> interfaces; // key = interface_id

    void rx_begin_handler(int interface_id, const std::vector<Bj_net_address>& addresses, Bj_net_mtu mtu);
    void rx_data_handler(int interface_id, std::span<unsigned char> data, Bj_net_send reply);
    void rx_end_handler(int interface_id);
    void send_unsolicited_announcements();
    void send_unsolicited_announcements(Interface& interface);
};
