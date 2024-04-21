//
//  bj_net_interface_database.h
//
//  Created by Gabriele Mondada on 21.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once

#include "bj_host.h"
#include "bj_service_collection.h"

class Bj_net_interface_database {
public:
    Bj_net_interface_database(const Bj_host& host, const Bj_service_collection& service_collection);

    // `domains` amd `database` contain pointers to other class members; moving this object makes them dangling
    Bj_net_interface_database(const Bj_net_interface_database&) = delete;
    Bj_net_interface_database& operator= (const Bj_net_interface_database&) = delete;

    Bj_service_collection& get_service_collection();
    void set_service_collection(const Bj_service_collection& service_collection);

    const u2_dns_database* database_view();

private:
    // input data
    Bj_host host;
    Bj_service_collection service_collection;

    // view data
    bool view_available;
    std::vector<const u2_dns_domain*> domains;
    u2_dns_database database;

    void build_view();
};
