#pragma once

#include <QString>
#include <QThread>
#include <QMutex>
#include <memory>
#include <atomic>
#include <functional>

// Forward declaration
namespace httplib {
    class Server;
}

namespace API {

    struct NodeRotationResult {
        bool success = false;
        QString error;
        QString group_name;
        int previous_node_id = -1;
        QString previous_node_name;
        int previous_node_index = -1;
        int current_node_id = -1;
        QString current_node_name;
        int current_node_index = -1;
        int total_nodes = 0;
    };

    class HttpAPIServer : public QThread {
        Q_OBJECT

    public:
        explicit HttpAPIServer(QObject *parent = nullptr);
        ~HttpAPIServer() override;

        // Start the HTTP server
        bool Start(const QString &host = "127.0.0.1", int port = 15555);

        // Stop the HTTP server
        void Stop();

        // Check if server is running
        bool IsRunning() const;

        // Get the API token
        QString GetToken() const;

        // Set callback for node switching
        void SetNodeSwitchCallback(std::function<NodeRotationResult()> callback);

    protected:
        void run() override;

    private:
        // Setup API routes
        void SetupRoutes();

        // Verify token
        bool VerifyToken(const QString &token) const;

        // Generate random token
        QString GenerateToken();

        // Load or generate token
        void LoadOrGenerateToken();

        // Save token to config
        void SaveToken();

        std::unique_ptr<httplib::Server> server_;
        std::atomic<bool> running_;
        QString host_;
        int port_;
        QString api_token_;
        QMutex mutex_;

        std::function<NodeRotationResult()> node_switch_callback_;
    };

    // Global instance
    inline HttpAPIServer *httpAPIServer = nullptr;

} // namespace API
