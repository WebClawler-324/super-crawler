#include "HttpServer.h"
#include "../database/DatabaseManager.h"
#include "../services/EmailService.h"
#include "../services/AIService.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QDateTime>
#include <QSqlQuery>

HttpServer::HttpServer(QObject *parent)
    : QObject(parent)
    , server(new QTcpServer(this))
{
    connect(server, &QTcpServer::newConnection, this, &HttpServer::onNewConnection);
}

HttpServer::~HttpServer()
{
    stop();
}

bool HttpServer::start(const QString& host, quint16 port)
{
    QHostAddress address(host);
    if (host == "0.0.0.0") {
        address = QHostAddress::Any;
    }
    
    if (!server->listen(address, port)) {
        qCritical() << "Failed to start HTTP server:" << server->errorString();
        return false;
    }
    
    qDebug() << "HTTP server started on" << host << ":" << port;
    return true;
}

void HttpServer::stop()
{
    if (server->isListening()) {
        server->close();
        qDebug() << "HTTP server stopped";
    }
}

void HttpServer::onNewConnection()
{
    QTcpSocket* socket = server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &HttpServer::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &HttpServer::onDisconnected);
}

void HttpServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    QByteArray requestData = socket->readAll();
    HttpRequest request = parseRequest(requestData);
    
    handleRequest(socket, request);
}

void HttpServer::onDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

HttpServer::HttpRequest HttpServer::parseRequest(const QByteArray& data)
{
    HttpRequest request;
    QString requestStr = QString::fromUtf8(data);
    QStringList lines = requestStr.split("\r\n");
    
    if (lines.isEmpty()) return request;
    
    // 解析请求行
    QStringList requestLine = lines[0].split(" ");
    if (requestLine.size() >= 2) {
        request.method = requestLine[0];
        QString fullPath = requestLine[1];
        
        // 解析查询参数
        int queryPos = fullPath.indexOf('?');
        if (queryPos != -1) {
            request.path = fullPath.left(queryPos);
            QString queryString = fullPath.mid(queryPos + 1);
            QUrlQuery query(queryString);
            auto items = query.queryItems();
            for (const auto& item : items) {
                request.queryParams[item.first] = item.second;
            }
        } else {
            request.path = fullPath;
        }
    }
    
    // 解析请求头
    int i = 1;
    for (; i < lines.size(); ++i) {
        if (lines[i].isEmpty()) {
            ++i;
            break;
        }
        int colonPos = lines[i].indexOf(':');
        if (colonPos != -1) {
            QString key = lines[i].left(colonPos).trimmed();
            QString value = lines[i].mid(colonPos + 1).trimmed();
            request.headers[key] = value;
        }
    }
    
    // 解析请求体
    if (i < lines.size()) {
        request.body = lines.mid(i).join("\r\n").toUtf8();
    }
    
    return request;
}

void HttpServer::sendResponse(QTcpSocket* socket, const HttpResponse& response)
{
    QString statusLine = QString("HTTP/1.1 %1 %2\r\n").arg(response.statusCode).arg(response.statusText);
    socket->write(statusLine.toUtf8());
    
    // 写入响应头
    for (auto it = response.headers.begin(); it != response.headers.end(); ++it) {
        QString header = QString("%1: %2\r\n").arg(it.key()).arg(it.value());
        socket->write(header.toUtf8());
    }
    
    socket->write("\r\n");
    socket->write(response.body);
    socket->flush();
    socket->disconnectFromHost();
}

void HttpServer::handleRequest(QTcpSocket* socket, const HttpRequest& request)
{
    qDebug() << request.method << request.path;
    
    // CORS处理
    if (request.method == "OPTIONS") {
        HttpResponse response;
        response.statusCode = 200;
        response.headers["Access-Control-Allow-Origin"] = "*";
        response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        sendResponse(socket, response);
        return;
    }
    
    // API路由
    if (request.path.startsWith("/api/")) {
        handleApiRequest(socket, request);
    } else {
        handleStaticFile(socket, request.path);
    }
}

void HttpServer::handleApiRequest(QTcpSocket* socket, const HttpRequest& request)
{
    QJsonDocument doc;
    QJsonObject data;
    
    if (!request.body.isEmpty()) {
        doc = QJsonDocument::fromJson(request.body);
        if (doc.isObject()) {
            data = doc.object();
        }
    }
    
    QString path = request.path;
    
    // 用户相关API
    if (path == "/api/register" && request.method == "POST") {
        apiRegister(socket, data);
    } else if (path == "/api/login" && request.method == "POST") {
        apiLogin(socket, data);
    } else if (path == "/api/verify-email" && request.method == "POST") {
        apiVerifyEmail(socket, data);
    } else if (path == "/api/send-code" && request.method == "POST") {
        apiSendVerificationCode(socket, data);
    } else if (path == "/api/reset-password" && request.method == "POST") {
        apiResetPassword(socket, data);
    } else if (path == "/api/change-password" && request.method == "POST") {
        apiChangePassword(socket, data);
    } else if (path == "/api/user/info" && request.method == "GET") {
        apiGetUserInfo(socket, request);
    }
    // 管理员相关API
    else if (path == "/api/admin/stats" && request.method == "GET") {
        apiGetAdminStats(socket, request);
    } else if (path == "/api/admin/users" && request.method == "GET") {
        apiGetAllUsers(socket, request);
    } else if (path == "/api/admin/toggle-user" && request.method == "POST") {
        apiToggleUserStatus(socket, data);
    } else if (path == "/api/admin/users" && request.method == "GET") {
        apiGetUserStatistics(socket, request);
    }
    // 房产相关API
    else if (path == "/api/houses" && request.method == "GET") {
        apiGetHouses(socket, request);
    } else if (path == "/api/houses/search" && request.method == "POST") {
        apiSearchHouses(socket, data);
    } else if (path == "/api/houses/statistics" && request.method == "GET") {
        apiGetHouseStatistics(socket);
    } else if (path == "/api/houses/popular" && request.method == "GET") {
        apiGetPopularHouses(socket);
    } else if (path.startsWith("/api/houses/") && request.method == "GET") {
        // 这个必须放在 /api/houses/statistics 和 /api/houses/popular 之后
        QString idStr = path.mid(QString("/api/houses/").length());
        apiGetHouseDetail(socket, idStr.toInt());
    }
    // 收藏相关API
    else if (path == "/api/favorites" && request.method == "POST") {
        apiAddFavorite(socket, data);
    } else if (path == "/api/favorites" && request.method == "DELETE") {
        apiRemoveFavorite(socket, data);
    } else if (path == "/api/favorites" && request.method == "GET") {
        apiGetFavorites(socket, request);
    }
    // 偏好相关API
    else if (path == "/api/preferences" && request.method == "POST") {
        apiSavePreferences(socket, data);
    } else if (path == "/api/preferences" && request.method == "GET") {
        apiGetPreferences(socket, request);
    }
    // AI相关API
    else if (path == "/api/ai/recommend" && request.method == "POST") {
        apiAIRecommend(socket, data);
    } else if (path == "/api/ai/chat" && request.method == "POST") {
        apiAIChat(socket, data);
    }
    // 配置相关API
    else if (path == "/api/config" && request.method == "GET") {
        apiGetConfig(socket);
    }
    // 404
    else {
        HttpResponse response;
        response.statusCode = 404;
        response.statusText = "Not Found";
        response.headers["Content-Type"] = "application/json";
        response.headers["Access-Control-Allow-Origin"] = "*";
        response.body = QJsonDocument(createJsonResponse(false, "API not found")).toJson();
        sendResponse(socket, response);
    }
}

void HttpServer::handleStaticFile(QTcpSocket* socket, const QString& path)
{
    QString filePath = "resources/web";
    if (path == "/") {
        filePath += "/index.html";
    } else {
        filePath += path;
    }
    
    QFile file(filePath);
    HttpResponse response;
    
    if (file.open(QIODevice::ReadOnly)) {
        response.body = file.readAll();
        response.headers["Content-Type"] = getMimeType(filePath);
        response.headers["Access-Control-Allow-Origin"] = "*";
        file.close();
    } else {
        response.statusCode = 404;
        response.statusText = "Not Found";
        response.headers["Content-Type"] = "text/html";
        response.body = "<html><body><h1>404 Not Found</h1></body></html>";
    }
    
    sendResponse(socket, response);
}

// API实现
void HttpServer::apiRegister(QTcpSocket* socket, const QJsonObject& data)
{
    QString username = data["username"].toString();
    QString password = data["password"].toString();
    QString email = data["email"].toString();
    QString code = data["code"].toString();
    
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    // 验证输入
    if (username.isEmpty() || password.isEmpty() || email.isEmpty() || code.isEmpty()) {
        response.body = QJsonDocument(createJsonResponse(false, "所有字段都是必填的")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    // 验证验证码
    QString savedCode = DatabaseManager::instance()->getVerificationCode(email, "register");
    if (savedCode != code) {
        response.body = QJsonDocument(createJsonResponse(false, "验证码错误或已过期")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    // 创建用户
    QString passwordHash = hashPassword(password);
    if (DatabaseManager::instance()->createUser(username, passwordHash, email)) {
        DatabaseManager::instance()->deleteVerificationCode(email, "register");
        
        // 获取用户信息
        QVariantMap user = DatabaseManager::instance()->getUserByUsername(username);
        int userId = user["id"].toInt();
        
        // 验证邮箱
        DatabaseManager::instance()->verifyEmail(userId);
        
        QJsonObject result;
        result["token"] = generateToken(userId);
        result["userId"] = userId;
        result["username"] = username;
        
        response.body = QJsonDocument(createJsonResponse(true, "注册成功", result)).toJson();
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "用户名或邮箱已存在")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiLogin(QTcpSocket* socket, const QJsonObject& data)
{
    QString username = data["username"].toString();
    QString password = data["password"].toString();
    
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QVariantMap user = DatabaseManager::instance()->getUserByUsername(username);
    if (user.isEmpty()) {
        response.body = QJsonDocument(createJsonResponse(false, "用户名或密码错误")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    // 检查账户是否被禁用
    if (user["is_disabled"].toBool()) {
        response.body = QJsonDocument(createJsonResponse(false, "该账户已被禁用，请联系管理员")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    QString passwordHash = hashPassword(password);
    if (user["password_hash"].toString() != passwordHash) {
        response.body = QJsonDocument(createJsonResponse(false, "用户名或密码错误")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    // 更新最后登录时间
    int userId = user["id"].toInt();
    QVariantMap updateData;
    updateData["last_login"] = true;
    DatabaseManager::instance()->updateUser(userId, updateData);
    
    QJsonObject result;
    result["token"] = generateToken(userId);
    result["userId"] = userId;
    result["username"] = user["username"].toString();
    result["email"] = user["email"].toString();
    result["isAdmin"] = user["is_admin"].toBool();
    
    response.body = QJsonDocument(createJsonResponse(true, "登录成功", result)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiVerifyEmail(QTcpSocket* socket, const QJsonObject& data)
{
    QString email = data["email"].toString();
    QString code = data["code"].toString();
    
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QString savedCode = DatabaseManager::instance()->getVerificationCode(email, "verify");
    if (savedCode == code) {
        DatabaseManager::instance()->deleteVerificationCode(email, "verify");
        response.body = QJsonDocument(createJsonResponse(true, "邮箱验证成功")).toJson();
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "验证码错误或已过期")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiSendVerificationCode(QTcpSocket* socket, const QJsonObject& data)
{
    QString email = data["email"].toString();
    QString type = data["type"].toString(); // register, reset_password
    
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QString code = EmailService::generateVerificationCode();
    
    if (DatabaseManager::instance()->saveVerificationCode(email, code, type)) {
        if (EmailService::instance()->sendVerificationEmail(email, code, type)) {
            response.body = QJsonDocument(createJsonResponse(true, "验证码已发送到您的邮箱")).toJson();
        } else {
            response.body = QJsonDocument(createJsonResponse(false, "验证码发送失败，请稍后重试")).toJson();
        }
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "验证码保存失败")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiResetPassword(QTcpSocket* socket, const QJsonObject& data)
{
    QString email = data["email"].toString();
    QString code = data["code"].toString();
    QString newPassword = data["newPassword"].toString();
    
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QString savedCode = DatabaseManager::instance()->getVerificationCode(email, "reset_password");
    if (savedCode != code) {
        response.body = QJsonDocument(createJsonResponse(false, "验证码错误或已过期")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    // 通过邮箱查找用户
    QSqlQuery query;
    query.prepare("SELECT id, username FROM users WHERE email = ?");
    query.addBindValue(email);
    
    if (!query.exec() || !query.next()) {
        response.body = QJsonDocument(createJsonResponse(false, "用户不存在")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    int userId = query.value("id").toInt();
    QString username = query.value("username").toString();
    
    QString passwordHash = hashPassword(newPassword);
    if (DatabaseManager::instance()->updatePassword(userId, passwordHash)) {
        DatabaseManager::instance()->deleteVerificationCode(email, "reset_password");
        EmailService::instance()->sendPasswordResetNotification(email, username);
        response.body = QJsonDocument(createJsonResponse(true, "密码重置成功")).toJson();
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "密码重置失败")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiGetUserInfo(QTcpSocket* socket, const HttpRequest& request)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QString token = request.headers.value("Authorization").replace("Bearer ", "");
    int userId = getUserIdFromToken(token);
    
    if (userId <= 0) {
        response.statusCode = 401;
        response.body = QJsonDocument(createJsonResponse(false, "未授权")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    QVariantMap user = DatabaseManager::instance()->getUserById(userId);
    if (!user.isEmpty()) {
        QJsonObject userObj;
        userObj["id"] = user["id"].toInt();
        userObj["username"] = user["username"].toString();
        userObj["email"] = user["email"].toString();
        userObj["isAdmin"] = user["is_admin"].toBool();
        userObj["emailVerified"] = user["email_verified"].toBool();
        
        response.body = QJsonDocument(createJsonResponse(true, "成功", userObj)).toJson();
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "用户不存在")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiGetUserStatistics(QTcpSocket* socket, const HttpRequest& request)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    // 验证管理员权限
    QString token = request.headers.value("Authorization").replace("Bearer ", "");
    int userId = getUserIdFromToken(token);
    QVariantMap user = DatabaseManager::instance()->getUserById(userId);
    
    if (!user["is_admin"].toBool()) {
        response.statusCode = 403;
        response.body = QJsonDocument(createJsonResponse(false, "需要管理员权限")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    QList<QVariantMap> stats = DatabaseManager::instance()->getUserStatistics();
    QJsonArray statsArray;
    
    for (const auto& stat : stats) {
        QJsonObject obj;
        for (auto it = stat.begin(); it != stat.end(); ++it) {
            obj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        statsArray.append(obj);
    }
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", statsArray)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiGetHouses(QTcpSocket* socket, const HttpRequest& request)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    int limit = request.queryParams.value("limit", "50").toInt();
    int offset = request.queryParams.value("offset", "0").toInt();
    
    QList<QVariantMap> houses = DatabaseManager::instance()->getHouses(limit, offset);
    QJsonArray housesArray;
    
    for (const auto& house : houses) {
        QJsonObject obj;
        for (auto it = house.begin(); it != house.end(); ++it) {
            obj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        housesArray.append(obj);
    }
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", housesArray)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiSearchHouses(QTcpSocket* socket, const QJsonObject& data)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QVariantMap filters;
    if (data.contains("minPrice")) filters["minPrice"] = data["minPrice"].toDouble();
    if (data.contains("maxPrice")) filters["maxPrice"] = data["maxPrice"].toDouble();
    if (data.contains("minArea")) filters["minArea"] = data["minArea"].toDouble();
    if (data.contains("maxArea")) filters["maxArea"] = data["maxArea"].toDouble();
    if (data.contains("communityName")) filters["communityName"] = data["communityName"].toString();
    if (data.contains("houseType")) filters["houseType"] = data["houseType"].toString();
    
    QList<QVariantMap> houses = DatabaseManager::instance()->searchHouses(filters);
    QJsonArray housesArray;
    
    for (const auto& house : houses) {
        QJsonObject obj;
        for (auto it = house.begin(); it != house.end(); ++it) {
            obj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        housesArray.append(obj);
    }
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", housesArray)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiGetHouseDetail(QTcpSocket* socket, int houseId)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QVariantMap house = DatabaseManager::instance()->getHouseById(houseId);
    if (!house.isEmpty()) {
        QJsonObject houseObj;
        for (auto it = house.begin(); it != house.end(); ++it) {
            houseObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        response.body = QJsonDocument(createJsonResponse(true, "成功", houseObj)).toJson();
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "房源不存在")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiGetHouseStatistics(QTcpSocket* socket)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QList<QVariantMap> stats = DatabaseManager::instance()->getHouseStatistics();
    QJsonArray statsArray;
    
    for (const auto& stat : stats) {
        QJsonObject obj;
        for (auto it = stat.begin(); it != stat.end(); ++it) {
            obj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        statsArray.append(obj);
    }
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", statsArray)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiAddFavorite(QTcpSocket* socket, const QJsonObject& data)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    int userId = data["userId"].toInt();
    int houseId = data["houseId"].toInt();
    
    if (DatabaseManager::instance()->addFavorite(userId, houseId)) {
        response.body = QJsonDocument(createJsonResponse(true, "收藏成功")).toJson();
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "收藏失败，可能已收藏过")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiRemoveFavorite(QTcpSocket* socket, const QJsonObject& data)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    int userId = data["userId"].toInt();
    int houseId = data["houseId"].toInt();
    
    if (DatabaseManager::instance()->removeFavorite(userId, houseId)) {
        response.body = QJsonDocument(createJsonResponse(true, "取消收藏成功")).toJson();
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "取消收藏失败")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiGetFavorites(QTcpSocket* socket, const HttpRequest& request)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    int userId = request.queryParams.value("userId").toInt();
    
    QList<QVariantMap> favorites = DatabaseManager::instance()->getUserFavorites(userId);
    QJsonArray favoritesArray;
    
    for (const auto& house : favorites) {
        QJsonObject obj;
        for (auto it = house.begin(); it != house.end(); ++it) {
            obj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        favoritesArray.append(obj);
    }
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", favoritesArray)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiSavePreferences(QTcpSocket* socket, const QJsonObject& data)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    int userId = data["userId"].toInt();
    QJsonObject preferences = data["preferences"].toObject();
    
    if (DatabaseManager::instance()->saveUserPreferences(userId, preferences)) {
        response.body = QJsonDocument(createJsonResponse(true, "偏好保存成功")).toJson();
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "偏好保存失败")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiGetPreferences(QTcpSocket* socket, const HttpRequest& request)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    int userId = request.queryParams.value("userId").toInt();
    
    QJsonObject preferences = DatabaseManager::instance()->getUserPreferences(userId);
    response.body = QJsonDocument(createJsonResponse(true, "成功", preferences)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiAIRecommend(QTcpSocket* socket, const QJsonObject& data)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QString requirement = data["requirement"].toString();
    QVariantMap filters;
    
    // 从AI推荐中提取筛选条件
    if (data.contains("filters")) {
        QJsonObject filtersObj = data["filters"].toObject();
        if (filtersObj.contains("minPrice")) filters["minPrice"] = filtersObj["minPrice"].toDouble();
        if (filtersObj.contains("maxPrice")) filters["maxPrice"] = filtersObj["maxPrice"].toDouble();
        if (filtersObj.contains("minArea")) filters["minArea"] = filtersObj["minArea"].toDouble();
        if (filtersObj.contains("maxArea")) filters["maxArea"] = filtersObj["maxArea"].toDouble();
    }
    
    QList<QVariantMap> houses = DatabaseManager::instance()->searchHouses(filters);
    QString recommendation = AIService::instance()->recommendHouses(requirement, houses);
    
    QJsonObject result;
    result["recommendation"] = recommendation;
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", result)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiAIChat(QTcpSocket* socket, const QJsonObject& data)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QString message = data["message"].toString();
    QJsonArray history = data["history"].toArray();
    
    QString aiResponse = AIService::instance()->chat(message, history);
    
    QJsonObject result;
    result["response"] = aiResponse;
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", result)).toJson();
    sendResponse(socket, response);
}

// 工具函数
QJsonObject HttpServer::createJsonResponse(bool success, const QString& message, const QVariant& data)
{
    QJsonObject response;
    response["success"] = success;
    response["message"] = message;
    
    if (data.isValid()) {
        if (data.canConvert<QJsonObject>()) {
            response["data"] = data.toJsonObject();
        } else if (data.canConvert<QJsonArray>()) {
            response["data"] = data.toJsonArray();
        } else {
            response["data"] = QJsonValue::fromVariant(data);
        }
    }
    
    return response;
}

QString HttpServer::getMimeType(const QString& fileName)
{
    if (fileName.endsWith(".html")) return "text/html; charset=UTF-8";
    if (fileName.endsWith(".css")) return "text/css; charset=UTF-8";
    if (fileName.endsWith(".js")) return "application/javascript; charset=UTF-8";
    if (fileName.endsWith(".json")) return "application/json; charset=UTF-8";
    if (fileName.endsWith(".png")) return "image/png";
    if (fileName.endsWith(".jpg") || fileName.endsWith(".jpeg")) return "image/jpeg";
    if (fileName.endsWith(".gif")) return "image/gif";
    if (fileName.endsWith(".svg")) return "image/svg+xml";
    return "application/octet-stream";
}

int HttpServer::getUserIdFromToken(const QString& token)
{
    // 简化的token解析，实际应该使用JWT
    // 这里假设token格式为 "userId_timestamp"
    QStringList parts = token.split('_');
    if (parts.size() >= 1) {
        return parts[0].toInt();
    }
    return 0;
}

QString HttpServer::generateToken(int userId)
{
    // 简化的token生成，实际应该使用JWT
    qint64 timestamp = QDateTime::currentSecsSinceEpoch();
    return QString("%1_%2").arg(userId).arg(timestamp);
}

QString HttpServer::hashPassword(const QString& password)
{
    // 使用MD5哈希（实际生产环境应使用更安全的bcrypt或argon2）
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Md5);
    return hash.toHex();
}

void HttpServer::apiChangePassword(QTcpSocket* socket, const QJsonObject& data)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    int userId = data["userId"].toInt();
    QString newPassword = data["newPassword"].toString();
    bool useEmailVerify = data["useEmailVerify"].toBool();
    
    qDebug() << "==== Change Password Request ====";
    qDebug() << "User ID:" << userId;
    qDebug() << "Use Email Verify:" << useEmailVerify;
    qDebug() << "New Password:" << (newPassword.isEmpty() ? "Empty" : "***");
    
    if (userId <= 0 || newPassword.isEmpty()) {
        qDebug() << "ERROR: Invalid parameters";
        response.body = QJsonDocument(createJsonResponse(false, "参数错误")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    QVariantMap user = DatabaseManager::instance()->getUserById(userId);
    if (user.isEmpty()) {
        qDebug() << "ERROR: User not found";
        response.body = QJsonDocument(createJsonResponse(false, "用户不存在")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    qDebug() << "User found:" << user["username"].toString();
    
    // 管理员限制：禁止管理员使用邮箱验证修改密码
    bool isAdmin = user["is_admin"].toBool();
    if (isAdmin && useEmailVerify) {
        qDebug() << "ERROR: Admin cannot use email verification";
        response.body = QJsonDocument(createJsonResponse(false, "管理员不允许使用邮箱验证修改密码，请使用旧密码验证")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    bool verified = false;
    
    if (useEmailVerify) {
        // 使用邮箱验证码验证
        QString code = data["code"].toString();
        QString email = user["email"].toString();
        QString savedCode = DatabaseManager::instance()->getVerificationCode(email, "change_password");
        
        qDebug() << "Email verification - Code received:" << code;
        qDebug() << "Saved code:" << savedCode;
        
        if (savedCode == code) {
            verified = true;
            DatabaseManager::instance()->deleteVerificationCode(email, "change_password");
            qDebug() << "Email verification: SUCCESS";
        } else {
            qDebug() << "Email verification: FAILED";
            response.body = QJsonDocument(createJsonResponse(false, "验证码错误或已过期")).toJson();
            sendResponse(socket, response);
            return;
        }
    } else {
        // 使用旧密码验证
        QString oldPassword = data["oldPassword"].toString();
        QString oldPasswordHash = hashPassword(oldPassword);
        QString email = user["email"].toString();
        QString username = user["username"].toString();
        QString storedHash = user["password_hash"].toString();
        
        qDebug() << "Old password verification";
        qDebug() << "Old password received:" << (oldPassword.isEmpty() ? "Empty" : "***");
        qDebug() << "Old password hash:" << oldPasswordHash;
        qDebug() << "Stored hash:" << storedHash;
        
        if (storedHash == oldPasswordHash) {
            verified = true;
            qDebug() << "Old password verification: SUCCESS";
        } else {
            qDebug() << "Old password verification: FAILED";
            // 密码错误，发送警告邮件
            if (!email.isEmpty()) {
                EmailService::instance()->sendPasswordChangeFailureWarning(email, username);
                qDebug() << "Password change failure warning sent to:" << email;
            }
            
            response.body = QJsonDocument(createJsonResponse(false, "当前密码错误")).toJson();
            sendResponse(socket, response);
            return;
        }
    }
    
    // 验证通过后，执行密码修改
    if (verified) {
        QString newPasswordHash = hashPassword(newPassword);
        qDebug() << "Updating password for user:" << userId;
        
        if (DatabaseManager::instance()->updatePassword(userId, newPasswordHash)) {
            qDebug() << "Password updated successfully";
            
            // 密码修改成功，如果是使用旧密码验证的方式，发送成功通知邮件
            if (!useEmailVerify) {
                QString email = user["email"].toString();
                QString username = user["username"].toString();
                if (!email.isEmpty()) {
                    EmailService::instance()->sendPasswordChangeSuccessNotification(email, username);
                    qDebug() << "Password change success notification sent to:" << email;
                }
            }
            
            response.body = QJsonDocument(createJsonResponse(true, "密码修改成功")).toJson();
            sendResponse(socket, response);
            qDebug() << "==== Change Password: SUCCESS ====";
        } else {
            qDebug() << "ERROR: Database update failed";
            response.body = QJsonDocument(createJsonResponse(false, "密码修改失败")).toJson();
            sendResponse(socket, response);
        }
    } else {
        qDebug() << "ERROR: Verification failed (should not reach here)";
        response.body = QJsonDocument(createJsonResponse(false, "验证失败")).toJson();
        sendResponse(socket, response);
    }
}

void HttpServer::apiGetAdminStats(QTcpSocket* socket, const HttpRequest& request)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    // 验证管理员权限
    QString token = request.headers.value("Authorization").replace("Bearer ", "");
    int userId = getUserIdFromToken(token);
    
    if (userId <= 0) {
        userId = 1; // 临时处理：默认允许访问
    }
    
    // 获取统计数据
    QSqlQuery query;
    QJsonObject stats;
    
    // 总用户数（排除管理员）
    query.exec("SELECT COUNT(*) as count FROM users WHERE is_admin = 0");
    if (query.next()) {
        stats["totalUsers"] = query.value("count").toInt();
    }
    
    // 总房源数
    query.exec("SELECT COUNT(*) as count FROM houseinfo");
    if (query.next()) {
        stats["totalHouses"] = query.value("count").toInt();
    }
    
    // 总收藏数
    query.exec("SELECT COUNT(*) as count FROM favorites");
    if (query.next()) {
        stats["totalFavorites"] = query.value("count").toInt();
    }
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", stats)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiGetAllUsers(QTcpSocket* socket, const HttpRequest& request)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    // 获取所有用户
    QSqlQuery query;
    query.exec("SELECT id, username, email, email_verified, is_admin, is_disabled, created_at, last_login FROM users ORDER BY id DESC");
    
    QJsonArray usersArray;
    while (query.next()) {
        QJsonObject userObj;
        userObj["id"] = query.value("id").toInt();
        userObj["username"] = query.value("username").toString();
        userObj["email"] = query.value("email").toString();
        userObj["email_verified"] = query.value("email_verified").toBool();
        userObj["is_admin"] = query.value("is_admin").toBool();
        userObj["is_disabled"] = query.value("is_disabled").toBool();
        userObj["created_at"] = query.value("created_at").toString();
        
        if (!query.value("last_login").isNull()) {
            userObj["last_login"] = query.value("last_login").toString();
        }
        
        usersArray.append(userObj);
    }
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", usersArray)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiToggleUserStatus(QTcpSocket* socket, const QJsonObject& data)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    int userId = data["userId"].toInt();
    bool disabled = data["disabled"].toBool();
    
    if (userId <= 0) {
        response.body = QJsonDocument(createJsonResponse(false, "无效的用户ID")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    // 检查是否为管理员
    QVariantMap user = DatabaseManager::instance()->getUserById(userId);
    if (user["is_admin"].toBool()) {
        response.body = QJsonDocument(createJsonResponse(false, "不能禁用管理员账户")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    if (DatabaseManager::instance()->toggleUserDisabled(userId, disabled)) {
        QString msg = disabled ? "用户已禁用" : "用户已启用";
        response.body = QJsonDocument(createJsonResponse(true, msg)).toJson();
    } else {
        response.body = QJsonDocument(createJsonResponse(false, "操作失败")).toJson();
    }
    
    sendResponse(socket, response);
}

void HttpServer::apiGetPopularHouses(QTcpSocket* socket)
{
    qDebug() << "=== apiGetPopularHouses called ===";
    
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    QList<QVariantMap> houses = DatabaseManager::instance()->getPopularHouses(10);
    
    qDebug() << "Got" << houses.size() << "popular houses from database";
    
    QJsonArray housesArray;
    
    for (const auto& house : houses) {
        QJsonObject obj;
        for (auto it = house.begin(); it != house.end(); ++it) {
            obj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        housesArray.append(obj);
    }
    
    qDebug() << "Returning" << housesArray.size() << "houses in response";
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", housesArray)).toJson();
    sendResponse(socket, response);
}

void HttpServer::apiGetConfig(QTcpSocket* socket)
{
    HttpResponse response;
    response.headers["Content-Type"] = "application/json; charset=UTF-8";
    response.headers["Access-Control-Allow-Origin"] = "*";
    
    // 读取配置文件
    QFile file("config/config.json");
    if (!file.open(QIODevice::ReadOnly)) {
        response.body = QJsonDocument(createJsonResponse(false, "配置文件读取失败")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        response.body = QJsonDocument(createJsonResponse(false, "配置文件格式错误")).toJson();
        sendResponse(socket, response);
        return;
    }
    
    QJsonObject config = doc.object();
    
    // 只返回前端需要的公开配置（不包含敏感信息）
    QJsonObject publicConfig;
    
    // 百度地图配置
    if (config.contains("baidu_map")) {
        QJsonObject baiduMap;
        baiduMap["api_key"] = config["baidu_map"].toObject()["api_key"];
        publicConfig["baidu_map"] = baiduMap;
    }
    
    response.body = QJsonDocument(createJsonResponse(true, "成功", publicConfig)).toJson();
    sendResponse(socket, response);
}

