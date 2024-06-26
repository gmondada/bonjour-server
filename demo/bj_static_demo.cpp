//
//  bj_static_demo.cpp
//
//  Created by Gabriele Mondada on March 29, 2024.
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

#include "bj_static_demo.h"
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include "u2_base.h"
#include "u2_dns.h"
#include "u2_dns_dump.h"
#include "u2_mdns.h"
#include "bj_static_server.h"
#include "bj_net_single_apple.h"
#include "u2_dns_dump.h"

#define _ENUM_SERVICE "\011_services\007_dns-sd\004_udp\005local\0"
#define _SERVICE "\011_service1\004_udp\005local\0"
#define _SERVICE_INSTANCE "\022Service Instance 1" _SERVICE
#define _HOST_NAME "\013ServiceHost\005local\0"
#define _HOST_PORT 1234
#define _SERVICE_INSTANCE_TXT "\5foo=1\0"


static const char _enum_service[] = _ENUM_SERVICE;
static const char _service[] = _SERVICE;
static const char _service_instance[] = _SERVICE_INSTANCE;
static const char _host_name[] = _HOST_NAME;
static const char _service_instance_txt[] = _SERVICE_INSTANCE_TXT;


// host

extern const struct u2_dns_domain _host_domain;

const struct u2_dns_record _host_record_a = {
    .domain = &_host_domain,
    .type = U2_DNS_RR_TYPE_A,
    .ttl = 120,
    .cache_flush = true,
    .a = {
        .addr = {192, 168, 23, 45},
    },
};

const struct u2_dns_record _host_record_nsec = {
    .domain = &_host_domain,
    .type = U2_DNS_RR_TYPE_NSEC,
    .ttl = 4500,
    .cache_flush = true,
};

const struct u2_dns_record *_host_records[] = {
    &_host_record_a,
    &_host_record_nsec,
};

const struct u2_dns_domain _host_domain = {
    .name = _host_name,
    .record_list = _host_records,
    .record_count = U2_ARRAY_LEN(_host_records),
};

// service instance

extern const struct u2_dns_domain _service_instance_domain;

const struct u2_dns_record _service_instance_record_srv = {
    .domain = &_service_instance_domain,
    .type = U2_DNS_RR_TYPE_SRV,
    .ttl = 120,
    .cache_flush = true,
    .srv = {
        .port = _HOST_PORT,
        .name = _host_name,
    },
};

const struct u2_dns_record _service_instance_record_txt = {
    .domain = &_service_instance_domain,
    .type = U2_DNS_RR_TYPE_TXT,
    .ttl = 4500,
    .cache_flush = true,
    .txt = {
        .name = _service_instance_txt,
    },
};

const struct u2_dns_record _service_instance_record_nsec = {
    .domain = &_service_instance_domain,
    .type = U2_DNS_RR_TYPE_NSEC,
    .ttl = 4500,
    .cache_flush = true,
};

const struct u2_dns_record *_service_instance_records[] = {
    &_service_instance_record_srv,
    &_service_instance_record_txt,
    &_service_instance_record_nsec,
};

const struct u2_dns_domain _service_instance_domain = {
    .name = _service_instance,
    .record_list = _service_instance_records,
    .record_count = U2_ARRAY_LEN(_service_instance_records),
};

// service

extern const struct u2_dns_domain _service_domain;

const struct u2_dns_record _service_record_ptr = {
    .domain = &_service_domain,
    .type = U2_DNS_RR_TYPE_PTR,
    .ttl = 4500,
    .cache_flush = false,
    .ptr = {
        .name = _service_instance,
    },
};

const struct u2_dns_record *_service_records[] = {
    &_service_record_ptr,
};

const struct u2_dns_domain _service_domain = {
    .name = _service,
    .record_list = _service_records,
    .record_count = U2_ARRAY_LEN(_service_records),
};

// enum service

extern const struct u2_dns_domain _enum_service_domain;

const struct u2_dns_record _enum_service_record_ptr = {
    .domain = &_enum_service_domain,
    .type = U2_DNS_RR_TYPE_PTR,
    .ttl = 4500,
    .cache_flush = false,
    .ptr = {
        .name = _service,
    },
};

const struct u2_dns_record *_enum_service_records[] = {
    &_enum_service_record_ptr,
};

const struct u2_dns_domain _enum_service_domain = {
    .name = _enum_service,
    .record_list = _enum_service_records,
    .record_count = U2_ARRAY_LEN(_enum_service_records),
};

// list of all domains

const struct u2_dns_domain *_domains[] = {
    &_host_domain,
    &_service_instance_domain,
    &_service_domain,
    &_enum_service_domain,
};

const struct u2_dns_database _database = {
    .domain_list = _domains,
    .domain_count = U2_ARRAY_LEN(_domains),
};


void bj_static_demo()
{
    u2_dns_database_dump(&_database, 0);
    printf("\n");

    Bj_net_single_apple net(Bj_net_address(Bj_net_protocol::ipv4), std::vector<Bj_net_address>(), true);
    net.set_log_level(1);
    
    Bj_static_server server(net, _database);
    server.set_log_level(2);

    server.start();

    for (;;) {
        sleep(3600);
    }
}
