#ifndef DEEPSEEKCLIENT_H
#define DEEPSEEKCLIENT_H

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QList>
#include <QObject>
#include <functional>
#include "../HouseData.h"

class DeepSeekClient : public QObject
{
    Q_OBJECT

public:
    explicit DeepSeekClient(QObject *parent = nullptr);
    ~DeepSeekClient();

    // 初始化API配置（从环境变量读取API密钥）
    bool initialize();

    // 设置回调函数
    void setCallbacks(
        std::function<void(const QString&)> onReportReady,
        std::function<void(const QString&)> onError
    );

    // 数据输入接口：接收数据库中的房源数据
    void setHouseData(const QList<HouseData> &houseDataList);

    // 数据输出接口：获取AI分析报告
    void requestAnalysisReport();

    // 格式转换函数：将房源数据转换为JSON格式
    QString convertHouseDataToJson(const QList<HouseData> &houseDataList);

signals:
    // AI分析完成信号
    void responseReceived(const QString& result);

    // 错误发生信号
    void errorOccurred(const QString& error);

public:
    // 直接发送消息
    void sendMessage(const QString &message);

private slots:
    void onApiReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_manager;
    QString m_apiKey;
    QString m_apiUrl;
    QList<HouseData> m_houseDataList;

    // 回调函数
    std::function<void(const QString&)> m_onReportReady;
    std::function<void(const QString&)> m_onError;

    // 发送API请求的内部函数
    void sendApiRequest(const QString &prompt);

    enum RequestType {
        AnalysisReport
    };
};

#endif // DEEPSEEKCLIENT_H
