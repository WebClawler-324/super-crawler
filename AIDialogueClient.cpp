#include "AIDialogueClient.h"
#include <QRegularExpression>
#include <QDebug>

AIDialogueClient::AIDialogueClient(QObject *parent)
    : QObject(parent), m_database(nullptr), m_aiClient(nullptr)
{
    m_aiClient = new DeepSeekClient(this);
    connect(m_aiClient, &DeepSeekClient::responseReceived,
            this, &AIDialogueClient::onAiResponseReceived);
    connect(m_aiClient, &DeepSeekClient::errorOccurred,
            this, &AIDialogueClient::onAiError);
}

AIDialogueClient::~AIDialogueClient()
{
    if (m_aiClient) {
        m_aiClient->deleteLater();
    }
}

void AIDialogueClient::setDatabase(Mysql* db)
{
    m_database = db;
}

void AIDialogueClient::processUserQuery(const QString& userQuery)
{
    if (!m_database) {
        emit dialogueError("数据库未连接");
        return;
    }

    if (!m_aiClient->initialize()) {
        emit dialogueError("AI客户端初始化失败，请检查API密钥");
        return;
    }

    // 解析用户查询
    auto [queryType, params] = parseQuery(userQuery);

    // 执行数据库查询
    QList<HouseData> results = executeDatabaseQuery(queryType, params);

    // 生成AI提示词并发送
    QString prompt = generateAiPrompt(userQuery, results);
    m_aiClient->sendMessage(prompt);
}

QPair<QString, QVariantMap> AIDialogueClient::parseQuery(const QString& query)
{
    QString queryType;
    QVariantMap params;

    // 价格查询模式
    QRegularExpression priceRegex("(\\d+)[万到至~]*(\\d+)?万");
    QRegularExpressionMatch priceMatch = priceRegex.match(query);

    if (priceMatch.hasMatch()) {
        queryType = "price";
        double minPrice = priceMatch.captured(1).toDouble();
        double maxPrice = priceMatch.captured(2).isEmpty() ?
                         minPrice + 50 : priceMatch.captured(2).toDouble(); // 如果只给了一个价格，默认范围±50万

        params["minPrice"] = minPrice;
        params["maxPrice"] = maxPrice;
        return qMakePair(queryType, params);
    }

    // 户型查询模式
    if (query.contains("室")) {
        QRegularExpression roomRegex("(\\d+)室");
        QRegularExpressionMatch roomMatch = roomRegex.match(query);

        if (roomMatch.hasMatch()) {
            queryType = "type";
            params["houseType"] = roomMatch.captured(1) + "室";
            return qMakePair(queryType, params);
        }
    }

    // 面积查询模式
    QRegularExpression areaRegex("(\\d+)[到至~]*(\\d+)?平方");
    QRegularExpressionMatch areaMatch = areaRegex.match(query);

    if (areaMatch.hasMatch()) {
        queryType = "area";
        double minArea = areaMatch.captured(1).toDouble();
        double maxArea = areaMatch.captured(2).isEmpty() ?
                        minArea + 50 : areaMatch.captured(2).toDouble();

        params["minArea"] = minArea;
        params["maxArea"] = maxArea;
        return qMakePair(queryType, params);
    }

    // 默认查询所有数据
    queryType = "all";
    return qMakePair(queryType, params);
}

QList<HouseData> AIDialogueClient::executeDatabaseQuery(const QString& queryType, const QVariantMap& params)
{
    if (!m_database) {
        return QList<HouseData>();
    }

    if (queryType == "price") {
        double minPrice = params["minPrice"].toDouble();
        double maxPrice = params["maxPrice"].toDouble();
        return m_database->findHousesByPrice(minPrice, maxPrice);
    }
    else if (queryType == "type") {
        QString houseType = params["houseType"].toString();
        return m_database->findHousesByType(houseType);
    }
    else if (queryType == "area") {
        double minArea = params["minArea"].toDouble();
        double maxArea = params["maxArea"].toDouble();
        return m_database->findHousesByArea(minArea, maxArea);
    }
    else {
        // 默认返回所有数据
        return m_database->getAllHouseData();
    }
}

QString AIDialogueClient::generateAiPrompt(const QString& userQuery, const QList<HouseData>& results)
{
    QString houseDataStr = formatHouseData(results);

    QString prompt = QString(
        "用户询问：%1\n\n"
        "基于以下房源数据，请用中文回答用户的问题，给出详细的解释和建议：\n"
        "%2\n\n"
        "请提供：\n"
        "1. 直接回答用户的问题\n"
        "2. 列出符合条件的房源详情\n"
        "3. 给出购房建议\n"
        "回答要简洁明了，有条理"
    ).arg(userQuery, houseDataStr);

    return prompt;
}

QString AIDialogueClient::formatHouseData(const QList<HouseData>& data)
{
    if (data.isEmpty()) {
        return "暂无符合条件的房源数据。";
    }

    QString result = QString("找到 %1 条符合条件的房源：\n\n").arg(data.size());

    for (int i = 0; i < data.size() && i < 10; ++i) { // 最多显示10条
        const HouseData& house = data[i];
        result += QString("房源 %1：\n").arg(i + 1);
        result += QString("- 小区：%1\n").arg(house.communityName);
        result += QString("- 总价：%1\n").arg(house.price);
        result += QString("- 单价：%1\n").arg(house.unitPrice);
        result += QString("- 户型：%1\n").arg(house.houseType);
        result += QString("- 面积：%1\n").arg(house.area);
        result += QString("- 楼层：%1\n").arg(house.floor);
        result += QString("- 朝向：%1\n").arg(house.orientation);
        result += QString("- 年代：%1\n").arg(house.buildingYear);
        result += "\n";
    }

    if (data.size() > 10) {
        result += QString("...还有 %1 条房源未显示\n").arg(data.size() - 10);
    }

    return result;
}

void AIDialogueClient::onAiResponseReceived(const QString& response)
{
    emit dialogueCompleted(response);
}

void AIDialogueClient::onAiError(const QString& error)
{
    emit dialogueError("AI回答失败：" + error);
}
