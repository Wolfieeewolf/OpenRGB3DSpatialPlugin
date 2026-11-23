// SPDX-License-Identifier: GPL-2.0-only

/* Linux and macOS audio capture. Windows uses WASAPI (AudioInputManager_Wasapi.cpp).

   Linux: PulseAudio and PipeWire (through pipewire-pulse) both answer "pactl" and "parec",
   which most desktops install by default. "parec" turns any source into a plain 16-bit mono
   stream, and the ".monitor" sources are the loopback of each output, like WASAPI loopback.
   Using the tools keeps the plugin free of extra libraries OpenRGB does not ship.

   macOS has no built-in loopback, so the list stays empty there for now. */

#ifndef _WIN32

#include "AudioInputManager.h"
#include "PluginLog.h"

#ifdef __linux__
#include <QProcess>
#include <QProcessEnvironment>
#endif

#include <algorithm>
#include <cstring>

#ifdef __linux__
namespace
{
constexpr int kParecRate = 48000;

struct PulseSource
{
    QString name;
    QString description;
    bool    monitor = false;
};

/* Parses "pactl list sources" with the C locale so the field names are always English. */
std::vector<PulseSource> ListPulseSources()
{
    std::vector<PulseSource> sources;

    QProcess pactl;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    pactl.setProcessEnvironment(env);
    pactl.start(QStringLiteral("pactl"), {QStringLiteral("list"), QStringLiteral("sources")});
    if(!pactl.waitForStarted(2000))
    {
        LOG_WARNING("[Audio] \"pactl\" not found: install pulseaudio-utils (or libpulse on Arch) for audio effects");
        return sources;
    }
    if(!pactl.waitForFinished(3000) || pactl.exitCode() != 0)
    {
        pactl.kill();
        pactl.waitForFinished(500);
        LOG_WARNING("[Audio] \"pactl list sources\" failed; is PulseAudio or pipewire-pulse running?");
        return sources;
    }

    const QStringList lines = QString::fromUtf8(pactl.readAllStandardOutput()).split(QLatin1Char('\n'));
    PulseSource current;
    bool in_source = false;
    auto flush = [&]() {
        if(in_source && !current.name.isEmpty())
        {
            if(current.description.isEmpty())
            {
                current.description = current.name;
            }
            sources.push_back(current);
        }
        current   = PulseSource();
        in_source = false;
    };
    for(const QString& raw : lines)
    {
        if(raw.startsWith(QLatin1String("Source #")))
        {
            flush();
            in_source = true;
            continue;
        }
        const QString line = raw.trimmed();
        if(line.startsWith(QLatin1String("Name: ")))
        {
            current.name    = line.mid(6).trimmed();
            current.monitor = current.name.endsWith(QLatin1String(".monitor"));
        }
        else if(line.startsWith(QLatin1String("Description: ")))
        {
            current.description = line.mid(13).trimmed();
        }
    }
    flush();
    return sources;
}
} // namespace

QStringList AudioInputManager::listInputDevices()
{
    QStringList names;
    device_ids.clear();

    const std::vector<PulseSource> sources = ListPulseSources();
    if(sources.empty())
    {
        return names;
    }

    /* Loopback first, like Windows; the first entry follows whatever output is current. */
    names << QStringLiteral("Default output (Loopback)");
    device_ids << QStringLiteral("@DEFAULT_MONITOR@");
    for(const PulseSource& source : sources)
    {
        if(source.monitor)
        {
            QString label = source.description;
            if(label.startsWith(QLatin1String("Monitor of ")))
            {
                label = label.mid(11);
            }
            names << label + QStringLiteral(" (Loopback)");
            device_ids << source.name;
        }
    }
    names << QStringLiteral("Default input");
    device_ids << QStringLiteral("@DEFAULT_SOURCE@");
    for(const PulseSource& source : sources)
    {
        if(!source.monitor)
        {
            names << source.description;
            device_ids << source.name;
        }
    }
    return names;
}

void AudioInputManager::readParec()
{
    if(!parec)
    {
        return;
    }
    parec_pending.append(parec->readAllStandardOutput());
    const int whole = (int)(parec_pending.size() / (int)sizeof(int16_t)) * (int)sizeof(int16_t);
    if(whole <= 0)
    {
        return;
    }
    std::vector<int16_t> samples((size_t)whole / sizeof(int16_t));
    std::memcpy(samples.data(), parec_pending.constData(), (size_t)whole);
    parec_pending.remove(0, whole);
    FeedPCM16(samples.data(), (int)samples.size());
}

void AudioInputManager::start()
{
    QMutexLocker lock(&mutex);
    if(running) return;
    if(selected_index < 0 || selected_index >= device_ids.size()) return;

    parec_pending.clear();
    parec = new QProcess(this);
    parec->setProcessChannelMode(QProcess::SeparateChannels);
    connect(parec, &QProcess::readyReadStandardOutput, this, [this]() { readParec(); });
    connect(parec, &QProcess::errorOccurred, this, [](QProcess::ProcessError error) {
        if(error == QProcess::FailedToStart)
        {
            LOG_WARNING("[Audio] \"parec\" not found: install pulseaudio-utils (or libpulse on Arch) for audio effects");
        }
    });
    connect(parec, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
                if(running)
                {
                    LOG_WARNING("[Audio] parec stopped (exit %d): %s", code,
                                parec ? parec->readAllStandardError().trimmed().constData() : "");
                }
            });

    setSampleRate(kParecRate);
    parec->start(QStringLiteral("parec"),
                 {QStringLiteral("--device=") + device_ids[selected_index],
                  QStringLiteral("--format=s16le"),
                  QStringLiteral("--rate=%1").arg(kParecRate),
                  QStringLiteral("--channels=1"),
                  QStringLiteral("--latency-msec=20"),
                  QStringLiteral("--client-name=OpenRGB 3D Spatial"),
                  QStringLiteral("--stream-name=Audio effects")});

    running = true;
    resetAutoLevel();
    level_timer.start();
}

#else // macOS

QStringList AudioInputManager::listInputDevices()
{
    return QStringList();
}

void AudioInputManager::start()
{
}

#endif

void AudioInputManager::stop()
{
    QMutexLocker lock(&mutex);
    if(!running) return;

    level_timer.stop();
    running = false;

#ifdef __linux__
    if(parec)
    {
        QProcess* process = parec;
        parec = nullptr;
        process->disconnect(this);
        process->kill();
        process->waitForFinished(1000);
        delete process;
    }
    parec_pending.clear();
#endif

    current_level.store(0.0f);
    resetAutoLevel();
    {
        QMutexLocker bl(&bands_mutex);
        std::fill(bands16.begin(), bands16.end(), 0.0f);
        note_peak_count = 0;
        note_drive = 0.0f;
        for(int i = 0; i < kMaxNotePeaks; ++i)
            note_peaks[i] = {};
        if((int)chroma12.size() == kChromaBins)
            std::fill(chroma12.begin(), chroma12.end(), 0.0f);
    }
}

#endif // !_WIN32
