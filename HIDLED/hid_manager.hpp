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
    
    const char *describe_io_return(IOReturn code) {
        switch (code) {
            case kIOReturnSuccess:          return "OK";
            case kIOReturnError:            return "general error";
            case kIOReturnNoMemory:         return "can't allocate memory";
            case kIOReturnNoResources:      return "resource shortage";
            case kIOReturnIPCError:         return "error during IPC";
            case kIOReturnNoDevice:         return "no such device";
            case kIOReturnNotPrivileged:    return "privilege violation";
            case kIOReturnBadArgument:      return "invalid argument";
            case kIOReturnLockedRead:       return "device read locked";
            case kIOReturnLockedWrite:      return "device write locked";
            case kIOReturnExclusiveAccess:  return "exclusive access and device already open";
            case kIOReturnBadMessageID:     return "sent/received messages had different msg_id";
            case kIOReturnUnsupported:      return "unsupported function";
            case kIOReturnVMError:          return "misc. VM failure";
            case kIOReturnInternalError:    return "internal error";
            case kIOReturnIOError:          return "General I/O error";
            case kIOReturnCannotLock:       return "can't acquire lock";
            case kIOReturnNotOpen:          return "device not open";
            case kIOReturnNotReadable:      return "read not supported";
            case kIOReturnNotWritable:      return "write not supported";
            case kIOReturnNotAligned:       return "alignment error";
            case kIOReturnBadMedia:         return "Media Error";
            case kIOReturnStillOpen:        return "device(s) still open";
            case kIOReturnRLDError:         return "rld failure";
            case kIOReturnDMAError:         return "DMA failure";
            case kIOReturnBusy:             return "Device Busy";
            case kIOReturnTimeout:          return "I/O Timeout";
            case kIOReturnOffline:          return "device offline";
            case kIOReturnNotReady:         return "not ready";
            case kIOReturnNotAttached:      return "device not attached";
            case kIOReturnNoChannels:       return "no DMA channels left";
            case kIOReturnNoSpace:          return "no space for data";
            case kIOReturnPortExists:       return "port already exists";
            case kIOReturnCannotWire:       return "can't wire down physical memory";
            case kIOReturnNoInterrupt:      return "no interrupt attached";
            case kIOReturnNoFrames:         return "no DMA frames enqueued";
            case kIOReturnMessageTooLarge:  return "oversized msg received on interrupt port";
            case kIOReturnNotPermitted:     return "not permitted";
            case kIOReturnNoPower:          return "no power to device";
            case kIOReturnNoMedia:          return "media not present";
            case kIOReturnUnformattedMedia: return "media not formatted";
            case kIOReturnUnsupportedMode:  return "no such mode";
            case kIOReturnUnderrun:         return "data underrun";
            case kIOReturnOverrun:          return "data overrun";
            case kIOReturnDeviceError:      return "the device is not working properly!";
            case kIOReturnNoCompletion:     return "a completion routine is required";
            case kIOReturnAborted:          return "operation aborted";
            case kIOReturnNoBandwidth:      return "bus bandwidth would be exceeded";
            case kIOReturnNotResponding:    return "device not responding";
            case kIOReturnIsoTooOld:        return "isochronous I/O request for distant past!";
            case kIOReturnIsoTooNew:        return "isochronous I/O request for distant future";
            case kIOReturnNotFound:         return "data was not found";
            case kIOReturnInvalid:          return "should never be seen";
            default: return "<unknown>";
        }
    }
    
    std::string copy_cf_string(CFStringRef str) {
        if (str == nullptr) {
            return "";
        }
        const char *c_str = CFStringGetCStringPtr(str, CFStringGetSystemEncoding());
        if (c_str != nullptr) {
            return c_str;
        }
        // Fallback
        std::string retval;
        std::size_t bufsize = 1024;
        char *buf = nullptr;
        do {
            try {
                void *old_buf = buf;
                if (buf == nullptr) {
                    buf = reinterpret_cast<char *>(malloc(bufsize));
                } else {
                    buf = reinterpret_cast<char *>(realloc(buf, bufsize));
                }
                if (buf == nullptr) {
                    std::cerr << "Not enough memory." << std::endl;
                    if (old_buf != nullptr) {
                        free(old_buf);
                    }
                    break;
                }
                if (CFStringGetCString(str, buf, bufsize, CFStringGetSystemEncoding()) == TRUE) {
                    retval = buf;
                    break;
                }
            } catch (std::exception e) {
                std::cerr << "Caught unexpected exception: " << e.what() << std::endl;
                break;
            }
            bufsize *= 2;
        } while (bufsize < 1024 * 1024);
        if (buf != nullptr) {
            free(buf);
        }
        return retval;
    }
    
    
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
    
    
    class hid_device_opener {
        IOHIDDeviceRef _device;
        IOReturn _open_res;
        void close() {
            if (_device != nullptr) {
                IOHIDDeviceClose(_device, kIOHIDOptionsTypeNone);
                _device = nullptr;
            }
        }
    public:
        hid_device_opener(IOHIDDeviceRef device) : _device(device), _open_res(kIOReturnSuccess) {
            _open_res = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
            if (_open_res != kIOReturnSuccess) {
                std::cerr << "Cannot open device: " << describe_io_return(_open_res) << std::endl;
                _device = nullptr;
            }
        }
        IOReturn result() const {
            return _open_res;
        }
        bool is_open() const {
            return _device != nullptr;
        }
        ~hid_device_opener() {
            close();
        }
    };
    
    class cannot_open_device: public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };
    

    template <class T>
    class hid_device_element_const_value {};
    
    template <class T>
    class hid_device_element_value : public hid_device_element_const_value<T> {};
    
    template <>
    class hid_device_element_const_value<CFIndex> {
    protected:
        IOHIDDeviceRef _device;
        IOHIDElementRef _element;
    public:
        hid_device_element_const_value(IOHIDDeviceRef device, IOHIDElementRef element) : _device(device), _element(element) {}
        hid_device_element_const_value(IOHIDElementRef element) : hid_device_element_const_value(IOHIDElementGetDevice(element), element) {}
        
        operator CFIndex() const {
            IOHIDValueRef value = nullptr;
            IOReturn res = IOHIDDeviceGetValue(_device, _element, &value);
            if (res == kIOReturnNotOpen) {
                // Let's open it ourselves
                hid_device_opener opener(_device);
                if (not opener.is_open()) {
                    throw cannot_open_device(describe_io_return(opener.result()));
                }
                res = IOHIDDeviceGetValue(_device, _element, &value);
            }
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
            IOReturn res = IOHIDDeviceSetValue(_device, _element, cf_value);
            if (res == kIOReturnNotOpen) {
                // Let's open it ourselves
                hid_device_opener opener(_device);
                res = IOHIDDeviceSetValue(_device, _element, cf_value);
            }
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
            return copy_cf_string(reinterpret_cast<CFStringRef>(IOHIDElementGetName(_element)));
        }
        
        CFIndex logical_min() const {
            return IOHIDElementGetLogicalMin(_element);
        }
        
        CFIndex logical_max() const {
            return IOHIDElementGetLogicalMax(_element);
        }
        
        template <class T>
        hid_device_element_const_value<T> value() const {
            return {_element};
        }
        
        template <class T>
        hid_device_element_value<T> value() {
            return {_element};
        }
    };
    
    
    
    class hid_device_const_elements_enumerator {
    protected:
        IOHIDDeviceRef _device;
        std::vector<hid_device_element> _elements;
    private:
        void copy_elements(uint32_t in_page, uint32_t in_usage_page) {
            matching_dict match_elements(true, in_page, in_usage_page);
            cf_wrap<CFArrayRef> elements(IOHIDDeviceCopyMatchingElements(_device, match_elements, kIOHIDOptionsTypeNone));
            if (elements != nullptr) {
                CFIndex const length = CFArrayGetCount(elements);
                _elements.clear();
                _elements.reserve(length);
                CFArrayApplyFunction(elements, CFRangeMake(0, length), [](void const *item, void *context) {
                    assert(context != nullptr);
                    reinterpret_cast<std::vector<hid_device_element> *>(context)->emplace_back(reinterpret_cast<IOHIDElementRef>(const_cast<void *>(item)));
                }, &_elements);
            }
        }
    public:
        
        using value_type = std::vector<hid_device_element>::value_type;
        using reference = std::vector<hid_device_element>::const_reference;
        using const_reference = std::vector<hid_device_element>::const_reference;
        using pointer = std::vector<hid_device_element>::const_pointer;
        using const_pointer = std::vector<hid_device_element>::const_pointer;
        
        hid_device_const_elements_enumerator(IOHIDDeviceRef device, uint32_t in_page = kHIDPage_Undefined, uint32_t in_usage_page = 0) :
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
        
    };
    
    
    class hid_device_elements_enumerator : public hid_device_const_elements_enumerator {
    public:
        
        using value_type = std::vector<hid_device_element>::value_type;
        using reference = std::vector<hid_device_element>::reference;
        using const_reference = std::vector<hid_device_element>::const_reference;
        using pointer = std::vector<hid_device_element>::pointer;
        using const_pointer = std::vector<hid_device_element>::const_pointer;
        
        using hid_device_const_elements_enumerator::hid_device_const_elements_enumerator;
        
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
            return copy_cf_string(reinterpret_cast<CFStringRef>(IOHIDDeviceGetProperty(_device, prop_name)));
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
        
        hid_device_elements_enumerator elements(uint32_t in_page = kHIDPage_Undefined, uint32_t in_usage_page = 0) {
            return {_device, in_page, in_usage_page};
        }
        
        hid_device_const_elements_enumerator elements(uint32_t in_page = kHIDPage_Undefined, uint32_t in_usage_page = 0) const {
            return {_device, in_page, in_usage_page};
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
            // No need to call "open", device is already open here
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
