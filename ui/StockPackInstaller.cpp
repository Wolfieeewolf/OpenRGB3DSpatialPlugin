// SPDX-License-Identifier: GPL-2.0-only

#include "StockPackInstaller.h"
#include "Effects3D/FolderVolume/FolderVolumeEffect.h"
#include "Effects3D/PlayerEngines.h"
#include "Effects3D/SpatialPatternKernels/SpatialPatternKernels.h"
#include "LogManager.h"
#include "PluginSettingsPaths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTemporaryDir>
#include <QUrl>

namespace
{
constexpr const char* kStockArchiveUrl =
    "https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets/archive/refs/heads/master.zip";

const QStringList kStockTopFolders = {
    QStringLiteral("effects"),
    QStringLiteral("patterns"),
    QStringLiteral("controllers"),
    QStringLiteral("timelines"),
};

bool CopyFileOverwrite(const QString& src, const QString& dst, QString* error_out)
{
    QDir().mkpath(QFileInfo(dst).absolutePath());
    if(QFile::exists(dst) && !QFile::remove(dst))
    {
        if(error_out)
        {
            *error_out = QStringLiteral("Could not replace existing file:\n%1").arg(dst);
        }
        return false;
    }
    if(!QFile::copy(src, dst))
    {
        if(error_out)
        {
            *error_out = QStringLiteral("Could not copy:\n%1\n→ %2").arg(src, dst);
        }
        return false;
    }
    return true;
}

} // namespace

StockPackInstaller::StockPackInstaller(OpenRGBPluginAPIInterface* api, QObject* parent)
    : QObject(parent)
    , api_(api)
{
}

QString StockPackInstaller::DefaultDownloadUrl()
{
    return QString::fromUtf8(kStockArchiveUrl);
}

void StockPackInstaller::InstallFromNetwork(const QString& override_url)
{
    const QString url = override_url.isEmpty() ? DefaultDownloadUrl() : override_url;
    StartDownload(url);
}

void StockPackInstaller::InstallFromZipFile(const QString& zip_path)
{
    if(zip_path.isEmpty() || !QFileInfo::exists(zip_path))
    {
        EmitFail(QStringLiteral("Zip file not found."));
        return;
    }
    UnpackAndMergeZip(zip_path);
}

void StockPackInstaller::StartDownload(const QString& url)
{
    if(!api_)
    {
        EmitFail(QStringLiteral("Plugin API is not available."));
        return;
    }

    emit Progress(QStringLiteral("Downloading stock pack…"));

    // Keep the zip alive until unpack finishes (Finished may show a modal dialog).
    auto* zip_hold = new QTemporaryDir();
    zip_hold->setAutoRemove(true);
    if(!zip_hold->isValid())
    {
        delete zip_hold;
        EmitFail(QStringLiteral("Could not create a temporary folder for the download."));
        return;
    }

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("OpenRGB3DSpatialPlugin-StockPackInstaller"));

    QNetworkReply* reply = nam->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if(total > 0)
        {
            const int pct = (int)((received * 100) / total);
            emit Progress(QStringLiteral("Downloading stock pack… %1%").arg(pct));
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, zip_hold]() {
        reply->deleteLater();
        nam->deleteLater();

        if(reply->error() != QNetworkReply::NoError)
        {
            delete zip_hold;
            EmitFail(QStringLiteral("Download failed:\n%1\n\nYou can use “Install from zip…” with a local copy.")
                         .arg(reply->errorString()));
            return;
        }

        const QString zip_path = zip_hold->filePath(QStringLiteral("stock-pack.zip"));
        QFile out(zip_path);
        if(!out.open(QIODevice::WriteOnly))
        {
            delete zip_hold;
            EmitFail(QStringLiteral("Could not write downloaded zip."));
            return;
        }
        out.write(reply->readAll());
        out.close();

        UnpackAndMergeZip(zip_path);
        delete zip_hold;
    });
}

void StockPackInstaller::UnpackAndMergeZip(const QString& zip_path)
{
    if(!api_)
    {
        EmitFail(QStringLiteral("Plugin API is not available."));
        return;
    }

    emit Progress(QStringLiteral("Extracting stock pack…"));

    QTemporaryDir extract_dir;
    extract_dir.setAutoRemove(true);
    if(!extract_dir.isValid())
    {
        EmitFail(QStringLiteral("Could not create a temporary folder for extraction."));
        return;
    }

    QString tar_error;
    if(!ExtractZipWithTar(zip_path, extract_dir.path(), &tar_error))
    {
        EmitFail(tar_error);
        return;
    }

    const QString content_root = FindArchiveContentRoot(extract_dir.path());
    if(content_root.isEmpty())
    {
        EmitFail(QStringLiteral(
            "Zip did not contain effects/ or patterns/.\n"
            "Use the OpenRGB3DSpatialPresets archive (or a zip with those folders at the top)."));
        return;
    }

    emit Progress(QStringLiteral("Installing into plugin data folder…"));

    QString merge_error;
    if(!MergeStockTree(content_root, &merge_error))
    {
        EmitFail(merge_error);
        return;
    }

    RescanFileLoadedEffects();

    const QString dest = QString::fromStdString(PluginSettingsPaths::PluginRoot(api_).string());
    LOG_INFO("[OpenRGB3DSpatialPlugin] Stock pack installed into %s", dest.toUtf8().constData());
    emit Finished(true,
                  QStringLiteral("Stock pack installed.\n\nData folder:\n%1\n\n"
                                 "Effect library will refresh. Same-named stock files were updated; "
                                 "your other custom files were left alone.")
                      .arg(dest));
}

bool StockPackInstaller::ExtractZipWithTar(const QString& zip_path, const QString& dest_dir, QString* error_out)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("tar"));
    proc.setArguments({QStringLiteral("-xf"), zip_path, QStringLiteral("-C"), dest_dir});
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    if(!proc.waitForStarted(10000))
    {
        if(error_out)
        {
            *error_out = QStringLiteral(
                "Could not start `tar` to extract the zip.\n"
                "On Windows 10+ tar is usually available; otherwise extract manually and use the folders.");
        }
        return false;
    }
    if(!proc.waitForFinished(120000) || proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
    {
        if(error_out)
        {
            *error_out = QStringLiteral("Extract failed:\n%1").arg(QString::fromUtf8(proc.readAll()));
        }
        return false;
    }
    return true;
}

QString StockPackInstaller::FindArchiveContentRoot(const QString& extract_dir) const
{
    QDir root(extract_dir);
    auto has_stock = [](const QString& path) {
        return QDir(path + QStringLiteral("/effects")).exists()
               || QDir(path + QStringLiteral("/patterns")).exists();
    };

    if(has_stock(extract_dir))
    {
        return extract_dir;
    }

    const QFileInfoList kids =
        root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for(const QFileInfo& fi : kids)
    {
        if(has_stock(fi.absoluteFilePath()))
        {
            return fi.absoluteFilePath();
        }
    }
    return QString();
}

bool StockPackInstaller::MergeStockTree(const QString& extracted_root, QString* error_out)
{
    PluginSettingsPaths::EnsurePluginDataLayout(api_);
    const QString dest_root = QString::fromStdString(PluginSettingsPaths::PluginRoot(api_).string());

    int copied = 0;
    for(const QString& top : kStockTopFolders)
    {
        const QString src_top = extracted_root + QLatin1Char('/') + top;
        if(!QDir(src_top).exists())
        {
            continue;
        }

        QDirIterator it(src_top, QDir::Files, QDirIterator::Subdirectories);
        while(it.hasNext())
        {
            const QString src = it.next();
            const QString rel = QDir(extracted_root).relativeFilePath(src);
            const QString dst = dest_root + QLatin1Char('/') + rel;
            if(!CopyFileOverwrite(src, dst, error_out))
            {
                return false;
            }
            ++copied;
        }
    }

    if(copied == 0)
    {
        if(error_out)
        {
            *error_out = QStringLiteral("No stock files found to install (effects/patterns/…).");
        }
        return false;
    }
    return true;
}

void StockPackInstaller::EmitFail(const QString& message)
{
    LOG_WARNING("[OpenRGB3DSpatialPlugin] Stock pack install failed: %s", message.toUtf8().constData());
    emit Finished(false, message);
}

void RescanFileLoadedEffects()
{
    SpatialPatternKernelsReload();
    RegisterFolderVolumeEffects();
    RegisterShaderFieldEffects();
}
