// SPDX-License-Identifier: GPL-2.0-or-later

#include "WaylandScreenCast.h"

#if defined(Q_OS_LINUX) && defined(SPATIAL_HAVE_PIPEWIRE)

#include "PluginLog.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QGuiApplication>
#include <QMetaObject>
#include <QRect>
#include <QScreen>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>

namespace
{
const QString kPortalService   = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPortalPath      = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kScreenCastIface = QStringLiteral("org.freedesktop.portal.ScreenCast");
const QString kRequestIface    = QStringLiteral("org.freedesktop.portal.Request");

constexpr uint kSourceMonitor  = 1;
constexpr uint kPersistUntilRevoked = 2;

/* libpipewire entry points, resolved at runtime. */
struct PipeWireApi
{
    bool loaded = false;
    decltype(&pw_init)                    init = nullptr;
    decltype(&pw_thread_loop_new)         thread_loop_new = nullptr;
    decltype(&pw_thread_loop_get_loop)    thread_loop_get_loop = nullptr;
    decltype(&pw_thread_loop_start)       thread_loop_start = nullptr;
    decltype(&pw_thread_loop_stop)        thread_loop_stop = nullptr;
    decltype(&pw_thread_loop_destroy)     thread_loop_destroy = nullptr;
    decltype(&pw_thread_loop_lock)        thread_loop_lock = nullptr;
    decltype(&pw_thread_loop_unlock)      thread_loop_unlock = nullptr;
    decltype(&pw_context_new)             context_new = nullptr;
    decltype(&pw_context_destroy)         context_destroy = nullptr;
    decltype(&pw_context_connect_fd)      context_connect_fd = nullptr;
    decltype(&pw_core_disconnect)         core_disconnect = nullptr;
    decltype(&pw_properties_new_string)   properties_new_string = nullptr;
    decltype(&pw_stream_new)              stream_new = nullptr;
    decltype(&pw_stream_add_listener)     stream_add_listener = nullptr;
    decltype(&pw_stream_connect)          stream_connect = nullptr;
    decltype(&pw_stream_destroy)          stream_destroy = nullptr;
    decltype(&pw_stream_dequeue_buffer)   stream_dequeue_buffer = nullptr;
    decltype(&pw_stream_queue_buffer)     stream_queue_buffer = nullptr;
};

template<typename T>
bool Resolve(void* lib, const char* name, T& out)
{
    out = reinterpret_cast<T>(dlsym(lib, name));
    return out != nullptr;
}

const PipeWireApi& PipeWire()
{
    static PipeWireApi api = []() {
        PipeWireApi a;
        /* Never unloaded: libpipewire keeps global state once initialised. */
        void* lib = dlopen("libpipewire-0.3.so.0", RTLD_NOW | RTLD_LOCAL);
        if(!lib)
        {
            LOG_WARNING("[ScreenCapture] libpipewire-0.3 not found; Screen Mirror cannot capture on Wayland");
            return a;
        }
        a.loaded = Resolve(lib, "pw_init", a.init) &&
                   Resolve(lib, "pw_thread_loop_new", a.thread_loop_new) &&
                   Resolve(lib, "pw_thread_loop_get_loop", a.thread_loop_get_loop) &&
                   Resolve(lib, "pw_thread_loop_start", a.thread_loop_start) &&
                   Resolve(lib, "pw_thread_loop_stop", a.thread_loop_stop) &&
                   Resolve(lib, "pw_thread_loop_destroy", a.thread_loop_destroy) &&
                   Resolve(lib, "pw_thread_loop_lock", a.thread_loop_lock) &&
                   Resolve(lib, "pw_thread_loop_unlock", a.thread_loop_unlock) &&
                   Resolve(lib, "pw_context_new", a.context_new) &&
                   Resolve(lib, "pw_context_destroy", a.context_destroy) &&
                   Resolve(lib, "pw_context_connect_fd", a.context_connect_fd) &&
                   Resolve(lib, "pw_core_disconnect", a.core_disconnect) &&
                   Resolve(lib, "pw_properties_new_string", a.properties_new_string) &&
                   Resolve(lib, "pw_stream_new", a.stream_new) &&
                   Resolve(lib, "pw_stream_add_listener", a.stream_add_listener) &&
                   Resolve(lib, "pw_stream_connect", a.stream_connect) &&
                   Resolve(lib, "pw_stream_destroy", a.stream_destroy) &&
                   Resolve(lib, "pw_stream_dequeue_buffer", a.stream_dequeue_buffer) &&
                   Resolve(lib, "pw_stream_queue_buffer", a.stream_queue_buffer);
        if(!a.loaded)
        {
            LOG_WARNING("[ScreenCapture] libpipewire-0.3 is missing functions; Screen Mirror cannot capture on Wayland");
            return a;
        }
        a.init(nullptr, nullptr);
        return a;
    }();
    return api;
}

/* Reads the (ii) struct the portal uses for stream position and size. */
bool ReadIntPair(const QVariant& value, int& a, int& b)
{
    if(!value.canConvert<QDBusArgument>())
    {
        return false;
    }
    const QDBusArgument arg = value.value<QDBusArgument>();
    arg.beginStructure();
    arg >> a >> b;
    arg.endStructure();
    return true;
}
} // namespace

struct WaylandScreenCast::Stream
{
    WaylandScreenCast*   owner = nullptr;
    uint32_t             node_id = 0;
    QRect                rect;
    bool                 has_position = false;
    int                  screen_index = -1;

    pw_stream*           stream = nullptr;
    spa_hook             listener{};
    pw_stream_events     events{};
    spa_video_info_raw   format{};
    std::chrono::steady_clock::time_point last_frame{};

    mutable std::mutex   frame_mutex;
    QImage               frame;
    uint64_t             frame_serial = 0;

    static void OnParamChanged(void* data, uint32_t id, const spa_pod* param)
    {
        Stream* self = static_cast<Stream*>(data);
        if(param && id == SPA_PARAM_Format)
        {
            spa_video_info_raw info{};
            if(spa_format_video_raw_parse(param, &info) >= 0)
            {
                self->format = info;
            }
        }
    }

    static void OnProcess(void* data)
    {
        Stream* self = static_cast<Stream*>(data);
        const PipeWireApi& pw = PipeWire();
        /* Keep only the newest queued buffer. */
        pw_buffer* newest = nullptr;
        while(pw_buffer* next = pw.stream_dequeue_buffer(self->stream))
        {
            if(newest)
            {
                pw.stream_queue_buffer(self->stream, newest);
            }
            newest = next;
        }
        if(!newest)
        {
            return;
        }
        self->Consume(newest->buffer);
        pw.stream_queue_buffer(self->stream, newest);
    }

    void Consume(const spa_buffer* buffer)
    {
        if(!buffer || buffer->n_datas < 1)
        {
            return;
        }
        const spa_data& d = buffer->datas[0];
        const int width  = (int)format.size.width;
        const int height = (int)format.size.height;
        if(!d.data || !d.chunk || d.chunk->size == 0 || width <= 0 || height <= 0)
        {
            return;
        }

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if(now - last_frame < std::chrono::milliseconds(owner->frame_interval_ms_.load()))
        {
            return;
        }

        QImage::Format qt_format;
        switch(format.format)
        {
            case SPA_VIDEO_FORMAT_BGRx:
            case SPA_VIDEO_FORMAT_BGRA:
                qt_format = QImage::Format_RGB32;
                break;
            case SPA_VIDEO_FORMAT_RGBx:
            case SPA_VIDEO_FORMAT_RGBA:
                qt_format = QImage::Format_RGBX8888;
                break;
            default:
                return;
        }

        const int stride = d.chunk->stride > 0 ? d.chunk->stride : width * 4;
        const uint64_t needed = (uint64_t)d.chunk->offset + (uint64_t)stride * (uint64_t)height;
        if(stride < width * 4 || needed > d.maxsize)
        {
            return;
        }
        const uchar* pixels = static_cast<const uchar*>(d.data) + d.chunk->offset;
        const QImage wrapped(pixels, width, height, stride, qt_format);

        /* Both branches deep-copy, so the buffer can go back to PipeWire straight after. */
        const int max_w = owner->max_width_.load();
        const int max_h = owner->max_height_.load();
        QImage copy = (width > max_w || height > max_h)
                          ? wrapped.scaled(std::min(width, max_w), std::min(height, max_h),
                                           Qt::IgnoreAspectRatio, Qt::FastTransformation)
                                .convertToFormat(QImage::Format_RGBA8888)
                          : wrapped.convertToFormat(QImage::Format_RGBA8888);
        if(copy.isNull())
        {
            return;
        }
        last_frame = now;
        std::lock_guard<std::mutex> lock(frame_mutex);
        frame = std::move(copy);
        frame_serial++;
    }
};

bool WaylandScreenCast::IsUsable()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland")) && PipeWire().loaded;
}

WaylandScreenCast::WaylandScreenCast(QObject* parent)
    : QObject(parent)
{
}

WaylandScreenCast::~WaylandScreenCast()
{
    DropResponseWatch();
    StopPipeWire();
    CloseSession();
}

void WaylandScreenCast::SetFrameLimits(int max_width, int max_height, int fps)
{
    max_width_.store(std::max(16, max_width));
    max_height_.store(std::max(16, max_height));
    frame_interval_ms_.store(1000 / std::clamp(fps, 1, 240));
}

void WaylandScreenCast::RequestSession()
{
    State expected = State::Idle;
    if(!state_.compare_exchange_strong(expected, State::Pending))
    {
        return;
    }
    QMetaObject::invokeMethod(this, [this]() { BeginSession(); }, Qt::QueuedConnection);
}

bool WaylandScreenCast::TakeFrame(int screen_index, uint64_t& serial, QImage& image) const
{
    if(state_.load() != State::Streaming)
    {
        return false;
    }
    /* streams_ only changes on the GUI thread while no capture thread runs (see the manager). */
    for(const std::unique_ptr<Stream>& s : streams_)
    {
        if(s->screen_index != screen_index)
        {
            continue;
        }
        std::lock_guard<std::mutex> lock(s->frame_mutex);
        if(s->frame.isNull() || s->frame_serial == serial)
        {
            return false;
        }
        serial = s->frame_serial;
        image  = s->frame;
        return true;
    }
    return false;
}

QString WaylandScreenCast::NextRequestPath(QString& token, const char* response_slot)
{
    DropResponseWatch();
    token = QStringLiteral("spatial3d%1").arg(++token_counter_);
    /* The portal names the request object after our bus name and the token. Subscribing before
       the call means a reply that comes back at once is never missed. */
    QString sender = QDBusConnection::sessionBus().baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    watched_request_ = kPortalPath + QStringLiteral("/request/") + sender + QLatin1Char('/') + token;
    watched_slot_    = response_slot;
    QDBusConnection::sessionBus().connect(kPortalService, watched_request_, kRequestIface,
                                          QStringLiteral("Response"), this, response_slot);
    return watched_request_;
}

void WaylandScreenCast::DropResponseWatch()
{
    if(!watched_request_.isEmpty() && watched_slot_)
    {
        QDBusConnection::sessionBus().disconnect(kPortalService, watched_request_, kRequestIface,
                                                 QStringLiteral("Response"), this, watched_slot_);
    }
    watched_request_.clear();
    watched_slot_ = nullptr;
}

void WaylandScreenCast::Fail(const char* what, uint response)
{
    DropResponseWatch();
    StopPipeWire();
    CloseSession();
    state_.store(State::Failed);
    if(response == 1)
    {
        LOG_INFO("[ScreenCapture] Screen sharing was declined; Screen Mirror stays dark until it is started again");
    }
    else
    {
        LOG_WARNING("[ScreenCapture] Wayland screen sharing failed: %s (response %u)", what, response);
    }
}

void WaylandScreenCast::BeginSession()
{
    QString token;
    NextRequestPath(token, SLOT(onCreateSessionResponse(uint,QVariantMap)));
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("session_handle_token"), QStringLiteral("spatial3d%1").arg(++token_counter_));

    QDBusMessage call = QDBusMessage::createMethodCall(kPortalService, kPortalPath, kScreenCastIface,
                                                       QStringLiteral("CreateSession"));
    call << options;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call);
    if(reply.type() == QDBusMessage::ErrorMessage)
    {
        LOG_WARNING("[ScreenCapture] ScreenCast portal unavailable: %s", qPrintable(reply.errorMessage()));
        Fail("CreateSession");
    }
}

void WaylandScreenCast::onCreateSessionResponse(uint response, const QVariantMap& results)
{
    if(response != 0)
    {
        Fail("CreateSession", response);
        return;
    }
    session_handle_ = results.value(QStringLiteral("session_handle")).toString();
    if(session_handle_.isEmpty())
    {
        Fail("CreateSession returned no session");
        return;
    }

    QString token;
    NextRequestPath(token, SLOT(onSelectSourcesResponse(uint,QVariantMap)));
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("types"), kSourceMonitor);
    options.insert(QStringLiteral("multiple"), true);
    options.insert(QStringLiteral("persist_mode"), kPersistUntilRevoked);
    if(!restore_token_.isEmpty())
    {
        options.insert(QStringLiteral("restore_token"), restore_token_);
    }

    QDBusMessage call = QDBusMessage::createMethodCall(kPortalService, kPortalPath, kScreenCastIface,
                                                       QStringLiteral("SelectSources"));
    call << QVariant::fromValue(QDBusObjectPath(session_handle_)) << options;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call);
    if(reply.type() == QDBusMessage::ErrorMessage)
    {
        Fail("SelectSources");
    }
}

void WaylandScreenCast::onSelectSourcesResponse(uint response, const QVariantMap& /*results*/)
{
    if(response != 0)
    {
        Fail("SelectSources", response);
        return;
    }

    QString token;
    NextRequestPath(token, SLOT(onStartResponse(uint,QVariantMap)));
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);

    QDBusMessage call = QDBusMessage::createMethodCall(kPortalService, kPortalPath, kScreenCastIface,
                                                       QStringLiteral("Start"));
    call << QVariant::fromValue(QDBusObjectPath(session_handle_)) << QString() << options;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call);
    if(reply.type() == QDBusMessage::ErrorMessage)
    {
        Fail("Start");
    }
}

void WaylandScreenCast::onStartResponse(uint response, const QVariantMap& results)
{
    DropResponseWatch();
    if(response != 0)
    {
        Fail("Start", response);
        return;
    }

    /* A token works once; each successful start hands back the next one. */
    const QString token = results.value(QStringLiteral("restore_token")).toString();
    if(!token.isEmpty())
    {
        restore_token_ = token;
        if(token_saver_)
        {
            token_saver_(token);
        }
    }
    OpenPipeWire(results);
}

void WaylandScreenCast::OpenPipeWire(const QVariantMap& start_results)
{
    /* streams: a(ua{sv}) — node id plus position and size in the desktop's logical layout. */
    std::vector<std::unique_ptr<Stream>> found;
    const QVariant streams_value = start_results.value(QStringLiteral("streams"));
    if(streams_value.canConvert<QDBusArgument>())
    {
        const QDBusArgument arg = streams_value.value<QDBusArgument>();
        arg.beginArray();
        while(!arg.atEnd())
        {
            std::unique_ptr<Stream> s = std::make_unique<Stream>();
            QVariantMap props;
            arg.beginStructure();
            arg >> s->node_id >> props;
            arg.endStructure();
            int x = 0, y = 0, w = 0, h = 0;
            s->has_position = ReadIntPair(props.value(QStringLiteral("position")), x, y);
            ReadIntPair(props.value(QStringLiteral("size")), w, h);
            s->rect = QRect(x, y, w, h);
            found.push_back(std::move(s));
        }
        arg.endArray();
    }
    if(found.empty())
    {
        Fail("no monitors were shared");
        return;
    }

    /* Match each shared monitor to a QScreen: by position first, then in order for the rest. */
    const QList<QScreen*> screens = QGuiApplication::screens();
    std::vector<bool> screen_taken((size_t)screens.size(), false);
    for(std::unique_ptr<Stream>& s : found)
    {
        if(!s->has_position)
        {
            continue;
        }
        for(int i = 0; i < screens.size(); i++)
        {
            if(!screen_taken[(size_t)i] && screens[i]->geometry().topLeft() == s->rect.topLeft())
            {
                s->screen_index = i;
                screen_taken[(size_t)i] = true;
                break;
            }
        }
    }
    for(std::unique_ptr<Stream>& s : found)
    {
        for(int i = 0; s->screen_index < 0 && i < screens.size(); i++)
        {
            if(!screen_taken[(size_t)i])
            {
                s->screen_index = i;
                screen_taken[(size_t)i] = true;
            }
        }
    }

    QDBusMessage call = QDBusMessage::createMethodCall(kPortalService, kPortalPath, kScreenCastIface,
                                                       QStringLiteral("OpenPipeWireRemote"));
    call << QVariant::fromValue(QDBusObjectPath(session_handle_)) << QVariantMap();
    const QDBusReply<QDBusUnixFileDescriptor> reply = QDBusConnection::sessionBus().call(call);
    if(!reply.isValid() || !reply.value().isValid())
    {
        Fail("OpenPipeWireRemote");
        return;
    }
    /* PipeWire takes ownership of the fd it is given; the D-Bus object closes its own copy. */
    const int fd = fcntl(reply.value().fileDescriptor(), F_DUPFD_CLOEXEC, 3);
    if(fd < 0)
    {
        Fail("could not duplicate the PipeWire fd");
        return;
    }

    const PipeWireApi& pw = PipeWire();
    loop_ = pw.thread_loop_new("spatial3d-screencast", nullptr);
    if(!loop_)
    {
        close(fd);
        Fail("pw_thread_loop_new");
        return;
    }
    context_ = pw.context_new(pw.thread_loop_get_loop(loop_), nullptr, 0);
    core_ = context_ ? pw.context_connect_fd(context_, fd, nullptr, 0) : nullptr;
    if(!core_)
    {
        if(!context_)
        {
            close(fd);
        }
        Fail("could not connect to PipeWire");
        return;
    }

    uint8_t pod_buffer[1024];
    for(std::unique_ptr<Stream>& s : found)
    {
        s->owner = this;
        s->events.version = PW_VERSION_STREAM_EVENTS;
        s->events.param_changed = &Stream::OnParamChanged;
        s->events.process = &Stream::OnProcess;
        s->stream = pw.stream_new(core_, "OpenRGB 3D Spatial Screen Mirror",
                                  pw.properties_new_string("media.type=Video media.category=Capture media.role=Screen"));
        if(!s->stream)
        {
            continue;
        }
        pw.stream_add_listener(s->stream, &s->listener, &s->events, s.get());

        spa_pod_builder builder;
        std::memset(&builder, 0, sizeof(builder));
        builder.data = pod_buffer;
        builder.size = sizeof(pod_buffer);

        spa_rectangle def_size = SPA_RECTANGLE((uint32_t)std::max(1, s->rect.width()),
                                               (uint32_t)std::max(1, s->rect.height()));
        spa_rectangle min_size = SPA_RECTANGLE(1, 1);
        spa_rectangle max_size = SPA_RECTANGLE(16384, 16384);
        spa_fraction def_rate = SPA_FRACTION(30, 1);
        spa_fraction min_rate = SPA_FRACTION(0, 1);
        spa_fraction max_rate = SPA_FRACTION(240, 1);

        /* Shared-memory formats only (no DMA-BUF modifiers), so buffers can be read directly. */
        spa_pod_frame frame;
        spa_pod_builder_push_object(&builder, &frame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
        spa_pod_builder_add(&builder, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), 0);
        spa_pod_builder_add(&builder, SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), 0);
        spa_pod_builder_add(&builder, SPA_FORMAT_VIDEO_format,
                            SPA_POD_CHOICE_ENUM_Id(5, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx,
                                                   SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBx,
                                                   SPA_VIDEO_FORMAT_RGBA), 0);
        spa_pod_builder_add(&builder, SPA_FORMAT_VIDEO_size,
                            SPA_POD_CHOICE_RANGE_Rectangle(&def_size, &min_size, &max_size), 0);
        spa_pod_builder_add(&builder, SPA_FORMAT_VIDEO_framerate,
                            SPA_POD_CHOICE_RANGE_Fraction(&def_rate, &min_rate, &max_rate), 0);
        const spa_pod* params[1] = {static_cast<const spa_pod*>(spa_pod_builder_pop(&builder, &frame))};

        if(pw.stream_connect(s->stream, SPA_DIRECTION_INPUT, s->node_id,
                             (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
                             params, 1) != 0)
        {
            LOG_WARNING("[ScreenCapture] Could not connect to PipeWire node %u", s->node_id);
        }
    }
    streams_ = std::move(found);

    if(pw.thread_loop_start(loop_) < 0)
    {
        Fail("pw_thread_loop_start");
        return;
    }
    state_.store(State::Streaming);
    LOG_INFO("[ScreenCapture] Wayland screen sharing started (%d monitor(s))", (int)streams_.size());
}

void WaylandScreenCast::StopPipeWire()
{
    const PipeWireApi& pw = PipeWire();
    if(state_.load() == State::Streaming)
    {
        state_.store(State::Idle);
    }
    if(loop_)
    {
        /* Stopping joins the loop thread, so no callback runs after this point. */
        pw.thread_loop_stop(loop_);
    }
    for(std::unique_ptr<Stream>& s : streams_)
    {
        if(s->stream)
        {
            pw.stream_destroy(s->stream);
            s->stream = nullptr;
        }
    }
    streams_.clear();
    if(core_)
    {
        pw.core_disconnect(core_);
        core_ = nullptr;
    }
    if(context_)
    {
        pw.context_destroy(context_);
        context_ = nullptr;
    }
    if(loop_)
    {
        pw.thread_loop_destroy(loop_);
        loop_ = nullptr;
    }
}

void WaylandScreenCast::CloseSession()
{
    if(session_handle_.isEmpty())
    {
        return;
    }
    if(QCoreApplication::instance())
    {
        QDBusMessage call = QDBusMessage::createMethodCall(kPortalService, session_handle_,
                                                           QStringLiteral("org.freedesktop.portal.Session"),
                                                           QStringLiteral("Close"));
        QDBusConnection::sessionBus().call(call, QDBus::NoBlock);
    }
    session_handle_.clear();
}

#endif
