#include "DeepSeekClient.h"
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QProcessEnvironment>

DeepSeekClient::DeepSeekClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_apiUrl("https://api.deepseek.com/v1/chat/completions")  // DeepSeek API端点
{
    connect(m_manager, &QNetworkAccessManager::finished, this, &DeepSeekClient::onApiReplyFinished);
}

DeepSeekClient::~DeepSeekClient()
{
    m_manager->deleteLater();
}

bool DeepSeekClient::initialize()
{
    // 从环境变量获取API密钥
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    m_apiKey = env.value("Deepseek_test1");

    if (m_apiKey.isEmpty()) {
        if (m_onError) {
            m_onError("错误：未找到环境变量 'Deepseek_test1'，请确保已正确配置DeepSeek API密钥");
        }
        return false;
    }

    qDebug() << "DeepSeekClient: 初始化成功，API密钥已配置";
    return true;
}

void DeepSeekClient::setCallbacks(
    std::function<void(const QString&)> onReportReady,
    std::function<void(const QString&)> onError
)
{
    m_onReportReady = onReportReady;
    m_onError = onError;
}

void DeepSeekClient::setHouseData(const QList<HouseData> &houseDataList)
{
    m_houseDataList = houseDataList;
    qDebug() << "DeepSeekClient: 接收到" << houseDataList.size() << "条房源数据";

    // 自动进行数据转换
    QString jsonData = convertHouseDataToJson(houseDataList);
    // 数据转换完成，可以在这里调用回调或直接返回
}

QString DeepSeekClient::convertHouseDataToJson(const QList<HouseData> &houseDataList)
{
    QJsonArray houseArray;

    for (const HouseData &house : houseDataList) {
        QJsonObject houseObj;
        houseObj["communityName"] = house.communityName;
        houseObj["area"] = house.area;
        houseObj["totalPrice"] = house.price;  // 使用HouseData.h中的price字段
        houseObj["unitPrice"] = house.unitPrice;
        houseObj["floor"] = house.floor;
        houseObj["orientation"] = house.orientation;
        houseObj["buildingYear"] = house.buildingYear;
        houseObj["houseType"] = house.houseType;

        houseArray.append(houseObj);
    }

    QJsonDocument doc(houseArray);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    qDebug() << "DeepSeekClient: 数据转换完成，JSON长度:" << jsonString.length();
    return jsonString;
}

void DeepSeekClient::requestAnalysisReport()
{
    if (m_houseDataList.isEmpty()) {
        if (m_onError) {
            m_onError("错误：没有房源数据，请先调用setHouseData设置数据");
        }
        return;
    }

    if (m_apiKey.isEmpty()) {
        if (m_onError) {
            m_onError("错误：API密钥未配置，请先调用initialize()");
        }
        return;
    }

    // 转换数据为JSON格式
    QString rawJson = convertHouseDataToJson(m_houseDataList);

    // 使用用户指定的提示词
    QString prompt = "基于以下房源JSON数据，生成1份简洁的中文分析报告（300-500字）："
                     "1. 核心结论：房源数量、均价、主力户型；"
                     "2. 性价比推荐：1-3套总价低/户型好的房源；"
                     "3. 购买建议：1-3条针对性建议；"
                     "格式要求：用换行分隔，无Markdown，无特殊符号，纯文本！"
                     "房源数据：" + rawJson;

    sendApiRequest(prompt);
}

void DeepSeekClient::sendMessage(const QString &message)
{
    if (m_apiKey.isEmpty()) {
        if (m_onError) {
            m_onError("错误：API密钥未配置，请先调用initialize()");
        }
        return;
    }

    sendApiRequest(message);
}

void DeepSeekClient::sendApiRequest(const QString &prompt)
{
    QNetworkRequest request;
    request.setUrl(QUrl(m_apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    request.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    // 构造DeepSeek API请求体
    QJsonObject requestJson;
    requestJson["model"] = "deepseek-chat";  // DeepSeek模型名称

    QJsonArray messages;
    QJsonObject message;
    message["role"] = "user";
    message["content"] = prompt;
    messages.append(message);
    requestJson["messages"] = messages;

    // 设置参数
    requestJson["temperature"] = 0.3;
    requestJson["max_tokens"] = 2000;

    QByteArray requestData = QJsonDocument(requestJson).toJson();
    QNetworkReply *reply = m_manager->post(request, requestData);
    reply->setProperty("requestType", AnalysisReport);

    qDebug() << "DeepSeekClient: 发送分析报告请求";
}

void DeepSeekClient::onApiReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray errorResponse = reply->readAll();
        QString errorMsg = QString("DeepSeek API网络错误：状态码=%1，错误描述=%2，服务器响应=%3")
                          .arg(statusCode)
                          .arg(reply->errorString())
                          .arg(QString(errorResponse).left(200));
        emit errorOccurred(errorMsg);
        if (m_onError) {
            m_onError(errorMsg);
        }
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);

    if (!responseDoc.isObject()) {
        QString errorMsg = "DeepSeek API响应错误：非JSON格式，响应：" + QString(responseData).left(200);
        emit errorOccurred(errorMsg);
        if (m_onError) {
            m_onError(errorMsg);
        }
        reply->deleteLater();
        return;
    }

    QJsonObject responseObj = responseDoc.object();
    RequestType requestType = static_cast<RequestType>(reply->property("requestType").toInt());
    reply->deleteLater();

    // 检查DeepSeek API响应格式
    if (responseObj.contains("choices") && responseObj["choices"].isArray()) {
        QJsonArray choices = responseObj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject choice = choices[0].toObject();
            if (choice.contains("message") && choice["message"].isObject()) {
                QJsonObject message = choice["message"].toObject();
                QString content = message["content"].toString().trimmed();

                if (requestType == AnalysisReport) {
                    emit responseReceived(content);
                    if (m_onReportReady) {
                        m_onReportReady(content);
                    }
                    qDebug() << "DeepSeekClient: 分析报告生成完成，长度:" << content.length();
                }
                return;
            }
        }
    }

    // 检查错误信息
    if (responseObj.contains("error")) {
        QJsonObject errorObj = responseObj["error"].toObject();
        QString errorMsg = "DeepSeek API错误：" + errorObj["message"].toString();
        emit errorOccurred(errorMsg);
        if (m_onError) {
            m_onError(errorMsg);
        }
        return;
    }

    QString errorDetail = "响应数据：" + QString(responseData).left(200);
    QString errorMsg = "DeepSeek API响应格式错误：" + errorDetail;
    emit errorOccurred(errorMsg);
    if (m_onError) {
        m_onError(errorMsg);
    }
}
