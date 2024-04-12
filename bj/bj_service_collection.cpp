//
//  Bj_service_collection.cpp
//
//  Created by Gabriele Mondada on 12.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_service_collection.h"

Bj_service_collection::Bj_service_collection(std::vector<Bj_service> services)
{
    this->services = services;
}

std::span<const u2_dns_domain*> Bj_service_collection::u2_dns_view()
{
#warning TODO
    return std::span<const u2_dns_domain*>();
}
