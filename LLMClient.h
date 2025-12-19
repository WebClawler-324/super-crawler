#ifndef LLMCLIENT_H
#define LLMCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray> // 新增：补全QJsonArray头文件
#include <QString>

class LLMClient : public QObject
{
    Q_OBJECT

public:
    explicit LLMClient(QObject *parent = nullptr);
    ~LLMClient();

    void setApiConfig(const QString &apiKey, const QString &apiUrl);

public slots:
    void cleanHouseData(const QString &rawJson);
    void generateHouseReport(const QString &rawJson);

signals:
    void dataCleaned(const QString &cleanedJson);
    void reportGenerated(const QString &report);
    void errorOccurred(const QString &errorMsg);

private slots:
    void onApiReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_manager;
    QString m_apiKey;
    QString m_apiUrl;
    enum RequestType {
        CleanData,
        GenerateReport
    };
};

#endif // LLMCLIENT_H
