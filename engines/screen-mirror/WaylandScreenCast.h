// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef WAYLANDSCREENCAST_H
#define WAYLANDSCREENCAST_H

#include <QtGlobal>

class WaylandScreenCast;

#if defined(Q_OS_LINUX) && defined(SPATIAL_HAVE_PIPEWIRE)

#include <QImage>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

struct pw_thread_loop;
struct pw_context;
struct pw_core;

/* Screen capture on Wayland: the xdg-desktop-portal ScreenCast interface asks the user which
   monitors to share (once; the restore token keeps the choice), then PipeWire streams the frames.
   libpipewire is loaded at runtime, so the plugin still loads on systems without it.

   Lives on the GUI thread (D-Bus replies arrive there). Capture threads only call
   RequestSession() and TakeFrame(), which are thread-safe. Based on the approach in the
   OpenRGB Effects Plugin's Wayland capturer (GPL-2.0-or-later). */
class WaylandScreenCast : public QObject
{
    Q_OBJECT

public:
    struct Stream;

    /* True when Qt runs on Wayland and libpipewire could be loaded. */
    static bool IsUsable();

    explicit WaylandScreenCast(QObject* parent = nullptr);
    ~WaylandScreenCast() override;

    void SetRestoreToken(const QString& token) { restore_token_ = token; }
    void SetTokenSaver(std::function<void(const QString&)> saver) { token_saver_ = std::move(saver); }
    void SetFrameLimits(int max_width, int max_height, int fps);

    /* Thread-safe. Asks the portal for a session unless one is open, pending or was declined. */
    void RequestSession();

    /* Thread-safe. Copies the newest frame for a screen (QGuiApplication::screens() index) if it
       is newer than `serial`; returns false when there is nothing new. */
    bool TakeFrame(int screen_index, uint64_t& serial, QImage& image) const;

    /* Thread-safe. True once the user declined the share dialog or the portal failed. */
    bool HasFailed() const { return state_.load() == State::Failed; }

private slots:
    void onCreateSessionResponse(uint response, const QVariantMap& results);
    void onSelectSourcesResponse(uint response, const QVariantMap& results);
    void onStartResponse(uint response, const QVariantMap& results);

private:
    enum class State
    {
        Idle,
        Pending,
        Streaming,
        Failed
    };

    void BeginSession();
    QString NextRequestPath(QString& token, const char* response_slot);
    void DropResponseWatch();
    void Fail(const char* what, uint response = 0);
    void OpenPipeWire(const QVariantMap& start_results);
    void StopPipeWire();
    void CloseSession();

    std::atomic<State>              state_{State::Idle};
    QString                         restore_token_;
    std::function<void(const QString&)> token_saver_;
    QString                         session_handle_;
    QString                         watched_request_;
    const char*                     watched_slot_ = nullptr;
    uint64_t                        token_counter_ = 0;

    std::atomic<int>                max_width_{960};
    std::atomic<int>                max_height_{540};
    std::atomic<int>                frame_interval_ms_{33};

    pw_thread_loop*                 loop_ = nullptr;
    pw_context*                     context_ = nullptr;
    pw_core*                        core_ = nullptr;
    std::vector<std::unique_ptr<Stream>> streams_;
};

#endif

#endif
