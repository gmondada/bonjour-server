//
//  bj_host.cpp
//
//  Created by Gabriele Mondada on 11.04.2024.
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

#include <cassert>
#include "bj_host.h"
#include "bj_util.h"

Bj_host::Bj_host(std::string_view host_name, std::string_view domain_name, const std::vector<Bj_net_address>& addresses)
    : host_name(host_name), domain_name(domain_name), addresses(addresses)
{
    view_available = false;
}

Bj_host::Bj_host(const Bj_host& host)
{
    *this = host;
}

Bj_host& Bj_host::operator= (const Bj_host& host)
{
    host_name = host.host_name;
    domain_name = host.domain_name;
    addresses = host.addresses;
    view_available = false;
    return *this;
}

const u2_dns_domain* Bj_host::domain_view()
{
    build_view();
    return &domain;
}

void Bj_host::build_view()
{
    if (view_available)
        return;

    dns_host_name = bj_util::dns_name(host_name + "." + domain_name);

    records.clear();
    for (auto &address : addresses) {
        u2_dns_record r = {
            .domain = &domain,
            .ttl = 120,
            .cache_flush = true,
        };
        switch (address.protocol) {
            case Bj_net_protocol::ipv4:
                r.type = U2_DNS_RR_TYPE_A;
                assert(sizeof(r.a.addr) == address.ipv4.size());
                std::copy(address.ipv4.begin(), address.ipv4.end(), r.a.addr);
                break;
            case Bj_net_protocol::ipv6:
                r.type = U2_DNS_RR_TYPE_AAAA;
                assert(sizeof(r.aaaa.addr) == address.ipv6.size());
                std::copy(address.ipv6.begin(), address.ipv6.end(), r.aaaa.addr);
                break;
            default:
                continue;
        }
        records.push_back(r);
    }

    struct u2_dns_record nsec = {
        .domain = &domain,
        .type = U2_DNS_RR_TYPE_NSEC,
        .ttl = 4500,
        .cache_flush = true,
    };
    records.push_back(nsec);

    record_ptrs.clear();
    for (auto& record : records) {
        record_ptrs.push_back(&record);
    }

    domain.name = dns_host_name.c_str();
    domain.record_list = record_ptrs.data();
    domain.record_count = (int)record_ptrs.size();

    view_available = true;
}
