//
//  u2_mdns.h
//
//  Created by Gabriele Mondada on March 29, 2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#ifndef _U2_MDNS_H_
#define _U2_MDNS_H_

#include <stdio.h>
#include "u2_dns.h"
#include "u2_def.h"

U2_C_BEGIN


/*** literals ***/

#define U2_MDNS_MSG_SIZE_MAX 9000 // this includes IP and UDP headers


/*** types ***/

struct u2_mdns_response_record {
    enum u2_dns_rr_category category;
    const struct u2_dns_record *record;
};

struct u2_mdns_emitter {
    // mandatory records first, optional records last
    const struct u2_mdns_response_record *record_list;
    int mandatory_record_count;
    int optional_record_count;
    int record_index;
    bool tear_down;
};

struct u2_mdns_query_proc {
    int decoding_error;

    const struct u2_dns_database *database;

    struct u2_dns_msg_reader reader;
    int question_count;
    int question_index;

    struct u2_mdns_response_record record_list[32];
    int answer_record_count;
    int additional_record_count;

    struct u2_mdns_emitter emitter;
};


/*** prototypes ***/

void u2_mdsn_query_proc_init(struct u2_mdns_query_proc *proc, const void *msg, size_t size, const struct u2_dns_database *database);
size_t u2_mdns_query_proc_run(struct u2_mdns_query_proc *proc, void *out_msg, size_t ideal_size, size_t max_size);

void u2_mdns_emitter_init(struct u2_mdns_emitter *emitter, const struct u2_mdns_response_record *record_list, int mandatory_record_count, int optional_record_count, bool tear_down);
size_t u2_mdns_emitter_run(struct u2_mdns_emitter *emitter, void *out_msg, size_t ideal_size, size_t max_size);


U2_C_END
#endif
