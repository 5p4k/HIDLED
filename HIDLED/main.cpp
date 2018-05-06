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
        std::cout << "Device " << device.manufacturer() << "/" << device.product() << std::endl;
        for (auto element : device.elements(kHIDPage_LEDs)) {
            std::cout << "   Element " << element.name() << " [" << element.logical_min() << ".." << element.logical_max() << "]";
            try {
                CFIndex const val = element.value<CFIndex>();
                std::cout << ": " << val;
            } catch (spak::cannot_open_device) {
                // Well...
            } catch (...) {
                throw;
            }
            std::cout << std::endl;
        }
    }
    return 0;
}
