// SPDX-License-Identifier: GPL-2.0-only

#ifndef STOCKPACKINSTALLER_H
#define STOCKPACKINSTALLER_H

#include <QObject>
#include <QString>

class OpenRGBPluginAPIInterface;

/** Download or unpack the OpenRGB3DSpatialPresets stock pack into plugin data. */
class StockPackInstaller : public QObject
{
    Q_OBJECT

public:
    explicit StockPackInstaller(OpenRGBPluginAPIInterface* api, QObject* parent = nullptr);

    static QString DefaultDownloadUrl();

    /** Download DefaultDownloadUrl() (or override_url) and merge into plugin data. */
    void InstallFromNetwork(const QString& override_url = QString());

    /** Unpack a local zip (GitHub archive or flat stock layout) into plugin data. */
    void InstallFromZipFile(const QString& zip_path);

signals:
    void Progress(const QString& message);
    void Finished(bool ok, const QString& message);

private:
    void StartDownload(const QString& url);
    void UnpackAndMergeZip(const QString& zip_path);
    bool ExtractZipWithTar(const QString& zip_path, const QString& dest_dir, QString* error_out);
    bool MergeStockTree(const QString& extracted_root, QString* error_out);
    QString FindArchiveContentRoot(const QString& extract_dir) const;
    void EmitFail(const QString& message);

    OpenRGBPluginAPIInterface* api_ = nullptr;
};

/** Re-scan disk content into EffectListManager (FolderVolume, Shader Field, kernels). */
void RescanFileLoadedEffects();

#endif
