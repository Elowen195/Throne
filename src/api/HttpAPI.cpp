#include "include/api/HttpAPI.h"

#include "3rdparty/httplib.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <QDateTime>

namespace API {

    HttpAPIServer::HttpAPIServer(QObject *parent)
        : QThread(parent)
        , running_(false)
        , host_("127.0.0.1")
        , port_(15555)
    {
        LoadOrGenerateToken();
    }

    HttpAPIServer::~HttpAPIServer() {
        Stop();
    }

    bool HttpAPIServer::Start(const QString &host, int port) {
        QMutexLocker locker(&mutex_);

        if (running_) {
            return false;
        }

        host_ = host;
        port_ = port;

        // Start the thread
        QThread::start();

        // Wait for server to start
        for (int i = 0; i < 50; ++i) {
            if (running_) {
                return true;
            }
            QThread::msleep(100);
        }

        return running_;
    }

    void HttpAPIServer::Stop() {
        if (!running_) {
            return;
        }

        running_ = false;

        if (server_) {
            server_->stop();
        }

        // Wait for thread to finish
        if (!wait(5000)) {
            terminate();
            wait();
        }
    }

    bool HttpAPIServer::IsRunning() const {
        return running_;
    }

    QString HttpAPIServer::GetToken() const {
        return api_token_;
    }

    void HttpAPIServer::SetNodeSwitchCallback(std::function<NodeRotationResult()> callback) {
        node_switch_callback_ = std::move(callback);
    }

    void HttpAPIServer::run() {
        server_ = std::make_unique<httplib::Server>();

        SetupRoutes();

        running_ = true;

        // Start listening
        auto host_std = host_.toStdString();
        if (!server_->listen(host_std.c_str(), port_)) {
            running_ = false;
            return;
        }

        running_ = false;
    }

    void HttpAPIServer::SetupRoutes() {
        // Health check endpoint
        server_->Get("/api/v1/health", [](const httplib::Request &, httplib::Response &res) {
            QJsonObject json;
            json["status"] = "ok";
            json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

            QJsonDocument doc(json);
            res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
        });

        // Get API info
        server_->Get("/api/v1/info", [this](const httplib::Request &req, httplib::Response &res) {
            // Verify token
            auto auth_header = req.get_header_value("Authorization");
            QString auth = QString::fromStdString(auth_header);

            if (!auth.startsWith("Bearer ")) {
                res.status = 401;
                QJsonObject json;
                json["success"] = false;
                json["error"] = "Missing or invalid Authorization header";
                QJsonDocument doc(json);
                res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
                return;
            }

            QString token = auth.mid(7); // Remove "Bearer "
            if (!VerifyToken(token)) {
                res.status = 401;
                QJsonObject json;
                json["success"] = false;
                json["error"] = "Invalid token";
                QJsonDocument doc(json);
                res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
                return;
            }

            QJsonObject json;
            json["success"] = true;
            json["api_port"] = port_;

            QJsonDocument doc(json);
            res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
        });

        // Switch to next node in current group
        server_->Post("/api/v1/switch-next", [this](const httplib::Request &req, httplib::Response &res) {
            // Verify token
            auto auth_header = req.get_header_value("Authorization");
            QString auth = QString::fromStdString(auth_header);

            if (!auth.startsWith("Bearer ")) {
                res.status = 401;
                QJsonObject json;
                json["success"] = false;
                json["error"] = "Missing or invalid Authorization header";
                QJsonDocument doc(json);
                res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
                return;
            }

            QString token = auth.mid(7);
            if (!VerifyToken(token)) {
                res.status = 401;
                QJsonObject json;
                json["success"] = false;
                json["error"] = "Invalid token";
                QJsonDocument doc(json);
                res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
                return;
            }

            // Call the callback
            if (!node_switch_callback_) {
                res.status = 500;
                QJsonObject json;
                json["success"] = false;
                json["error"] = "Node switch callback not set";
                QJsonDocument errorDoc(json);
                res.set_content(errorDoc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
                return;
            }

            NodeRotationResult result = node_switch_callback_();

            if (!result.success) {
                res.status = 400;
                QJsonObject json;
                json["success"] = false;
                json["error"] = result.error;
                QJsonDocument errorDoc(json);
                res.set_content(errorDoc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
                return;
            }

            // Success response
            QJsonObject json;
            json["success"] = true;
            json["group_name"] = result.group_name;

            if (result.previous_node_id >= 0) {
                QJsonObject previousNode;
                previousNode["id"] = result.previous_node_id;
                previousNode["name"] = result.previous_node_name;
                previousNode["index"] = result.previous_node_index;
                json["previous_node"] = previousNode;
            }

            QJsonObject currentNode;
            currentNode["id"] = result.current_node_id;
            currentNode["name"] = result.current_node_name;
            currentNode["index"] = result.current_node_index;
            json["current_node"] = currentNode;

            json["total_nodes"] = result.total_nodes;

            QJsonDocument responseDoc(json);
            res.set_content(responseDoc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
        });
    }

    bool HttpAPIServer::VerifyToken(const QString &token) const {
        return token == api_token_;
    }

    QString HttpAPIServer::GenerateToken() {
        const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        QString token;
        for (int i = 0; i < 32; ++i) {
            int index = QRandomGenerator::global()->bounded(chars.length());
            token.append(chars.at(index));
        }
        return token;
    }

    void HttpAPIServer::LoadOrGenerateToken() {
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configPath);
        QString tokenFile = configPath + "/api_token.txt";

        QFile file(tokenFile);
        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            api_token_ = QString::fromUtf8(file.readAll()).trimmed();
            file.close();

            if (!api_token_.isEmpty()) {
                return;
            }
        }

        // Generate new token
        api_token_ = GenerateToken();
        SaveToken();
    }

    void HttpAPIServer::SaveToken() {
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configPath);
        QString tokenFile = configPath + "/api_token.txt";

        QFile file(tokenFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(api_token_.toUtf8());
            file.close();
        }
    }

} // namespace API
