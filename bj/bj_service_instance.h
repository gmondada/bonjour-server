//
//  bj_service_instance.h
//
//  Created by Gabriele Mondada on 11.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include <string>
#include <span>
#include <vector>
#include <span>
#include "u2_dns.h"

class Bj_service_instance {
public:
    Bj_service_instance(std::string_view host_name, std::string_view instance_name, std::string_view service_name, std::string_view domain_name, uint16_t port, std::span<const char> txt_record);
    Bj_service_instance(const Bj_service_instance& service);
    Bj_service_instance& operator= (const Bj_service_instance& service);

    std::string get_service_name() const;
    const u2_dns_domain* domain_view();

private:
    // input data
    std::string host_name;
    std::string instance_name; // also named service
    std::string service_name;  // also named regtype
    std::string domain_name;
    uint16_t port;
    std::string txt_record;

    // view data
    bool view_available;
    std::string dns_host_name;
    std::string dns_service_instance_name;
    std::vector<u2_dns_record> service_instance_records;
    std::vector<u2_dns_record*> service_instance_record_ptrs;
    u2_dns_domain service_instance_domain;

    void build_view();
};
