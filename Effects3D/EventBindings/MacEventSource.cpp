// SPDX-License-Identifier: GPL-2.0-only

#include <QtGlobal>
#include "MacEventSource.h"
#include "PluginLog.h"

#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#endif

namespace EffectBinding
{

#ifdef Q_OS_MACOS

namespace
{

void ScreenLockCallback(CFNotificationCenterRef /*center*/,
                        void* observer,
                        CFStringRef name,
                        const void* /*object*/,
                        CFDictionaryRef /*userInfo*/)
{
    auto* self = static_cast<MacEventSource*>(observer);
    if(!self || !name)
    {
        return;
    }
    if(CFStringCompare(name, CFSTR("com.apple.screenIsLocked"), 0) == kCFCompareEqualTo)
    {
        self->NotifyLock(true);
    }
    else if(CFStringCompare(name, CFSTR("com.apple.screenIsUnlocked"), 0) == kCFCompareEqualTo)
    {
        self->NotifyLock(false);
    }
}

void PowerCallback(void* refCon, io_service_t /*service*/, natural_t messageType, void* messageArgument)
{
    auto* self = static_cast<MacEventSource*>(refCon);
    if(!self)
    {
        return;
    }
    const io_connect_t root = static_cast<io_connect_t>(self->power_connection_for_ack());
    switch(messageType)
    {
        case kIOMessageCanSystemSleep:
            IOAllowPowerChange(root, reinterpret_cast<intptr_t>(messageArgument));
            break;
        case kIOMessageSystemWillSleep:
            self->NotifySleep(true);
            IOAllowPowerChange(root, reinterpret_cast<intptr_t>(messageArgument));
            break;
        case kIOMessageSystemHasPoweredOn:
            self->NotifySleep(false);
            break;
        default:
            break;
    }
}

} // namespace

MacEventSource::MacEventSource() = default;

MacEventSource::~MacEventSource()
{
    Stop();
}

std::vector<EventInfo> MacEventSource::ListEvents() const
{
    return {
        {"session_lock", "Screen lock", EventEdge::Level},
        {"session_unlock", "Screen unlock", EventEdge::Level},
        {"prepare_for_sleep", "About to sleep", EventEdge::Level},
        {"resume", "Resumed from sleep", EventEdge::Level},
    };
}

void MacEventSource::NotifyLock(bool locked)
{
    if(locked)
    {
        EmitLevel("session_lock", true);
        EmitLevel("session_unlock", false);
    }
    else
    {
        EmitLevel("session_lock", false);
        EmitLevel("session_unlock", true);
    }
}

void MacEventSource::NotifySleep(bool sleeping)
{
    if(sleeping)
    {
        EmitLevel("prepare_for_sleep", true);
        EmitLevel("resume", false);
    }
    else
    {
        EmitLevel("prepare_for_sleep", false);
        EmitLevel("resume", true);
    }
}

void MacEventSource::Start()
{
    if(started_)
    {
        return;
    }

    CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
    if(center)
    {
        CFNotificationCenterAddObserver(center,
                                        this,
                                        ScreenLockCallback,
                                        CFSTR("com.apple.screenIsLocked"),
                                        nullptr,
                                        CFNotificationSuspensionBehaviorDeliverImmediately);
        CFNotificationCenterAddObserver(center,
                                        this,
                                        ScreenLockCallback,
                                        CFSTR("com.apple.screenIsUnlocked"),
                                        nullptr,
                                        CFNotificationSuspensionBehaviorDeliverImmediately);
        lock_watching_ = true;
    }
    else
    {
        LOG_WARNING("[3DSpatial] macOS events: no distributed notification center");
    }

    IONotificationPortRef port = nullptr;
    io_object_t notifier = IO_OBJECT_NULL;
    io_connect_t connection = IORegisterForSystemPower(this, &port, PowerCallback, &notifier);
    if(connection != MACH_PORT_NULL && port)
    {
        CFRunLoopAddSource(CFRunLoopGetMain(),
                           IONotificationPortGetRunLoopSource(port),
                           kCFRunLoopDefaultMode);
        power_port_ = port;
        power_notifier_ = notifier;
        power_connection_ = connection;
    }
    else
    {
        LOG_WARNING("[3DSpatial] macOS events: IORegisterForSystemPower failed");
        if(port)
        {
            IONotificationPortDestroy(port);
        }
    }

    started_ = true;
    LOG_INFO("[3DSpatial] macOS event source started");
}

void MacEventSource::Stop()
{
    if(lock_watching_)
    {
        CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
        if(center)
        {
            CFNotificationCenterRemoveObserver(center, this, CFSTR("com.apple.screenIsLocked"), nullptr);
            CFNotificationCenterRemoveObserver(center, this, CFSTR("com.apple.screenIsUnlocked"), nullptr);
        }
        lock_watching_ = false;
    }

    if(power_connection_ != 0)
    {
        io_object_t notifier = power_notifier_;
        IODeregisterForSystemPower(&notifier);
        IOServiceClose(static_cast<io_connect_t>(power_connection_));
        power_notifier_ = 0;
        power_connection_ = 0;
    }

    if(power_port_)
    {
        auto* port = static_cast<IONotificationPortRef>(power_port_);
        CFRunLoopRemoveSource(CFRunLoopGetMain(),
                              IONotificationPortGetRunLoopSource(port),
                              kCFRunLoopDefaultMode);
        IONotificationPortDestroy(port);
        power_port_ = nullptr;
    }

    started_ = false;
}

#endif

std::unique_ptr<EventSource> TryCreateMacEventSource()
{
#ifdef Q_OS_MACOS
    return std::make_unique<MacEventSource>();
#else
    return nullptr;
#endif
}

} // namespace EffectBinding
