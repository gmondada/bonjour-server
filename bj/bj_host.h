//
//  bj_host.h
//  bonjour-server
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
    Bj_host(std::string name, const std::vector<Bj_net_address>& addresses);
    Bj_host(const Bj_host&) = delete;
    Bj_host& operator= (const Bj_host&) = delete;

    const u2_dns_domain* u2_dns_view();

private:
    std::string name;
    std::vector<Bj_net_address> addresses;

    std::vector<u2_dns_record> records;
    u2_dns_domain domain;
};
