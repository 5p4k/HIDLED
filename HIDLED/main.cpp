//
//  main.cpp
//  HIDLED
//
//  Created by Pietro Saccardi on 06/05/2018.
//  Copyright © 2018 Pietro Saccardi. All rights reserved.
//

#include <iostream>
#include "hid_manager.hpp"

int main(int argc, const char * argv[]) {
    for (auto device : spak::hid_device_enumerator(kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard)) {
        std::cout << "Device " << device.manufacturer() << " >> " << device.product();
        spak::hid_device_opener opener = device.open();
        if (not opener.is_open()) {
            std::cout << " (can't be opened: " << spak::describe_io_return(opener.result()) << ")";
        }
        std::cout << std::endl;
        for (auto element : device.elements(kHIDPage_LEDs)) {
            std::cout << "   Element " << element.name() << " [" << element.logical_min() << ".." << element.logical_max() << "]";
            if (opener.is_open()) {
                std::cout << ": " << element.value<CFIndex>();
            }
            std::cout << std::endl;
        }
    }
    return 0;
}
