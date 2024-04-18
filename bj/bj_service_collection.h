//
//  Bj_service_collection.h
//
//  Created by Gabriele Mondada on 12.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include <vector>
#include "bj_service.h"
#include "bj_service_instance.h"

class Bj_service_collection {
public:
    Bj_service_collection(std::string_view host_name, std::string_view domain_name, const std::vector<Bj_service_instance>& service_instances);
    Bj_service_collection(const Bj_service_collection&) = delete;
    Bj_service_collection& operator= (const Bj_service_collection&) = delete;

    std::span<const u2_dns_domain*> domains_view();

private:
    // input data   
    std::string host_name;
    std::string domain_name;
    std::vector<Bj_service_instance> service_instances;

    // view data
    bool view_available;
    std::vector<Bj_service> services;
    std::vector<std::string> dns_service_names;
    std::vector<u2_dns_record> enum_service_records;
    std::vector<u2_dns_record*> enum_service_record_ptrs;
    u2_dns_domain enum_service_domain;
    std::vector<const u2_dns_domain*> domains;

    void build_view();
};
