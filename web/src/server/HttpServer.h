#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QMap>

class HttpServer : public QObject
{
    Q_OBJECT

public:
    explicit HttpServer(QObject *parent = nullptr);
    ~HttpServer();
    
    bool start(const QString& host, quint16 port);
    void stop();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    struct HttpRequest {
        QString method;
        QString path;
        QMap<QString, QString> headers;
        QMap<QString, QString> queryParams;
        QByteArray body;
    };
    
    struct HttpResponse {
        int statusCode = 200;
        QString statusText = "OK";
        QMap<QString, QString> headers;
        QByteArray body;
    };
    
    HttpRequest parseRequest(const QByteArray& data);
    void sendResponse(QTcpSocket* socket, const HttpResponse& response);
    void handleRequest(QTcpSocket* socket, const HttpRequest& request);
    
    // API路由处理
    void handleApiRequest(QTcpSocket* socket, const HttpRequest& request);
    void handleStaticFile(QTcpSocket* socket, const QString& path);
    
    // 用户API
    void apiRegister(QTcpSocket* socket, const QJsonObject& data);
    void apiLogin(QTcpSocket* socket, const QJsonObject& data);
    void apiVerifyEmail(QTcpSocket* socket, const QJsonObject& data);
    void apiSendVerificationCode(QTcpSocket* socket, const QJsonObject& data);
    void apiResetPassword(QTcpSocket* socket, const QJsonObject& data);
    void apiChangePassword(QTcpSocket* socket, const QJsonObject& data);
    void apiGetUserInfo(QTcpSocket* socket, const HttpRequest& request);
    void apiGetUserStatistics(QTcpSocket* socket, const HttpRequest& request);
    
    // 管理员API
    void apiGetAdminStats(QTcpSocket* socket, const HttpRequest& request);
    void apiGetAllUsers(QTcpSocket* socket, const HttpRequest& request);
    void apiToggleUserStatus(QTcpSocket* socket, const QJsonObject& data);
    
    // 房产API
    void apiGetHouses(QTcpSocket* socket, const HttpRequest& request);
    void apiSearchHouses(QTcpSocket* socket, const QJsonObject& data);
    void apiGetHouseDetail(QTcpSocket* socket, int houseId);
    void apiGetHouseStatistics(QTcpSocket* socket);
    void apiGetPopularHouses(QTcpSocket* socket);
    
    // 收藏API
    void apiAddFavorite(QTcpSocket* socket, const QJsonObject& data);
    void apiRemoveFavorite(QTcpSocket* socket, const QJsonObject& data);
    void apiGetFavorites(QTcpSocket* socket, const HttpRequest& request);
    
    // 偏好API
    void apiSavePreferences(QTcpSocket* socket, const QJsonObject& data);
    void apiGetPreferences(QTcpSocket* socket, const HttpRequest& request);
    
    // AI API
    void apiAIRecommend(QTcpSocket* socket, const QJsonObject& data);
    void apiAIChat(QTcpSocket* socket, const QJsonObject& data);
    
    // 配置API
    void apiGetConfig(QTcpSocket* socket);
    
    // 工具函数
    QJsonObject createJsonResponse(bool success, const QString& message, const QVariant& data = QVariant());
    QString getMimeType(const QString& fileName);
    int getUserIdFromToken(const QString& token);
    QString generateToken(int userId);
    QString hashPassword(const QString& password);
    
    QTcpServer* server;
};

#endif // HTTPSERVER_H
