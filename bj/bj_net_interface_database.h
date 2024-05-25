//
//  bj_net_interface_database.h
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

#pragma once

#include "bj_host.h"
#include "bj_service_collection.h"

class Bj_net_interface_database {
public:
    Bj_net_interface_database(const Bj_host& host, const Bj_service_collection& service_collection);

    // `domains` amd `database` contain pointers to other class members; moving this object makes them dangling
    Bj_net_interface_database(const Bj_net_interface_database&) = delete;
    Bj_net_interface_database& operator= (const Bj_net_interface_database&) = delete;

    Bj_service_collection& get_service_collection();
    void set_service_collection(const Bj_service_collection& service_collection);

    const u2_dns_database* database_view();

private:
    // input data
    Bj_host host;
    Bj_service_collection service_collection;

    // view data
    bool view_available;
    std::vector<const u2_dns_domain*> domains;
    u2_dns_database database;

    void build_view();
};
