//
//  bj_host.cpp
//  bonjour-server
//
//  Created by Gabriele Mondada on 11.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_host.h"

Bj_host::Bj_host(std::string name, const std::vector<Bj_net_address>& addresses)
{
    this->name = name;
    this->addresses = addresses;

#warning TODO
}

const u2_dns_domain* Bj_host::u2_dns_view()
{
    return &domain;
}
