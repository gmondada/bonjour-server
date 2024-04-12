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


/*** prototypes ***/

size_t u2_mdns_process_query(const struct u2_dns_database *database, void *msg_in, size_t size_in, void *msg_out, size_t max_out);
size_t u2_mdns_generate_unsolicited_announcement(const struct u2_dns_record **record_list, int record_count, bool tear_down, void *msg_out, size_t max_out);


U2_C_END
#endif
