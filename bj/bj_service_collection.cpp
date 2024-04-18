//
//  Bj_service_collection.cpp
//
//  Created by Gabriele Mondada on 12.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include <cassert>
#include <map>
#include "bj_service_collection.h"
#include "bj_util.h"

Bj_service_collection::Bj_service_collection(std::string_view host_name, std::string_view domain_name, const std::vector<Bj_service_instance>& service_instances)
    : host_name(host_name), domain_name(domain_name), service_instances(service_instances)
{
    view_available = false;
}

std::span<const u2_dns_domain*> Bj_service_collection::domains_view()
{
    build_view();
    return std::span<const u2_dns_domain*>(domains);
}

void Bj_service_collection::build_view()
{
    if (view_available)
        return;

    // group instances by service

    std::map<std::string, std::vector<Bj_service_instance>> service_map;

    for (const auto& instance : service_instances) {
        auto service_name = instance.get_service_name();
        if (!service_map.contains(service_name))
            service_map[service_name] = std::vector<Bj_service_instance>();
        std::vector<Bj_service_instance>& instances = service_map[service_name];
        instances.push_back(instance);
    }

    // create services

    for (const auto& [key, value] : service_map) {
        services.push_back(Bj_service(key, domain_name, value));
        dns_service_names.push_back(bj_util::dns_name(key + "." + domain_name));
    }

    // create enum service

    for (auto& dns_service_name : dns_service_names) {
        u2_dns_record r = {
            .domain = &enum_service_domain,
            .type = U2_DNS_RR_TYPE_PTR,
            .ttl = 4500,
            .cache_flush = false,
            .ptr = {
                .name = dns_service_name.c_str(),
            },
        };
        enum_service_records.push_back(r);
    }

    for (auto& record : enum_service_records) {
        enum_service_record_ptrs.push_back(&record);
    }

    enum_service_domain.name = "\011_services\007_dns-sd\004_udp\005local\0";
    enum_service_domain.record_list = enum_service_record_ptrs.data();
    enum_service_domain.record_count = (int)enum_service_record_ptrs.size();

    // put all domains in a list

    for (auto& service : services) {
        auto service_domains = service.domains_view();
        domains.insert(domains.end(), service_domains.begin(), service_domains.end());
    }

    domains.push_back(&enum_service_domain);

    view_available = true;
}
