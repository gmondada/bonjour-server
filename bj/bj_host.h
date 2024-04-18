//
//  bj_host.h
//
//  Created by Gabriele Mondada on 11.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include <string>
#include <vector>
#include "u2_dns.h"
#include "bj_net.h"

class Bj_host {

public:
    Bj_host(std::string_view host_name, std::string_view domain_name, const std::vector<Bj_net_address>& addresses);
    Bj_host(const Bj_host&) = delete;
    Bj_host& operator= (const Bj_host&) = delete;

    const u2_dns_domain* u2_dns_view();

private:
    // input data
    std::string host_name;
    std::string domain_name;
    std::vector<Bj_net_address> addresses;

    // view data
    bool view_available;
    std::string dns_host_name;
    std::vector<u2_dns_record> records;
    std::vector<u2_dns_record*> record_ptrs;
    u2_dns_domain domain;

    void build_view();
};
