//
//  bj_service_instance.cpp
//
//  Created by Gabriele Mondada on 11.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_service_instance.h"
#include "bj_util.h"

Bj_service_instance::Bj_service_instance(std::string_view host_name, std::string_view instance_name, std::string_view service_name, std::string_view domain_name, uint16_t port, std::span<const char> txt_record)
{
    this->host_name      = host_name;
    this->instance_name  = instance_name;
    this->service_name   = service_name;
    this->domain_name    = domain_name;
    this->port           = port;
    this->txt_record     = std::string(txt_record.data(), txt_record.size());
    this->view_available = false;
}

Bj_service_instance::Bj_service_instance(const Bj_service_instance& service)
{
    *this = service;
}

Bj_service_instance& Bj_service_instance::operator= (const Bj_service_instance& service)
{
    this->host_name      = service.host_name;
    this->instance_name  = service.instance_name;
    this->service_name   = service.service_name;
    this->domain_name    = service.domain_name;
    this->port           = service.port;
    this->txt_record     = service.txt_record;
    this->view_available = false;
    return *this;
}

std::string Bj_service_instance::get_service_name() const
{
    return service_name;
}

const u2_dns_domain* Bj_service_instance::domain_view()
{
    build_view();
    return &service_instance_domain;
}

void Bj_service_instance::build_view()
{
    if (view_available)
        return;

    dns_host_name = bj_util::dns_name(host_name + "." + domain_name);
    dns_service_instance_name = bj_util::dns_name(instance_name + "." + service_name + "." + domain_name);

    u2_dns_record srv = {
        .domain = &service_instance_domain,
        .type = U2_DNS_RR_TYPE_SRV,
        .ttl = 120,
        .cache_flush = true,
        .srv = {
            .port = port,
            .name = dns_host_name.c_str(),
        },
    };

    u2_dns_record txt = {
        .domain = &service_instance_domain,
        .type = U2_DNS_RR_TYPE_TXT,
        .ttl = 4500,
        .cache_flush = true,
        .txt = {
            .name = txt_record.c_str(),
        },
    };

    u2_dns_record nsec = {
        .domain = &service_instance_domain,
        .type = U2_DNS_RR_TYPE_NSEC,
        .ttl = 4500,
        .cache_flush = true,
    };

    service_instance_records.clear();
    service_instance_records.push_back(srv);
    service_instance_records.push_back(txt); // TODO: should be added even when empty?
    service_instance_records.push_back(nsec);

    service_instance_record_ptrs.clear();
    for (auto& r : service_instance_records) {
        service_instance_record_ptrs.push_back(&r);
    }

    service_instance_domain.name = dns_service_instance_name.c_str();
    service_instance_domain.record_list = service_instance_record_ptrs.data();
    service_instance_domain.record_count = (int)service_instance_record_ptrs.size();

    view_available = true;
}
