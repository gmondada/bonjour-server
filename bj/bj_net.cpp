//
//  bj_net.cpp
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
