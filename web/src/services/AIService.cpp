#include "AIService.h"
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>

AIService* AIService::m_instance = nullptr;

AIService* AIService::instance()
{
    if (!m_instance) {
        m_instance = new AIService();
    }
    return m_instance;
}

AIService::AIService(QObject *parent)
    : QObject(parent)
    , maxTokens(2000)
    , temperature(0.7)
{
}

AIService::~AIService()
{
}

bool AIService::initialize(const QJsonObject& config)
{
    apiKey = config["api_key"].toString();
    apiUrl = config["api_url"].toString();
    model = config["model"].toString();
    maxTokens = config["max_tokens"].toInt(2000);
    temperature = config["temperature"].toDouble(0.7);
    
    qDebug() << "AI Service initialized with model:" << model;
    return !apiKey.isEmpty() && !apiUrl.isEmpty();
}

QString AIService::recommendHouses(const QString& userRequirement, const QList<QVariantMap>& houses)
{
    // 构建房产数据摘要
    QString housesData = "可用房源列表：\n";
    for (int i = 0; i < qMin(houses.size(), 20); ++i) {
        const auto& house = houses[i];
        housesData += QString("%1. %2 - 价格:%3万 面积:%4㎡ 单价:%5元/㎡ 户型:%6 小区:%7\n")
            .arg(i + 1)
            .arg(house["houseTitle"].toString())
            .arg(house["price"].toDouble())
            .arg(house["area"].toDouble())
            .arg(house["unitPrice"].toDouble())
            .arg(house["houseType"].toString())
            .arg(house["communityName"].toString());
    }
    
    // 构建系统提示
    QString systemPrompt = R"(
        你是一个专业的房产推荐助手。根据用户的需求和提供的房源数据，
        为用户推荐最合适的房产。请分析用户需求中的关键因素（如价格预算、
        面积要求、地段偏好等），然后从房源列表中选出最匹配的3-5个房源，
        并解释推荐理由。回答要专业、友好、有条理。
    )";
    
    // 构建消息数组
    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", systemPrompt}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", housesData + "\n用户需求：" + userRequirement}
    });
    
    return callDeepSeekAPI(messages);
}

QString AIService::chat(const QString& userMessage, const QJsonArray& chatHistory)
{
    QJsonArray messages;
    
    // 添加系统提示
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", "你是一个专业的房产咨询助手，可以帮助用户了解房产信息、购房建议等。"}
    });
    
    // 添加历史对话
    for (const auto& item : chatHistory) {
        messages.append(item);
    }
    
    // 添加当前用户消息
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", userMessage}
    });
    
    return callDeepSeekAPI(messages);
}

QString AIService::callDeepSeekAPI(const QJsonArray& messages)
{
    QNetworkAccessManager manager;
    QUrl url(apiUrl);
    QNetworkRequest request(url);
    
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    
    // 构建请求体
    QJsonObject requestBody;
    requestBody["model"] = model;
    requestBody["messages"] = messages;
    requestBody["max_tokens"] = maxTokens;
    requestBody["temperature"] = temperature;
    
    QJsonDocument doc(requestBody);
    QByteArray requestData = doc.toJson();
    
    qDebug() << "Calling DeepSeek API...";
    
    // 发送POST请求
    QNetworkReply *reply = manager.post(request, requestData);
    
    // 等待响应
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    // 处理响应
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "DeepSeek API error:" << reply->errorString();
        reply->deleteLater();
        return "抱歉，AI服务暂时不可用，请稍后再试。";
    }
    
    QByteArray responseData = reply->readAll();
    reply->deleteLater();
    
    // 解析响应
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    QJsonObject responseObj = responseDoc.object();
    
    if (responseObj.contains("choices")) {
        QJsonArray choices = responseObj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject firstChoice = choices[0].toObject();
            QJsonObject message = firstChoice["message"].toObject();
            QString content = message["content"].toString();
            qDebug() << "AI response received";
            return content;
        }
    }
    
    if (responseObj.contains("error")) {
        QJsonObject error = responseObj["error"].toObject();
        qWarning() << "DeepSeek API error:" << error["message"].toString();
        return "AI处理出错：" + error["message"].toString();
    }
    
    return "抱歉，未能获取有效的AI响应。";
}
