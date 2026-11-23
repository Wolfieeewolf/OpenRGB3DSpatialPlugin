// SPDX-License-Identifier: GPL-2.0-only

#ifdef __linux__

#include "ReactiveInputLinux.h"
#include "PluginLog.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>

#include <linux/input.h>

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ReactiveLinuxInput
{
namespace
{

struct KeyCode
{
    std::uint8_t vk = 0;
    std::uint8_t make = 0;
    bool extended = false;
    bool pause = false;
};

/* Linux key code -> Windows virtual key + set-1 scan code. Codes 1-88 are the scan code. */
bool TranslateKey(unsigned int code, KeyCode* out)
{
    static const std::uint8_t kVkByCode[89] = {
        0,
        0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0xBD, 0xBB, 0x08, 0x09,   /* 1-15 */
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0xDB, 0xDD, 0x0D, 0xA2,         /* 16-29 */
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xBA, 0xDE, 0xC0, 0xA0, 0xDC,        /* 30-43 */
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', 0xBC, 0xBE, 0xBF, 0xA1, 0x6A, 0xA4, 0x20, 0x14, /* 44-58 */
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,                       /* 59-68 F1-F10 */
        0x90, 0x91,                                                                       /* 69-70 */
        0x67, 0x68, 0x69, 0x6D, 0x64, 0x65, 0x66, 0x6B, 0x61, 0x62, 0x63, 0x60, 0x6E,     /* 71-83 keypad */
        0, 0, 0xE2, 0x7A, 0x7B,                                                           /* 84-88 */
    };
    *out = KeyCode();
    if(code >= 1 && code <= 88)
    {
        if(kVkByCode[code] == 0)
        {
            return false;
        }
        out->vk = kVkByCode[code];
        out->make = static_cast<std::uint8_t>(code);
        return true;
    }
    struct Extra
    {
        unsigned int code;
        std::uint8_t vk;
        std::uint8_t make;
        bool extended;
    };
    static const Extra kExtra[] = {
        {KEY_KPENTER, 0x0D, 0x1C, true},  {KEY_RIGHTCTRL, 0xA3, 0x1D, true},
        {KEY_KPSLASH, 0x6F, 0x35, true},  {KEY_SYSRQ, 0x2C, 0x37, true},
        {KEY_RIGHTALT, 0xA5, 0x38, true}, {KEY_HOME, 0x24, 0x47, true},
        {KEY_UP, 0x26, 0x48, true},       {KEY_PAGEUP, 0x21, 0x49, true},
        {KEY_LEFT, 0x25, 0x4B, true},     {KEY_RIGHT, 0x27, 0x4D, true},
        {KEY_END, 0x23, 0x4F, true},      {KEY_DOWN, 0x28, 0x50, true},
        {KEY_PAGEDOWN, 0x22, 0x51, true}, {KEY_INSERT, 0x2D, 0x52, true},
        {KEY_DELETE, 0x2E, 0x53, true},   {KEY_LEFTMETA, 0x5B, 0x5B, true},
        {KEY_RIGHTMETA, 0x5C, 0x5C, true}, {KEY_COMPOSE, 0x5D, 0x5D, true},
        {KEY_MUTE, 0xAD, 0, false},       {KEY_VOLUMEDOWN, 0xAE, 0, false},
        {KEY_VOLUMEUP, 0xAF, 0, false},   {KEY_NEXTSONG, 0xB0, 0, false},
        {KEY_PREVIOUSSONG, 0xB1, 0, false}, {KEY_STOPCD, 0xB2, 0, false},
        {KEY_PLAYPAUSE, 0xB3, 0, false},
    };
    for(const Extra& e : kExtra)
    {
        if(e.code == code)
        {
            out->vk = e.vk;
            out->make = e.make;
            out->extended = e.extended;
            return true;
        }
    }
    if(code == KEY_PAUSE)
    {
        out->vk = 0x13;
        out->pause = true;
        return true;
    }
    return false;
}

int MouseButton(unsigned int code)
{
    switch(code)
    {
    case BTN_LEFT:    return 1;
    case BTN_RIGHT:   return 2;
    case BTN_MIDDLE:  return 3;
    case BTN_SIDE:
    case BTN_BACK:    return 4;
    case BTN_EXTRA:
    case BTN_FORWARD: return 5;
    default:          return 0;
    }
}

/* Same bits as XInput wButtons; 0x0400 / 0x0800 are Reactive's trigger bits. */
std::uint16_t GamepadBit(unsigned int code)
{
    switch(code)
    {
    case BTN_SOUTH:      return 0x1000;
    case BTN_EAST:       return 0x2000;
    case BTN_NORTH:      return 0x4000;
    case BTN_WEST:       return 0x8000;
    case BTN_TL:         return 0x0100;
    case BTN_TR:         return 0x0200;
    case BTN_TL2:        return 0x0400;
    case BTN_TR2:        return 0x0800;
    case BTN_SELECT:     return 0x0020;
    case BTN_START:      return 0x0010;
    case BTN_THUMBL:     return 0x0040;
    case BTN_THUMBR:     return 0x0080;
    case BTN_DPAD_UP:    return 0x0001;
    case BTN_DPAD_DOWN:  return 0x0002;
    case BTN_DPAD_LEFT:  return 0x0004;
    case BTN_DPAD_RIGHT: return 0x0008;
    default:             return 0;
    }
}

bool TestBit(const std::vector<unsigned long>& bits, unsigned int bit)
{
    const size_t per = sizeof(unsigned long) * 8;
    return bit / per < bits.size() && (bits[bit / per] >> (bit % per)) & 1ul;
}

/* Asks logind whether the user's session is locked, so the lock-screen password is never
   read. Windows and macOS keep lock-screen typing away from other apps on their own. */
class LockWatch
{
public:
    bool Locked()
    {
        if(path_.isEmpty() && !Resolve())
        {
            return false;
        }
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
                                                          path_,
                                                          QStringLiteral("org.freedesktop.DBus.Properties"),
                                                          QStringLiteral("Get"));
        msg << QStringLiteral("org.freedesktop.login1.Session") << QStringLiteral("LockedHint");
        const QDBusMessage reply = QDBusConnection::systemBus().call(msg, QDBus::Block, 300);
        if(reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        {
            return false;
        }
        return reply.arguments().first().value<QDBusVariant>().variant().toBool();
    }

private:
    bool Resolve()
    {
        if(tried_)
        {
            return false;
        }
        tried_ = true;
        QDBusMessage by_pid = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
                                                             QStringLiteral("/org/freedesktop/login1"),
                                                             QStringLiteral("org.freedesktop.login1.Manager"),
                                                             QStringLiteral("GetSessionByPID"));
        by_pid << static_cast<uint>(getpid());
        QDBusMessage reply = QDBusConnection::systemBus().call(by_pid, QDBus::Block, 500);
        if(reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        {
            /* Autostarted OpenRGB belongs to no session; "auto" is the user's desktop session. */
            QDBusMessage by_auto = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
                                                                  QStringLiteral("/org/freedesktop/login1"),
                                                                  QStringLiteral("org.freedesktop.login1.Manager"),
                                                                  QStringLiteral("GetSession"));
            by_auto << QStringLiteral("auto");
            reply = QDBusConnection::systemBus().call(by_auto, QDBus::Block, 500);
        }
        if(reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        {
            LOG_WARNING("[Reactive] No logind session found; can't pause input while the screen is locked");
            return false;
        }
        path_ = reply.arguments().first().value<QDBusObjectPath>().path();
        return !path_.isEmpty();
    }

    QString path_;
    bool tried_ = false;
};

struct Device
{
    int fd = -1;
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    bool gamepad = false;
    std::uint32_t pad = 0;
    int trigger_left_axis = -1;
    int trigger_right_axis = -1;
    int trigger_left_threshold = 0;
    int trigger_right_threshold = 0;
    bool trigger_left = false;
    bool trigger_right = false;
    int hat_x = 0;
    int hat_y = 0;
    std::set<std::uint64_t> held; /* (kind << 32) | payload, released if the device goes away */
};

int TriggerThreshold(int fd, int axis)
{
    input_absinfo info{};
    if(ioctl(fd, EVIOCGABS(axis), &info) != 0 || info.maximum <= info.minimum)
    {
        return -1;
    }
    /* Same 12% as the Windows XInput path (30 of 255). */
    return info.minimum + (info.maximum - info.minimum) * 12 / 100;
}

class Session
{
public:
    Session(const Reader::Sink& sink, bool keyboard, bool mouse, bool gamepad)
        : sink_(sink), want_keyboard_(keyboard), want_mouse_(mouse), want_gamepad_(gamepad)
    {
    }

    ~Session()
    {
        for(auto& item : devices_)
        {
            ReleaseAll(item.second);
            close(item.second.fd);
        }
    }

    void Rescan()
    {
        DIR* dir = opendir("/dev/input");
        if(!dir)
        {
            return;
        }
        int denied = 0;
        while(dirent* entry = readdir(dir))
        {
            if(std::strncmp(entry->d_name, "event", 5) != 0)
            {
                continue;
            }
            const std::string path = std::string("/dev/input/") + entry->d_name;
            if(devices_.count(path) || ignored_.count(path))
            {
                continue;
            }
            const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if(fd < 0)
            {
                if(errno == EACCES || errno == EPERM)
                {
                    ++denied;
                }
                continue;
            }
            Device device;
            if(!Describe(fd, &device))
            {
                close(fd);
                ignored_.insert(path);
                continue;
            }
            devices_[path] = device;
        }
        closedir(dir);

        if(devices_.empty() && denied > 0 && !warned_)
        {
            LOG_WARNING("[Reactive] Can't read keyboard/mouse input: add your user to the \"input\" group "
                        "(sudo usermod -aG input $USER), then log out and back in");
            warned_ = true;
        }
    }

    /* Closes every device (releasing held keys); Rescan opens them again. */
    void CloseAll()
    {
        for(auto& item : devices_)
        {
            ReleaseAll(item.second);
            close(item.second.fd);
        }
        devices_.clear();
    }

    void Poll(int timeout_ms)
    {
        std::vector<pollfd> fds;
        std::vector<std::string> paths;
        for(const auto& item : devices_)
        {
            fds.push_back(pollfd{item.second.fd, POLLIN, 0});
            paths.push_back(item.first);
        }
        if(fds.empty())
        {
            poll(nullptr, 0, timeout_ms);
            return;
        }
        if(poll(fds.data(), fds.size(), timeout_ms) <= 0)
        {
            return;
        }
        for(size_t i = 0; i < fds.size(); ++i)
        {
            if(fds[i].revents == 0)
            {
                continue;
            }
            Device& device = devices_[paths[i]];
            if(!ReadEvents(device) || (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)))
            {
                /* Unplugged: release what it held so nothing stays "pressed". */
                ReleaseAll(device);
                close(device.fd);
                devices_.erase(paths[i]);
            }
        }
    }

private:
    bool Describe(int fd, Device* device)
    {
        std::vector<unsigned long> keys((KEY_MAX + 1) / (sizeof(unsigned long) * 8) + 1, 0);
        if(ioctl(fd, EVIOCGBIT(EV_KEY, keys.size() * sizeof(unsigned long)), keys.data()) < 0)
        {
            return false;
        }
        const bool keyboard = TestBit(keys, KEY_A) && TestBit(keys, KEY_SPACE);
        const bool media = TestBit(keys, KEY_VOLUMEUP) || TestBit(keys, KEY_PLAYPAUSE);
        const bool mouse = TestBit(keys, BTN_LEFT);
        device->gamepad = TestBit(keys, BTN_SOUTH) || TestBit(keys, BTN_GAMEPAD);
        /* Open only devices of a kind someone listens to. */
        const bool wanted = (want_keyboard_ && (keyboard || media)) || (want_mouse_ && mouse)
                            || (want_gamepad_ && device->gamepad);
        if(!wanted)
        {
            return false;
        }
        input_id id{};
        if(ioctl(fd, EVIOCGID, &id) == 0)
        {
            device->vid = id.vendor;
            device->pid = id.product;
        }
        device->fd = fd;
        if(device->gamepad)
        {
            device->pad = next_pad_++ % 4u;
            std::vector<unsigned long> abs((ABS_MAX + 1) / (sizeof(unsigned long) * 8) + 1, 0);
            ioctl(fd, EVIOCGBIT(EV_ABS, abs.size() * sizeof(unsigned long)), abs.data());
            /* Xbox (xpad) and PlayStation pads report the triggers on Z / RZ;
               Xbox over Bluetooth uses BRAKE / GAS. */
            const int left = TestBit(abs, ABS_BRAKE) ? ABS_BRAKE : (TestBit(abs, ABS_Z) ? ABS_Z : -1);
            const int right = TestBit(abs, ABS_GAS) ? ABS_GAS : (TestBit(abs, ABS_RZ) ? ABS_RZ : -1);
            if(left >= 0)
            {
                device->trigger_left_threshold = TriggerThreshold(fd, left);
                device->trigger_left_axis = device->trigger_left_threshold >= 0 ? left : -1;
            }
            if(right >= 0)
            {
                device->trigger_right_threshold = TriggerThreshold(fd, right);
                device->trigger_right_axis = device->trigger_right_threshold >= 0 ? right : -1;
            }
        }
        return true;
    }

    void Send(Device& device, ReactiveSourceKind kind, std::uint32_t payload, bool down)
    {
        const std::uint64_t key = (static_cast<std::uint64_t>(kind) << 32) | payload;
        if(down)
        {
            device.held.insert(key);
        }
        else
        {
            device.held.erase(key);
        }
        Edge edge;
        edge.kind = kind;
        edge.payload = payload;
        edge.pad = (kind == ReactiveSourceKind::Gamepad) ? device.pad : 0;
        edge.down = down;
        edge.vid = device.vid;
        edge.pid = device.pid;
        sink_(edge);
    }

    void ReleaseAll(Device& device)
    {
        const std::set<std::uint64_t> held = device.held;
        for(std::uint64_t key : held)
        {
            Send(device, static_cast<ReactiveSourceKind>(key >> 32), static_cast<std::uint32_t>(key), false);
        }
    }

    void HandleKey(Device& device, unsigned int code, int value)
    {
        if(value == 2)
        {
            return; /* auto-repeat */
        }
        const bool down = (value != 0);
        if(const int button = MouseButton(code))
        {
            if(want_mouse_)
            {
                Send(device, ReactiveSourceKind::Mouse, static_cast<std::uint32_t>(button), down);
            }
            return;
        }
        if(device.gamepad && want_gamepad_)
        {
            if(const std::uint16_t bit = GamepadBit(code))
            {
                Send(device, ReactiveSourceKind::Gamepad, bit, down);
                return;
            }
        }
        KeyCode key;
        if(want_keyboard_ && TranslateKey(code, &key))
        {
            const std::uint32_t payload = key.vk | (static_cast<std::uint32_t>(key.make) << 8) |
                                          (key.extended ? 0x10000u : 0u) | (key.pause ? 0x20000u : 0u);
            Send(device, ReactiveSourceKind::Keyboard, payload, down);
        }
    }

    void HandleAbs(Device& device, unsigned int code, int value)
    {
        if(!device.gamepad || !want_gamepad_)
        {
            return;
        }
        if(code == ABS_HAT0X || code == ABS_HAT0Y)
        {
            int& last = (code == ABS_HAT0X) ? device.hat_x : device.hat_y;
            const std::uint16_t minus = (code == ABS_HAT0X) ? 0x0004 : 0x0001;
            const std::uint16_t plus  = (code == ABS_HAT0X) ? 0x0008 : 0x0002;
            const int now = (value < 0) ? -1 : (value > 0 ? 1 : 0);
            if(now == last)
            {
                return;
            }
            if(last < 0) Send(device, ReactiveSourceKind::Gamepad, minus, false);
            if(last > 0) Send(device, ReactiveSourceKind::Gamepad, plus, false);
            if(now < 0)  Send(device, ReactiveSourceKind::Gamepad, minus, true);
            if(now > 0)  Send(device, ReactiveSourceKind::Gamepad, plus, true);
            last = now;
            return;
        }
        if((int)code == device.trigger_left_axis)
        {
            const bool on = value > device.trigger_left_threshold;
            if(on != device.trigger_left)
            {
                device.trigger_left = on;
                Send(device, ReactiveSourceKind::Gamepad, 0x0400, on);
            }
        }
        else if((int)code == device.trigger_right_axis)
        {
            const bool on = value > device.trigger_right_threshold;
            if(on != device.trigger_right)
            {
                device.trigger_right = on;
                Send(device, ReactiveSourceKind::Gamepad, 0x0800, on);
            }
        }
    }

    /* False when the device is gone. */
    bool ReadEvents(Device& device)
    {
        input_event events[64];
        for(;;)
        {
            const ssize_t got = read(device.fd, events, sizeof(events));
            if(got < 0)
            {
                return errno == EAGAIN || errno == EINTR;
            }
            if(got == 0)
            {
                return false;
            }
            const size_t count = static_cast<size_t>(got) / sizeof(input_event);
            for(size_t i = 0; i < count; ++i)
            {
                const input_event& ev = events[i];
                if(ev.type == EV_KEY)
                {
                    HandleKey(device, ev.code, ev.value);
                }
                else if(ev.type == EV_ABS)
                {
                    HandleAbs(device, ev.code, ev.value);
                }
            }
        }
    }

    const Reader::Sink& sink_;
    std::map<std::string, Device> devices_;
    std::set<std::string> ignored_;
    std::uint32_t next_pad_ = 0;
    bool warned_ = false;
    bool want_keyboard_ = true;
    bool want_mouse_ = true;
    bool want_gamepad_ = true;
};

} // namespace

Reader::~Reader()
{
    Stop();
}

void Reader::Start(Sink sink, bool keyboard, bool mouse, bool gamepad)
{
    if(running_.exchange(true))
    {
        return;
    }
    sink_ = std::move(sink);
    keyboard_ = keyboard;
    mouse_ = mouse;
    gamepad_ = gamepad;
    thread_ = std::thread(&Reader::Run, this);
}

void Reader::Stop()
{
    running_.store(false);
    if(thread_.joinable())
    {
        thread_.join();
    }
}

void Reader::Run()
{
    Session session(sink_, keyboard_, mouse_, gamepad_);
    LockWatch lock;
    bool locked = false;
    auto next_scan = std::chrono::steady_clock::time_point();
    auto next_lock_check = std::chrono::steady_clock::time_point();
    while(running_.load())
    {
        const auto now = std::chrono::steady_clock::now();
        if(now >= next_lock_check)
        {
            const bool now_locked = lock.Locked();
            if(now_locked && !locked)
            {
                session.CloseAll();
            }
            if(!now_locked && locked)
            {
                next_scan = now;
            }
            locked = now_locked;
            next_lock_check = now + std::chrono::milliseconds(250);
        }
        if(locked)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if(now >= next_scan)
        {
            /* Picks up devices plugged in while the effect runs. */
            session.Rescan();
            next_scan = now + std::chrono::seconds(2);
        }
        session.Poll(100);
    }
}

}

#endif
