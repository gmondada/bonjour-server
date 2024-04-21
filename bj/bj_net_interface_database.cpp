//
//  bj_net_interface_database.cpp
//
//  Created by Gabriele Mondada on 21.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_net_interface_database.h"

Bj_net_interface_database::Bj_net_interface_database(const Bj_host& host, const Bj_service_collection& service_collection)
    : host(host), service_collection(service_collection)
{
    view_available = false;
}

Bj_service_collection& Bj_net_interface_database::get_service_collection()
{
    return service_collection;
}

void Bj_net_interface_database::set_service_collection(const Bj_service_collection& service_collection)
{
    this->service_collection = service_collection;
    view_available = false;
}

const u2_dns_database* Bj_net_interface_database::database_view()
{
    build_view();
    return &database;
}

void Bj_net_interface_database::build_view()
{
    if (view_available)
        return;

    auto host_domain = host.domain_view();
    auto service_domains = service_collection.domains_view();
    domains.clear();
    domains.push_back(host_domain);
    domains.insert(domains.end(), service_domains.begin(), service_domains.end());
    database.domain_list = domains.data();
    database.domain_count = (int)domains.size();

    view_available = true;
}
