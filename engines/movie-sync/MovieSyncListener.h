// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QObject>
#include <QByteArray>
#include <functional>
#include <string>

class QTcpServer;
class QTcpSocket;

namespace MovieSync
{

/** Loopback HTTP listener: GET /v1/health, GET /v1/catalog, POST /v1/effect. */
class ScriptListener : public QObject
{
    Q_OBJECT
public:
    using EffectHandler = std::function<void(const std::string& effect_id,
                                             const std::string& edge,
                                             bool active)>;
    using CatalogJsonFn = std::function<std::string()>;

    explicit ScriptListener(QObject* parent = nullptr);
    ~ScriptListener() override;

    bool Start(quint16 port, QString* error);
    void Stop();
    bool isListening() const;
    quint16 port() const { return port_; }

    void SetEffectHandler(EffectHandler fn) { on_effect_ = std::move(fn); }
    void SetCatalogJson(CatalogJsonFn fn) { catalog_json_ = std::move(fn); }

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    static QByteArray httpResponse(int code, const char* status, const QByteArray& body,
                                   const char* content_type = "application/json");
    void handleRequest(QTcpSocket* socket, const QByteArray& raw);

    QTcpServer* server_ = nullptr;
    quint16 port_ = 27180;
    EffectHandler on_effect_;
    CatalogJsonFn catalog_json_;
};

} // namespace MovieSync
