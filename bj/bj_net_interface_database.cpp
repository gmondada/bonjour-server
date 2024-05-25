//
//  bj_net_interface_database.cpp
//
//  Created by Gabriele Mondada on 21.04.2024.
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

#include "bj_net_interface_database.h"

Bj_net_interface_database::Bj_net_interface_database(const Bj_host& host, const Bj_service_collection& service_collection)
    : host(host), service_collection(service_collection)
{
    view_available = false;
}

Bj_service_collection& Bj_net_interface_database::get_service_collection()
{
    return service_collection;
}

void Bj_net_interface_database::set_service_collection(const Bj_service_collection& service_collection)
{
    this->service_collection = service_collection;
    view_available = false;
}

const u2_dns_database* Bj_net_interface_database::database_view()
{
    build_view();
    return &database;
}

void Bj_net_interface_database::build_view()
{
    if (view_available)
        return;

    auto host_domain = host.domain_view();
    auto service_domains = service_collection.domains_view();
    domains.clear();
    domains.push_back(host_domain);
    domains.insert(domains.end(), service_domains.begin(), service_domains.end());
    database.domain_list = domains.data();
    database.domain_count = (int)domains.size();

    view_available = true;
}
