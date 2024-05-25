//
//  bj_host.h
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
#include <vector>
#include "u2_dns.h"
#include "bj_net.h"

class Bj_host {

public:
    Bj_host(std::string_view host_name, std::string_view domain_name, const std::vector<Bj_net_address>& addresses);
    Bj_host(const Bj_host& host);
    Bj_host& operator= (const Bj_host& host);

    const u2_dns_domain* domain_view();

private:
    // input data
    std::string host_name;
    std::string domain_name;
    std::vector<Bj_net_address> addresses;

    // view data
    bool view_available;
    std::string dns_host_name;
    std::vector<u2_dns_record> records;
    std::vector<u2_dns_record*> record_ptrs;
    u2_dns_domain domain;

    void build_view();
};
