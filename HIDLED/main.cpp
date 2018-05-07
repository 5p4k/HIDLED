//
//  main.cpp
//  HIDLED
//
//  Created by Pietro Saccardi on 06/05/2018.
//  Copyright © 2018 Pietro Saccardi. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to
//  deal in the Software without restriction, including without limitation the
//  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
//  sell copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
//  IN THE SOFTWARE.
//

#include <iostream>
#include <sstream>
#include "hid.hpp"

void list(spak::hid_device_enumerator &enumerator) {
    for (auto &device : enumerator) {
        std::string const manu = device.manufacturer();
        std::string const prod = device.product();
        std::cout << "Device ";
        if (prod.empty()) {
            std::cout << "<unknown>";
        } else {
            std::cout << "'" << prod << "'";
        }
        std::cout << " by ";
        if (manu.empty()) {
            std::cout << "<unknown>";
        } else {
            std::cout << "'" << manu << "'";
        }
        
        spak::hid_device_opener opener = device.open();
        if (not opener.is_open()) {
            std::cout << " (can't be opened: " << spak::describe_io_return(opener.result()) << ")";
        }
        std::cout << std::endl;
        
        std::size_t elm_idx = 0;
        for (auto &element : device.elements(kHIDPage_LEDs)) {
            std::cout << "    Element " << elm_idx ;
            std::string const name = element.name();
            if (not name.empty()) {
                std::cout << " \"" << name << "\"";
            }
            std::cout << " [" << element.logical_min() << ".." << element.logical_max() << "]";
            if (opener.is_open()) {
                try {
                    CFIndex const value = element.value<CFIndex>();
                    std::cout << ": " << value;
                } catch (...) {
                    // Pass
                }
            }
            std::cout << std::endl;
            ++elm_idx;
        }
    }
}

std::unique_ptr<spak::hid_device> match_keyboard(spak::hid_device_enumerator &enumerator, std::string const &match_prod = "", std::string const &match_manu = "") {
    for (auto device : enumerator) {
        std::string const manu = device.manufacturer();
        std::string const prod = device.product();
        if (not match_manu.empty() and manu != match_manu) {
            continue;
        }
        if (not match_prod.empty() and prod != match_prod) {
            continue;
        }
        return std::make_unique<spak::hid_device>(std::move(device));
    }
    return nullptr;
}


void help() {
    std::cout << "Usage: <program> --help" << std::endl;
    std::cout << "       <program> [--list]" << std::endl;
    std::cout << "       <program> [--product <product>] [--manufacturer <manufacturer>] --toggle <led_idx>" << std::endl;
    std::cout << "       <program> [--product <product>] [--manufacturer <manufacturer>] --set <led_idx> <value>" << std::endl;
}


struct cmdline {
    
    enum struct actions {
        list,
        toggle,
        set,
        help,
        wrong_cmd_line
    };
    
    actions action;
    std::string match_manufacturer;
    std::string match_product;
    std::size_t element;
    CFIndex value;
    
    cmdline() : action(actions::list), element(std::numeric_limits<std::size_t>::max()), value(0) {}
    
    void parse(int argc, const char * argv[]) {
        for (int argn = 1; argn < argc; ++argn) {
            std::string const arg = argv[argn];
            if (arg == "-h" or arg == "--help") {
                action = actions::help;
            } else if (arg == "-l" or arg == "--list") {
                action = actions::list;
            } else if (arg == "-p" or arg == "--product") {
                if (argn >= argc - 1) {
                    std::cerr << "Missing argument 'product' at position " << argn + 1 << std::endl;
                    action = actions::wrong_cmd_line;
                    continue;
                }
                match_product = argv[++argn];
            } else if (arg == "-m" or arg == "--manufacturer") {
                if (argn >= argc - 1) {
                    std::cerr << "Missing argument 'manufacturer' at position " << argn + 1 << std::endl;
                    action = actions::wrong_cmd_line;
                    continue;
                }
                match_manufacturer = argv[++argn];
            } else if (arg == "-s" or arg == "--set") {
                if (argn >= argc - 1) {
                    std::cerr << "Missing argument 'element index' at position " << argn + 1 << std::endl;
                    action = actions::wrong_cmd_line;
                    continue;
                }
                action = actions::set;
                std::stringstream ss_idx(argv[++argn]);
                ss_idx >> element;
                if (argn >= argc - 1) {
                    std::cerr << "Missing argument 'value' at position " << argn + 1 << std::endl;
                    action = actions::wrong_cmd_line;
                    continue;
                }
                std::stringstream ss_val(argv[++argn]);
                ss_val >> value;
            } else if (arg == "-t" or arg == "--toggle") {
                if (argn >= argc - 1) {
                    std::cerr << "Missing argument 'element index' at position " << argn + 1 << std::endl;
                    action = actions::wrong_cmd_line;
                    continue;
                }
                action = actions::toggle;
                std::stringstream ss_idx(argv[++argn]);
                ss_idx >> element;
            } else {
                std::cerr << "Unknown switch or argument '" << arg << "' at position " << argn << std::endl;
                action = actions::wrong_cmd_line;
            }
        }
        
    }
};


namespace return_code {
    static constexpr int ok = 0;
    static constexpr int cmdline_error = 1;
    static constexpr int keyboard_not_found = 2;
    static constexpr int cannot_open_device = 3;
    static constexpr int led_not_found = 4;
    static constexpr int unknown_error = 5;
}


int main(int argc, const char * argv[]) {
    try {
        spak::hid_device_enumerator enumerator(kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard);
        cmdline cmd;
        cmd.parse(argc, argv);
        switch (cmd.action) {
            case cmdline::actions::wrong_cmd_line:
                help();
                return return_code::cmdline_error;
            case cmdline::actions::help:
                help();
                return return_code::ok;
            case cmdline::actions::list:
                list(enumerator);
                return return_code::ok;
            case cmdline::actions::set:
                [[fallthrough]];
            case cmdline::actions::toggle: {
                auto p_device = match_keyboard(enumerator, cmd.match_product, cmd.match_manufacturer);
                if (p_device == nullptr) {
                    std::cerr << "Unable to find a keyboard matching";
                    if (not cmd.match_product.empty()) {
                        std::cerr << " product '" << cmd.match_product << "'";
                    }
                    if (not cmd.match_manufacturer.empty()) {
                        std::cerr << " manufacturer '" << cmd.match_manufacturer << "'";
                    }
                    std::cerr << "." << std::endl;
                    return return_code::keyboard_not_found;
                }
                spak::hid_device_opener opener = p_device->open();
                if (not opener.is_open()) {
                    std::cerr << "Could not open device: " << spak::describe_io_return(opener.result()) << std::endl;
                    return return_code::cannot_open_device;
                }
                spak::hid_device_elements_enumerator elements = p_device->elements(kHIDPage_LEDs);
                if (cmd.element >= elements.size()) {
                    std::cerr << "Device has only " << elements.size() << " LED elements, cannot find LED number " << cmd.element << std::endl;
                    return return_code::led_not_found;
                }
                if (cmd.action == cmdline::actions::set) {
                    elements[cmd.element].value<CFIndex>() = cmd.value;
                } else {
                    spak::hid_device_element &element = elements[cmd.element];
                    spak::hid_device_element_value<CFIndex> value = element.value<CFIndex>();
                    if (value == element.logical_min()) {
                        value = element.logical_max();
                    } else {
                        value = element.logical_min();
                    }
                }
                return return_code::ok;
            }
        }
    } catch (std::exception e) {
        std::cerr << "Unknown exception: " << e.what() << std::endl;
        return return_code::unknown_error;
    }
}
