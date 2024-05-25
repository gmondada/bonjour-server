//
//  bj_service.cpp
//
//  Created by Gabriele Mondada on 13.04.2024.
//  Copyright © 2024 Gabriele Mondada.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the “Software”),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//

#include "bj_service.h"
#include "bj_util.h"

Bj_service::Bj_service(std::string_view service_name, std::string_view domain_name, const std::vector<Bj_service_instance>& service_instances)
    : service_name(service_name), domain_name(domain_name), service_instances(service_instances)
{
    view_available = false;
}

Bj_service::Bj_service(const Bj_service& service)
{
    *this = service;
}

Bj_service& Bj_service::operator= (const Bj_service& service)
{
    service_name = service.service_name;
    domain_name = service.domain_name;
    service_instances = service.service_instances;
    view_available = false;
    return *this;
}

std::span<const u2_dns_domain*> Bj_service::domains_view()
{
    build_view();
    return std::span<const u2_dns_domain*>(domains);
}

void Bj_service::build_view()
{
    if (view_available)
        return;

    for (auto& instance : service_instances) {
        const u2_dns_domain* instance_domain = instance.domain_view();
        domains.push_back(instance_domain);

        u2_dns_record r = {
            .domain = &service_domain,
            .type = U2_DNS_RR_TYPE_PTR,
            .ttl = 4500,
            .cache_flush = false,
            .ptr = {
                .name = instance_domain->name,
            },
        };
        records.push_back(r);
    }

    for (auto& record : records) {
        record_ptrs.push_back(&record);
    }

    dns_service_name = bj_util::dns_name(service_name + "." + domain_name);

    service_domain.name = dns_service_name.c_str();
    service_domain.record_list = record_ptrs.data();
    service_domain.record_count = (int)record_ptrs.size();
    domains.push_back(&service_domain);

    view_available = true;
}
