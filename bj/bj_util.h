//
//  bj_util.h
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include <span>
#include <string>

namespace bj_util
{

void dump_data(std::span<unsigned char> data, int indent = 0);
std::string dns_name(std::string_view name);

}
