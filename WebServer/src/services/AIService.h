#ifndef AISERVICE_H
#define AISERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVariantMap>

class AIService : public QObject
{
    Q_OBJECT

public:
    static AIService* instance();
    
    bool initialize(const QJsonObject& config);
    
    // AI推荐房产
    QString recommendHouses(const QString& userRequirement, const QList<QVariantMap>& houses);
    
    // 智能问答
    QString chat(const QString& userMessage, const QJsonArray& chatHistory = QJsonArray());

private:
    explicit AIService(QObject *parent = nullptr);
    ~AIService();
    
    QString callDeepSeekAPI(const QJsonArray& messages);
    
    QString apiKey;
    QString apiUrl;
    QString model;
    int maxTokens;
    double temperature;
    
    static AIService* m_instance;
};

#endif // AISERVICE_H
