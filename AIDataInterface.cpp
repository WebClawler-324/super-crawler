#include "AIDataInterface.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QDebug>

/**
 * AIDataInterface类的实现
 *
 * 提供数据传入和传出接口，连接数据库和AI分析功能
 */

AIDataInterface::AIDataInterface(QObject *parent)
    : QObject(parent)
    , m_database(nullptr)
    , m_aiClient(nullptr)
    , m_lastDataCount(0)
{
}

AIDataInterface::~AIDataInterface()
{
    // 不需要手动删除m_database和m_aiClient，因为它们由外部管理
}

/**
 * 设置数据库连接
 *
 * 队友需要调用此函数设置数据库对象
 */
void AIDataInterface::setDatabaseConnection(Mysql* db)
{
    m_database = db;
    qDebug() << "AIDataInterface: 数据库连接已设置";
}

/**
 * 设置AI客户端
 *
 * 队友需要调用此函数设置AI客户端对象
 */
void AIDataInterface::setAIClient(DeepSeekClient* aiClient)
{
    m_aiClient = aiClient;

    // 连接AI客户端的信号
    if (m_aiClient) {
        connect(m_aiClient, &DeepSeekClient::responseReceived,
                this, &AIDataInterface::onAIResponseReceived);
        connect(m_aiClient, &DeepSeekClient::errorOccurred,
                this, &AIDataInterface::onAIError);
    }

    qDebug() << "AIDataInterface: AI客户端已设置";
}

// ==================== 数据传入接口实现 ====================

/**
 * 数据传入接口 - 从数据库获取并传递给AI分析
 *
 * 主要外部接口函数，队友调用此函数启动完整的数据分析流程
 */
bool AIDataInterface::importDataAndAnalyze(const QString& queryCondition)
{
    // 1. 检查必要的组件是否已设置
    if (!m_database) {
        emit analysisError("错误：数据库连接未设置，请先调用setDatabaseConnection()");
        return false;
    }

    if (!m_aiClient) {
        emit analysisError("错误：AI客户端未设置，请先调用setAIClient()");
        return false;
    }

    // 2. 初始化AI客户端
    if (!m_aiClient->initialize()) {
        emit analysisError("错误：AI客户端初始化失败，请检查API密钥配置");
        return false;
    }

    emit analysisProgress(10, "正在从数据库获取数据...");

    // 3. 从数据库获取数据
    QList<HouseData> houseDataList = fetchHouseDataFromDatabase(queryCondition);

    if (houseDataList.isEmpty()) {
        emit analysisError("错误：数据库中没有找到符合条件的数据");
        return false;
    }

    emit analysisProgress(30, QString("已获取%1条房源数据，正在格式化...").arg(houseDataList.size()));

    // 4. 格式化数据
    QString formattedData = formatDataForAI(houseDataList);

    emit analysisProgress(50, "数据格式化完成，正在发送给AI分析...");

    // 5. 发送给AI分析
    if (!sendToAIAnalysis(formattedData)) {
        emit analysisError("错误：发送数据给AI失败");
        return false;
    }

    emit analysisProgress(70, "已发送给AI，正在等待分析结果...");
    return true;
}

/**
 * 直接传入房源数据进行AI分析
 *
 * 如果队友已经有数据，可以直接传入而不需要从数据库获取
 */
bool AIDataInterface::analyzeHouseData(const QList<HouseData>& houseDataList)
{
    if (!m_aiClient) {
        emit analysisError("错误：AI客户端未设置，请先调用setAIClient()");
        return false;
    }

    if (!m_aiClient->initialize()) {
        emit analysisError("错误：AI客户端初始化失败，请检查API密钥配置");
        return false;
    }

    if (houseDataList.isEmpty()) {
        emit analysisError("错误：传入的数据为空");
        return false;
    }

    emit analysisProgress(20, QString("正在处理%1条房源数据...").arg(houseDataList.size()));

    // 格式化数据
    QString formattedData = formatDataForAI(houseDataList);

    emit analysisProgress(40, "数据格式化完成，正在发送给AI分析...");

    // 发送给AI分析
    if (!sendToAIAnalysis(formattedData)) {
        emit analysisError("错误：发送数据给AI失败");
        return false;
    }

    emit analysisProgress(60, "已发送给AI，正在等待分析结果...");
    return true;
}

// ==================== 数据传出接口实现 ====================

/**
 * 数据传出接口 - 导出AI分析结果
 *
 * 支持多种导出格式：json, text, html
 */
QString AIDataInterface::exportAnalysisResult(const QString& format, const QString& filePath)
{
    if (m_latestAnalysisResult.isEmpty()) {
        return "错误：没有可导出的分析结果";
    }

    QString exportContent;

    if (format.toLower() == "json") {
        exportContent = exportAsJson(m_latestAnalysisResult);
    } else if (format.toLower() == "html") {
        exportContent = exportAsHtml(m_latestAnalysisResult);
    } else {
        // 默认text格式
        exportContent = exportAsText(m_latestAnalysisResult);
    }

    // 如果指定了文件路径，则保存到文件
    if (!filePath.isEmpty()) {
        if (saveToFile(exportContent, filePath)) {
            return QString("分析结果已保存到文件：%1").arg(filePath);
        } else {
            return "错误：保存文件失败";
        }
    }

    // 否则直接返回内容
    return exportContent;
}

/**
 * 获取最新的分析结果
 */
QString AIDataInterface::getLatestAnalysisResult() const
{
    return m_latestAnalysisResult;
}

// ==================== 私有辅助函数实现 ====================

/**
 * 从数据库获取房源数据
 *
 * 根据查询条件从数据库获取数据
 * 队友可以在这里添加更多的查询条件处理逻辑
 */
QList<HouseData> AIDataInterface::fetchHouseDataFromDatabase(const QString& condition)
{
    Q_UNUSED(condition) // 当前版本暂时不支持复杂查询条件

    // 调用数据库接口获取所有房源数据
    // 队友需要确保m_database已经正确连接
    return m_database->getAllHouseData();
}

/**
 * 格式化数据为AI可处理的格式
 *
 * 将HouseData列表转换为JSON格式
 */
QString AIDataInterface::formatDataForAI(const QList<HouseData>& houseDataList)
{
    QJsonArray houseArray;

    for (const HouseData &house : houseDataList) {
        QJsonObject houseObj;
        houseObj["communityName"] = house.communityName;
        houseObj["area"] = house.area;
        houseObj["totalPrice"] = house.price;
        houseObj["unitPrice"] = house.unitPrice;
        houseObj["floor"] = house.floor;
        houseObj["orientation"] = house.orientation;
        houseObj["buildingYear"] = house.buildingYear;
        houseObj["houseType"] = house.houseType;

        houseArray.append(houseObj);
    }

    QJsonDocument doc(houseArray);
    return doc.toJson(QJsonDocument::Compact);
}

/**
 * 发送数据给AI进行分析
 */
bool AIDataInterface::sendToAIAnalysis(const QString& formattedData)
{
    // 构造AI提示词
    QString prompt = QString("基于以下房源JSON数据，生成1份简洁的中文分析报告（300-500字）："
                           "1. 核心结论：房源数量、均价、主力户型；"
                           "2. 性价比推荐：1-3套总价低/户型好的房源；"
                           "3. 购买建议：1-3条针对性建议；"
                           "格式要求：用换行分隔，无Markdown，无特殊符号，纯文本！"
                           "房源数据：%1").arg(formattedData);

    // 发送给AI
    m_aiClient->sendMessage(prompt);
    return true;
}

// ==================== 导出格式实现 ====================

QString AIDataInterface::exportAsJson(const QString& result) const
{
    QJsonObject exportObj;
    exportObj["analysisResult"] = result;
    exportObj["dataCount"] = m_lastDataCount;
    exportObj["exportTime"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QJsonDocument doc(exportObj);
    return doc.toJson(QJsonDocument::Indented);
}

QString AIDataInterface::exportAsHtml(const QString& result) const
{
    QString html = QString("<html><head><title>房源分析报告</title></head><body>")
                 + QString("<h1>AI房源分析报告</h1>")
                 + QString("<p><strong>分析时间：</strong>%1</p>").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                 + QString("<p><strong>数据条数：</strong>%1条</p>").arg(m_lastDataCount)
                 + QString("<h2>分析结果</h2><pre>%1</pre>").arg(result)
                 + QString("</body></html>");

    return html;
}

QString AIDataInterface::exportAsText(const QString& result) const
{
    QString text = QString("=== AI房源分析报告 ===\n")
                 + QString("分析时间：%1\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                 + QString("数据条数：%1条\n").arg(m_lastDataCount)
                 + QString("\n分析结果：\n%1").arg(result);

    return text;
}

bool AIDataInterface::saveToFile(const QString& content, const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    file.close();

    return true;
}

// ==================== 信号处理 ====================

void AIDataInterface::onAIResponseReceived(const QString& result)
{
    m_latestAnalysisResult = result;
    emit analysisProgress(100, "分析完成！");
    emit analysisCompleted(result, m_lastDataCount);
}

void AIDataInterface::onAIError(const QString& error)
{
    emit analysisError(QString("AI分析错误：%1").arg(error));
}
