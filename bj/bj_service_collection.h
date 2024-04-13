//
//  Bj_service_collection.h
//
//  Created by Gabriele Mondada on 12.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include <vector>
#include "bj_service_instance.h"

class Bj_service_collection {
public:
    Bj_service_collection(std::vector<Bj_service_instance> services);
    Bj_service_collection(const Bj_service_collection&) = delete;
    Bj_service_collection& operator= (const Bj_service_collection&) = delete;

    std::span<const u2_dns_domain*> u2_dns_view();

private:
    std::vector<Bj_service_instance> services;
};
