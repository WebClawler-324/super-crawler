#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include "database/DatabaseManager.h"
#include "services/EmailService.h"
#include "services/AIService.h"
#include "server/HttpServer.h"

bool loadConfig(const QString& configPath, QJsonObject& config)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open config file:" << configPath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qCritical() << "Invalid config file format";
        return false;
    }
    
    config = doc.object();
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "========================================";
    qDebug() << "二手房信息服务平台启动中...";
    qDebug() << "========================================";
    
    // 加载配置
    QJsonObject config;
    QString configPath = "config/config.json";
    
    if (!loadConfig(configPath, config)) {
        qCritical() << "无法加载配置文件，请检查" << configPath;
        return 1;
    }
    
    qDebug() << "配置文件加载成功";
    
    // 初始化数据库
    qDebug() << "正在连接数据库...";
    if (!DatabaseManager::instance()->initialize(config["database"].toObject())) {
        qCritical() << "数据库初始化失败";
        return 1;
    }
    qDebug() << "数据库连接成功";
    
    // 初始化邮件服务
    qDebug() << "正在初始化邮件服务...";
    if (!EmailService::instance()->initialize(config["email"].toObject())) {
        qWarning() << "邮件服务初始化失败";
    } else {
        qDebug() << "邮件服务初始化成功";
    }
    
    // 初始化AI服务
    qDebug() << "正在初始化AI服务...";
    if (!AIService::instance()->initialize(config["deepseek"].toObject())) {
        qWarning() << "AI服务初始化失败";
    } else {
        qDebug() << "AI服务初始化成功";
    }
    
    // 启动HTTP服务器
    qDebug() << "正在启动HTTP服务器...";
    HttpServer server;
    QJsonObject serverConfig = config["server"].toObject();
    QString host = serverConfig["host"].toString("0.0.0.0");
    quint16 port = serverConfig["port"].toInt(8080);
    
    if (!server.start(host, port)) {
        qCritical() << "HTTP服务器启动失败";
        return 1;
    }
    
    qDebug() << "========================================";
    qDebug() << "服务器启动成功！";
    qDebug() << "访问地址: http://" << (host == "0.0.0.0" ? "localhost" : host) << ":" << port;
    qDebug() << "按 Ctrl+C 停止服务器";
    qDebug() << "========================================";
    
    return app.exec();
}
