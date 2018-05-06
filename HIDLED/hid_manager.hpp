//
//  HIDManager.hpp
//  HIDLED
//
//  Created by Pietro Saccardi on 06/05/2018.
//  Copyright © 2018 Pietro Saccardi. All rights reserved.
//

#ifndef HIDManager_hpp
#define HIDManager_hpp

#include <IOKit/hid/IOHIDLib.h>
#include <assert.h>
#include <vector>
#include <string>

namespace spak {
    
    template <class T, class=typename std::enable_if<std::is_convertible<T, CFTypeRef>::value>::type>
    class cf_wrap {
        T _ref;
    public:
        cf_wrap() : _ref(nullptr) {}
        cf_wrap(T cf_ref) : _ref(cf_ref) {}
        cf_wrap(cf_wrap const &) = delete;
        cf_wrap &operator=(cf_wrap const &) = delete;
        cf_wrap(cf_wrap &&rval) : cf_wrap() {
            std::swap(_ref, rval._ref);
        }
        cf_wrap &operator=(cf_wrap &&rval) {
            release();
            std::swap(_ref, rval._ref);
        }
        void release() {
            if (_ref != nullptr) {
                CFRelease(_ref);
                _ref = nullptr;
            }
        }
        operator T() const {
            return _ref;
        }
        ~cf_wrap() {
            release();
        }
    };
    
    class matching_dict : public cf_wrap<CFMutableDictionaryRef> {
        static CFMutableDictionaryRef create() {
            return CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        }
    public:
        matching_dict(bool device_is_element = false, uint32_t in_usage_page = kHIDPage_Undefined, uint32_t in_usage = 0) : cf_wrap<CFMutableDictionaryRef>(create())
        {
            assert(*this != nullptr);
            if (in_usage_page != 0) {
                // Add key for device type to refine the matching dictionary.
                cf_wrap<CFNumberRef> page_num(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &in_usage_page));
                assert(page_num != nullptr);
                if (device_is_element) {
                    CFDictionarySetValue(*this, CFSTR(kIOHIDElementUsagePageKey), page_num);
                } else {
                    CFDictionarySetValue(*this, CFSTR(kIOHIDDeviceUsagePageKey), page_num);
                }
                // Note: the usage is only valid if the usage page is also defined
                if (in_usage != 0) {
                    cf_wrap<CFNumberRef> usage_num(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &in_usage));
                    assert(usage_num != nullptr);
                    if (device_is_element) {
                        CFDictionarySetValue(*this, CFSTR(kIOHIDElementUsageKey), usage_num);
                    } else {
                        CFDictionarySetValue(*this, CFSTR(kIOHIDDeviceUsageKey), usage_num);
                    }
                }
            }
        }
    };
    
    

    template <class T>
    class hid_device_element_const_value {};
    
    template <class T>
    class hid_device_element_value : public hid_device_element_const_value<T> {};
    
    template <>
    class hid_device_element_const_value<CFIndex> {
    protected:
        IOHIDElementRef _element;
        IOHIDDeviceRef _device;
    public:
        operator CFIndex() const {
            IOHIDValueRef value = nullptr;
            IOReturn const res = IOHIDDeviceGetValue(_device, _element, &value);
            assert(res == kIOReturnSuccess);
            return IOHIDValueGetIntegerValue(value);
        }
        
    };
    
    template <>
    class hid_device_element_value<CFIndex> final : public hid_device_element_const_value<CFIndex> {
    public:
        using hid_device_element_const_value<CFIndex>::hid_device_element_const_value;
        
        hid_device_element_value &operator=(CFIndex value) {
            cf_wrap<IOHIDValueRef> cf_value(IOHIDValueCreateWithIntegerValue(kCFAllocatorDefault, _element, std::time(nullptr), value));
            IOReturn const res = IOHIDDeviceSetValue(_device, _element, cf_value);
            assert(res == kIOReturnSuccess);
            return *this;
        }
    };
    
    
    class hid_device_element {
        IOHIDElementRef _element;
    public:
        hid_device_element(IOHIDElementRef element) : _element(element) {}
        
        uint32_t usage() const {
            return IOHIDElementGetUsage(_element);
        }
        
        uint32_t usage_page() const {
            return IOHIDElementGetUsagePage(_element);
        }
        
        IOHIDElementType type() const {
            return IOHIDElementGetType(_element);
        }
        
        std::string name() const {
            return CFStringGetCStringPtr(reinterpret_cast<CFStringRef>(IOHIDElementGetName(_element)), kCFStringEncodingUTF8);
        }
        
        CFIndex logical_min() const {
            return IOHIDElementGetLogicalMin(_element);
        }
        
        CFIndex logical_max() const {
            return IOHIDElementGetLogicalMax(_element);
        }
        
        template <class T>
        hid_device_element_const_value<T> value() const {
            return {_element, IOHIDElementGetDevice(_element)};
        }
        
        template <class T>
        hid_device_element_value<T> value() {
            return {_element, IOHIDElementGetDevice(_element)};
        }
    };
    
    
    
    class hid_device_elements_enumerator {
        IOHIDDeviceRef _device;
        std::vector<hid_device_element> _elements;
        
        void copy_elements(uint32_t in_page, uint32_t in_usage_page) {
            matching_dict match_elements(true, in_page, in_usage_page);
            cf_wrap<CFArrayRef> elements(IOHIDDeviceCopyMatchingElements(_device, match_elements, kIOHIDOptionsTypeNone));
            assert(elements);
            CFIndex const length = CFArrayGetCount(elements);
            _elements.clear();
            _elements.reserve(length);
            CFArrayApplyFunction(elements, CFRangeMake(0, length), [](void const *item, void *context) {
                assert(context != nullptr);
                reinterpret_cast<std::vector<hid_device_element> *>(context)->emplace_back(reinterpret_cast<IOHIDElementRef>(const_cast<void *>(item)));
            }, &_elements);
        }
    public:
        
        using value_type = std::vector<hid_device_element>::value_type;
        using reference = std::vector<hid_device_element>::reference;
        using const_reference = std::vector<hid_device_element>::const_reference;
        using pointer = std::vector<hid_device_element>::pointer;
        using const_pointer = std::vector<hid_device_element>::const_pointer;
        
        hid_device_elements_enumerator(IOHIDDeviceRef device, uint32_t in_page = kHIDPage_Undefined, uint32_t in_usage_page = 0) :
            _device(device)
        {
            copy_elements(in_page, in_usage_page);
        }
        
        std::vector<hid_device_element> const &elements() const {
            return _elements;
        }
        
        auto begin() const {
            return _elements.begin();
        }
        
        auto end() const {
            return _elements.end();
        }
        
        auto begin() {
            return _elements.begin();
        }
        
        auto end() {
            return _elements.end();
        }
    };
    
    class hid_device {
        IOHIDDeviceRef _device;
        std::string get_string_property(CFStringRef prop_name) const {
            return CFStringGetCStringPtr(reinterpret_cast<CFStringRef>(IOHIDDeviceGetProperty(_device, prop_name)), kCFStringEncodingUTF8);
        }
    public:
        
        hid_device(IOHIDDeviceRef device) : _device(device) {}
        
        bool conforms_to(uint32_t in_page, uint32_t in_usage_page = 0) const {
            return IOHIDDeviceConformsTo(_device, in_page, in_usage_page) == TRUE;
        }
        
        std::string manufacturer() const {
            return get_string_property(CFSTR(kIOHIDManufacturerKey));
        }
        
        std::string product() const {
            return get_string_property(CFSTR(kIOHIDProductKey));
        }
    };
    
    
    class hid_device_enumerator {
        cf_wrap<IOHIDManagerRef> _mgr;
        std::vector<hid_device> _devices;
        
        void setup_device_filter(uint32_t in_page, uint32_t in_usage_page) {
            matching_dict match_keyboards(false, in_page, in_usage_page);
            IOHIDManagerSetDeviceMatching(_mgr, match_keyboards);
        }
        void open() {
            IOReturn const res = IOHIDManagerOpen(_mgr, kIOHIDOptionsTypeNone);
            assert(res == kIOReturnSuccess);
        }
        void copy_devices() {
            cf_wrap<CFSetRef> devices_set(IOHIDManagerCopyDevices(_mgr));
            assert(devices_set != nullptr);
            _devices.clear();
            _devices.reserve(CFSetGetCount(devices_set));
            CFSetApplyFunction(devices_set, [](void const *item, void *context) {
                assert(context != nullptr);
                reinterpret_cast<std::vector<hid_device> *>(context)->emplace_back(reinterpret_cast<IOHIDDeviceRef>(const_cast<void *>(item)));
            }, &_devices);
        }
    public:
        
        using value_type = std::vector<hid_device>::value_type;
        using reference = std::vector<hid_device>::const_reference;
        using const_reference = std::vector<hid_device>::const_reference;
        using pointer = std::vector<hid_device>::const_pointer;
        using const_pointer = std::vector<hid_device>::const_pointer;
        
        hid_device_enumerator(uint32_t in_page = kHIDPage_Undefined, uint32_t in_usage_page = 0) :
            _mgr(IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone))
        {
            setup_device_filter(in_page, in_usage_page);
            open();
            copy_devices();
        }
        
        std::vector<hid_device> const &devices() const {
            return _devices;
        }
        
        auto begin() const {
            return _devices.begin();
        }
        
        auto end() const {
            return _devices.end();
        }
    };
    
        
}

#endif /* HIDManager_hpp */
