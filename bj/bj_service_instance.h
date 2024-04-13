//
//  bj_service_instance.h
//  bonjour-server
//
//  Created by Gabriele Mondada on 11.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#pragma once
#include <string>
#include <span>
#include <optional>
#include <span>
#include "u2_dns.h"

class Bj_service_instance {
public:
    Bj_service_instance(std::string_view name, std::string_view regtype, std::optional<std::string_view> domain, uint16_t port, std::span<unsigned char> txt_record);

private:
    std::string name;
    std::string regtype;
    std::string domain;
    uint16_t port; // TODO: host encoding?
    std::string txt_record;
};
