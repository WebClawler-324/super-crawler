#include "LLMClient.h"
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

LLMClient::LLMClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    connect(m_manager, &QNetworkAccessManager::finished, this, &LLMClient::onApiReplyFinished);
}

LLMClient::~LLMClient()
{
    m_manager->deleteLater();
}

void LLMClient::setApiConfig(const QString &apiKey, const QString &apiUrl)
{
    m_apiKey = apiKey;
    m_apiUrl = apiUrl;
    qDebug() << "LLMClient: API config done, url:" << m_apiUrl.left(50) << "...";
}

// 数据清洗：完全改用半角符号+普通字符串拼接
void LLMClient::cleanHouseData(const QString &rawJson)
{
    if (m_apiKey.isEmpty() || m_apiUrl.isEmpty()) {
        emit errorOccurred("LLM Error: API key or url not set");
        return;
    }

    // 关键修正：用"+"拼接字符串，所有符号都是半角，无中文特殊字符
    QString prompt = "You are a house data cleaning expert. Standardize the following JSON house data with these rules:"
                     "1. House type: Unify format, e.g., '3室一厅'→'3室1厅', '2室2卫' remains, '未知' remains;"
                     "2. Area: Unify format to 'XX.X ㎡', e.g., '89.5平米'→'89.5 ㎡', '120平'→'120.0 ㎡', keep 1 decimal;"
                     "3. Price: Unify format, e.g., '500万'→'500.0 万', '350,000元/平'→'350000 元/㎡', remove commas;"
                     "4. Floor: Remove extra spaces, e.g., '低楼层 （共18层）'→'低楼层(共18层)';"
                     "5. Orientation: Merge duplicates, e.g., '南 南'→'南', keep '东南、西南' etc.;"
                     "6. Build year: Unify format, e.g., '2010'→'2010年', '未知' remains;"
                     "7. Keep all fields (city, houseTitle, communityName etc.), no add/delete;"
                     "8. Output only the cleaned JSON string, no extra text/comment!\n"
                     "Raw data:" + rawJson;

    // 构造请求（语法正确，分步创建避免嵌套错误）
    QNetworkRequest request;
    request.setUrl(QUrl(m_apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    request.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    QJsonObject requestJson;
    QJsonObject inputObj;
    QJsonArray messagesArr;
    QJsonObject messageObj;
    messageObj["role"] = "user";
    messageObj["content"] = prompt;
    messagesArr.append(messageObj);
    inputObj["messages"] = messagesArr;
    requestJson["model"] = "qwen-turbo";
    requestJson["input"] = inputObj;

    QJsonObject paramsObj;
    paramsObj["result_format"] = "json";
    paramsObj["temperature"] = 0.1;
    requestJson["parameters"] = paramsObj;

    QByteArray requestData = QJsonDocument(requestJson).toJson();
    QNetworkReply *reply = m_manager->post(request, requestData);
    reply->setProperty("requestType", CleanData);
    qDebug() << "LLMClient: Clean data request sent";
}

// 生成报告：同样用半角符号+普通字符串拼接
void LLMClient::generateHouseReport(const QString &rawJson)
{
    if (m_apiKey.isEmpty() || m_apiUrl.isEmpty()) {
        emit errorOccurred("LLM Error: API key or url not set");
        return;
    }

    // 关键修正：Prompt用英文（彻底避免中文符号冲突），大模型同样能生成中文报告
    QString prompt = "You are a senior real estate analyst. Generate a professional concise report based on the following JSON house data. "
                     "Report structure:"
                     "1. Data Overview: Total houses, total communities, average price, price range (min-max);"
                     "2. Core Analysis: House type distribution (percentage), price distribution (per 500k interval), area distribution (per 30㎡ interval);"
                     "3. Cost-Effective Recommendations: 2-3 houses (suitable area, unit price below average, good floor/orientation);"
                     "4. Purchase Suggestions: 1-2 targeted suggestions based on data."
                     "Format requirements:"
                     "- Clear points with titles + bullet points, no JSON;"
                     "- Concise professional language, within 500 words;"
                     "Unit: Price in '万', area in '㎡', keep 1 decimal."
                     "Note: Only use provided data, no fabrication. Generate report in Chinese!\n"
                     "House data:" + rawJson;

    QNetworkRequest request;
    request.setUrl(QUrl(m_apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    request.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    QJsonObject requestJson;
    QJsonObject inputObj;
    QJsonArray messagesArr;
    QJsonObject messageObj;
    messageObj["role"] = "user";
    messageObj["content"] = prompt;
    messagesArr.append(messageObj);
    inputObj["messages"] = messagesArr;
    requestJson["model"] = "qwen-turbo";
    requestJson["input"] = inputObj;

    QJsonObject paramsObj;
    paramsObj["result_format"] = "text";
    paramsObj["temperature"] = 0.3;
    requestJson["parameters"] = paramsObj;

    QByteArray requestData = QJsonDocument(requestJson).toJson();
    QNetworkReply *reply = m_manager->post(request, requestData);
    reply->setProperty("requestType", GenerateReport);
    qDebug() << "LLMClient: Report request sent";
}

// 响应处理（无修改，确保语法正确）
void LLMClient::onApiReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        // 1. 获取 HTTP 状态码（关键！判断是 401/403/404 等）
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 2. 获取服务器返回的错误响应（如果有）
        QByteArray errorResponse = reply->readAll();
        // 3. 拼接详细错误日志
        QString errorMsg = QString("LLM 网络错误：")
                           + "状态码=" + QString::number(statusCode) + "，"
                           + "错误描述=" + reply->errorString() + "，"
                           + "服务器响应=" + QString(errorResponse).left(200);
        emit errorOccurred(errorMsg);
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    // 打印完整响应（前500字符），方便排查格式问题
    emit errorOccurred("📥 大模型响应（前500字符）：" + QString(responseData).left(500));

    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    if (!responseDoc.isObject()) {
        emit errorOccurred("LLM 响应错误：非 JSON 格式，响应：" + QString(responseData).left(200));
        reply->deleteLater();
        return;
    }
    QJsonObject responseObj = responseDoc.object();
    LLMClient::RequestType requestType = static_cast<LLMClient::RequestType>(reply->property("requestType").toInt());
    reply->deleteLater();

    if (responseObj.contains("output") && responseObj["output"].isObject()) {
        QJsonObject outputObj = responseObj["output"].toObject();
        if (outputObj.contains("choices") && outputObj["choices"].isArray()) {
            QJsonArray choicesArr = outputObj["choices"].toArray();
            if (!choicesArr.isEmpty()) {
                QJsonObject choiceObj = choicesArr[0].toObject();
                if (choiceObj.contains("message") && choiceObj["message"].isObject()) {
                    QJsonObject msgObj = choiceObj["message"].toObject();
                    QString content = msgObj["content"].toString().trimmed();

                    if (requestType == LLMClient::CleanData) {
                        emit dataCleaned(content);
                        qDebug() << "LLMClient: Clean data done, length:" << content.length();
                    } else if (requestType == LLMClient::GenerateReport) {
                        emit reportGenerated(content);
                        qDebug() << "LLMClient: Report done, length:" << content.length();
                    }
                    return;
                }
            }
        }
    }

    QString errorDetail = "Response data:" + QString(responseData).left(200);
    emit errorOccurred("LLM Response Error: Invalid format, " + errorDetail);
}
