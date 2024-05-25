//
//  bj_service.h
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

#pragma once
#include <string>
#include <vector>
#include "u2_dns.h"
#include "bj_service_instance.h"

class Bj_service {
public:
    Bj_service(std::string_view service_name, std::string_view domain_name, const std::vector<Bj_service_instance>& service_instances);
    Bj_service(const Bj_service&);
    Bj_service& operator= (const Bj_service&);

    std::span<const u2_dns_domain*> domains_view();

private:
    // input data
    std::string service_name;
    std::string domain_name;
    std::vector<Bj_service_instance> service_instances;

    // view data
    bool view_available;
    std::vector<u2_dns_record> records;
    std::vector<u2_dns_record*> record_ptrs;
    std::string dns_service_name;
    u2_dns_domain service_domain;
    std::vector<const u2_dns_domain*> domains;

    void build_view();
};
