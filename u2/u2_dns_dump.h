//
//  u2_dns_dump.h
//
//  Created by Gabriele Mondada on March 29, 2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#ifndef _U2_DNS_DUMP_H_
#define _U2_DNS_DUMP_H_

#include "u2_def.h"
#include "u2_dns.h"

U2_C_BEGIN


/*** prototypes ***/

void u2_dns_msg_dump_name(const void *data, int pos);
void u2_dns_msg_dump_question(const void *data, const struct u2_dns_msg_entry *entry, int indent);
void u2_dns_msg_dump_rr(const void *data, const struct u2_dns_msg_entry *entry, int indent);
void u2_dns_msg_dump(const void *data, size_t size, int indent);
void u2_dns_database_dump(const struct u2_dns_database *database, int indent);


U2_C_END
#endif
