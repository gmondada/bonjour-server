//
//  bj_net.cpp
//
//  Created by Gabriele Mondada on March 29, 2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_net.h"
#include <sstream>
#include <iomanip>

std::string Bj_net_address::as_str() const
{
    switch (protocol) {
        case Bj_net_protocol::ipv4: {
            std::stringstream ss;
            for (int i = 0 ; i < ipv4.size(); i++) {
                if (i > 0)
                    ss << ".";
                ss << (int)ipv4[i];
            }
            return ss.str();
        }
        case Bj_net_protocol::ipv6: {
            std::stringstream ss;
            for (int i = 0 ; i < ipv6.size(); i++) {
                if (i > 0 && (i % 2) == 0)
                    ss << ":";
                ss << std::hex << std::setw(2) << std::setfill('0') << (int)ipv6[i];
            }
            return ss.str();
        }
        default:
            return "?";
    }
}

bool Bj_net_address::is_any() const
{
    switch (protocol) {
        case Bj_net_protocol::ipv4:
            for (auto byte : ipv4) {
                if (byte != 0)
                    return false;
            }
            return true;
        case Bj_net_protocol::ipv6: {
            for (auto byte : ipv6) {
                if (byte != 0)
                    return false;
            }
            return true;
        }
        default:
            return true;
    }
}
