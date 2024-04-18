//
//  bj_service.h
//
//  Created by Gabriele Mondada on 13.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include <string>
#include <vector>
#include "u2_dns.h"
#include "bj_service_instance.h"

class Bj_service {
public:
    Bj_service(std::string_view service_name, std::string_view domain_name, const std::vector<Bj_service_instance>& service_instances);
    Bj_service(const Bj_service&);
    Bj_service& operator= (const Bj_service&);

    std::span<const u2_dns_domain*> domains_view();

private:
    // input data
    std::string service_name;
    std::string domain_name;
    std::vector<Bj_service_instance> service_instances;

    // view data
    bool view_available;
    std::vector<u2_dns_record> records;
    std::vector<u2_dns_record*> record_ptrs;
    std::string dns_service_name;
    u2_dns_domain service_domain;
    std::vector<const u2_dns_domain*> domains;

    void build_view();
};
