//
//  bj_service_instance.h
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

#pragma once
#include <string>
#include <span>
#include <vector>
#include <span>
#include "u2_dns.h"

class Bj_service_instance {
public:
    Bj_service_instance(std::string_view host_name, std::string_view instance_name, std::string_view service_name, std::string_view domain_name, uint16_t port, std::span<const char> txt_record);
    Bj_service_instance(const Bj_service_instance& service);
    Bj_service_instance& operator= (const Bj_service_instance& service);

    std::string get_service_name() const;
    const u2_dns_domain* domain_view();

private:
    // input data
    std::string host_name;
    std::string instance_name; // also named service
    std::string service_name;  // also named regtype
    std::string domain_name;
    uint16_t port;
    std::string txt_record;

    // view data
    bool view_available;
    std::string dns_host_name;
    std::string dns_service_instance_name;
    std::vector<u2_dns_record> service_instance_records;
    std::vector<u2_dns_record*> service_instance_record_ptrs;
    u2_dns_domain service_instance_domain;

    void build_view();
};
