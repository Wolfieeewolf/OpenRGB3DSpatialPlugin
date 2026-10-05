// SPDX-License-Identifier: GPL-2.0-only

#include "MovieCueListener.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <nlohmann/json.hpp>

namespace MovieCue
{

ScriptListener::ScriptListener(QObject* parent)
    : QObject(parent)
{
}

ScriptListener::~ScriptListener()
{
    Stop();
}

bool ScriptListener::Start(quint16 port, QString* error)
{
    Stop();
    server_ = new QTcpServer(this);
    if(!server_->listen(QHostAddress::LocalHost, port))
    {
        if(error)
        {
            *error = server_->errorString();
        }
        delete server_;
        server_ = nullptr;
        return false;
    }
    port_ = server_->serverPort();
    connect(server_, &QTcpServer::newConnection, this, &ScriptListener::onNewConnection);
    return true;
}

void ScriptListener::Stop()
{
    if(server_)
    {
        server_->close();
        server_->deleteLater();
        server_ = nullptr;
    }
}

bool ScriptListener::isListening() const
{
    return server_ && server_->isListening();
}

void ScriptListener::onNewConnection()
{
    while(server_ && server_->hasPendingConnections())
    {
        QTcpSocket* socket = server_->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &ScriptListener::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void ScriptListener::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if(!socket)
    {
        return;
    }
    const QByteArray raw = socket->property("buf").toByteArray() + socket->readAll();
    const int hdr_end = raw.indexOf("\r\n\r\n");
    if(hdr_end < 0)
    {
        socket->setProperty("buf", raw);
        return;
    }
    int content_length = 0;
    const QByteArray headers = raw.left(hdr_end);
    for(const QByteArray& line : headers.split('\n'))
    {
        const QByteArray t = line.trimmed();
        if(t.toLower().startsWith("content-length:"))
        {
            content_length = t.mid(15).trimmed().toInt();
        }
    }
    const QByteArray body = raw.mid(hdr_end + 4);
    if(body.size() < content_length)
    {
        socket->setProperty("buf", raw);
        return;
    }
    socket->setProperty("buf", QByteArray());
    handleRequest(socket, raw.left(hdr_end + 4 + content_length));
}

QByteArray ScriptListener::httpResponse(int code, const char* status, const QByteArray& body, const char* content_type)
{
    QByteArray out;
    out += "HTTP/1.1 ";
    out += QByteArray::number(code);
    out += " ";
    out += status;
    out += "\r\nContent-Type: ";
    out += content_type;
    out += "\r\nContent-Length: ";
    out += QByteArray::number(body.size());
    out += "\r\nConnection: close\r\n\r\n";
    out += body;
    return out;
}

void ScriptListener::handleRequest(QTcpSocket* socket, const QByteArray& raw)
{
    const int line_end = raw.indexOf("\r\n");
    const QByteArray request_line = line_end > 0 ? raw.left(line_end) : raw;
    const QList<QByteArray> parts = request_line.split(' ');
    const QByteArray method = parts.value(0);
    const QByteArray path = parts.value(1);

    auto reply = [&](int code, const char* status, const QByteArray& body) {
        socket->write(httpResponse(code, status, body));
        socket->disconnectFromHost();
    };

    if(method == "GET" && path == "/v1/health")
    {
        reply(200, "OK", "{\"ok\":true,\"service\":\"openrgb3d.movie_cue\"}");
        return;
    }
    if(method == "GET" && path == "/v1/catalog")
    {
        const std::string json = catalog_json_ ? catalog_json_() : "{\"effects\":[]}";
        reply(200, "OK", QByteArray::fromStdString(json));
        return;
    }
    if(method == "POST" && path.startsWith("/v1/effect"))
    {
        const int hdr_end = raw.indexOf("\r\n\r\n");
        const QByteArray body = hdr_end >= 0 ? raw.mid(hdr_end + 4) : QByteArray();
        try
        {
            const nlohmann::json j = nlohmann::json::parse(body.size() ? body.constData() : "{}");
            const std::string effect_id = j.value("effect_id", "");
            const std::string edge = j.value("edge", "pulse");
            bool active = true;
            if(j.contains("active"))
            {
                active = j["active"].get<bool>();
            }
            if(effect_id.empty())
            {
                reply(400, "Bad Request", "{\"ok\":false,\"error\":\"effect_id required\"}");
                return;
            }
            if(on_effect_)
            {
                on_effect_(effect_id, edge, active);
            }
            // Handler reports failures via status UI; always ack receipt so VLC keeps polling.
            reply(200, "OK", QByteArray::fromStdString(
                      std::string("{\"ok\":true,\"effect_id\":\"") + effect_id + "\"}"));
            return;
        }
        catch(...)
        {
            reply(400, "Bad Request", "{\"ok\":false,\"error\":\"invalid json\"}");
            return;
        }
    }
    reply(404, "Not Found", "{\"ok\":false,\"error\":\"not found\"}");
}

} // namespace MovieCue
