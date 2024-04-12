//
//  bj_service.cpp
//  bonjour-server
//
//  Created by Gabriele Mondada on 11.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_service.h"

Bj_service::Bj_service(std::string_view name, std::string_view regtype, std::optional<std::string_view> domain, uint16_t port, std::span<unsigned char> txt_record)
{
    this->name = name;
    this->regtype = regtype;
    this->domain = domain.has_value() ? domain.value() : "local";
    this->port = port;
    this->txt_record = std::string((char *)txt_record.data(), txt_record.size());
}
