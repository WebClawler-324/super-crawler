#ifndef AIDATAINTERFACE_H
#define AIDATAINTERFACE_H

#include <QObject>
#include <QString>
#include <QList>
#include "HouseData.h"
#include "MYSQL.h"
#include "DeepSeekClient.h"

/**
 * @brief AI数据接口类
 *
 * 该类提供两个主要接口：
 * 1. 数据传入接口：从数据库获取房源数据，格式化后传递给AI分析
 * 2. 数据传出接口：接收AI分析结果并处理
 *
 * 设计理念：将数据流和AI分析逻辑分离，提供标准化的接口供队友使用
 */
class AIDataInterface : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit AIDataInterface(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~AIDataInterface();

    // ==================== 数据传入接口 ====================

    /**
     * @brief 数据传入接口 - 从数据库获取并传递给AI
     *
     * 这是主要的外部接口函数，队友需要调用此函数来启动数据分析流程
     *
     * 工作流程：
     * 1. 调用 fetchHouseDataFromDatabase() 从数据库获取数据
     * 2. 调用 formatDataForAI() 格式化数据
     * 3. 调用 sendToAIAnalysis() 发送给AI分析
     * 4. 发出 analysisCompleted() 信号返回结果
     *
     * @param queryCondition 查询条件（如城市、价格范围等），可为空表示获取全部数据
     * @return bool 操作是否成功启动
     *
     * @note
     * - 此函数是异步的，结果通过 analysisCompleted() 信号返回
     * - 如果返回false，表示初始化失败（如数据库连接失败、API密钥未设置等）
     *
     * @example
     * // 获取全部房源数据进行分析
     * interface->importDataAndAnalyze();
     *
     * // 获取特定城市的房源数据
     * interface->importDataAndAnalyze("city=北京");
     */
    bool importDataAndAnalyze(const QString& queryCondition = QString());

    /**
     * @brief 直接传入房源数据进行AI分析
     *
     * 如果队友已经有房源数据，不需要从数据库获取，可以直接传入
     *
     * @param houseDataList 房源数据列表
     * @return bool 操作是否成功启动
     */
    bool analyzeHouseData(const QList<HouseData>& houseDataList);

    // ==================== 数据传出接口 ====================

    /**
     * @brief 数据传出接口 - 导出AI分析结果
     *
     * 将AI分析结果导出到指定格式
     *
     * @param format 导出格式："json", "text", "html"
     * @param filePath 导出文件路径，如果为空则返回字符串
     * @return QString 导出的数据内容，如果filePath不为空则返回文件路径
     */
    QString exportAnalysisResult(const QString& format = "text", const QString& filePath = QString());

    /**
     * @brief 获取最新的分析结果
     * @return QString 最新的AI分析结果
     */
    QString getLatestAnalysisResult() const;

    // ==================== 配置接口 ====================

    /**
     * @brief 设置数据库连接
     *
     * 队友需要先调用此函数设置数据库连接信息
     *
     * @param db MySQL数据库对象指针
     */
    void setDatabaseConnection(Mysql* db);

    /**
     * @brief 设置AI客户端
     *
     * 设置DeepSeek AI客户端（已配置API密钥）
     *
     * @param aiClient DeepSeek客户端对象指针
     */
    void setAIClient(DeepSeekClient* aiClient);

signals:
    /**
     * @brief 分析完成信号
     *
     * 当AI分析完成后发出此信号
     *
     * @param result AI分析结果文本
     * @param dataCount 分析的数据条数
     */
    void analysisCompleted(const QString& result, int dataCount);

    /**
     * @brief 分析进度信号
     *
     * @param progress 进度百分比 (0-100)
     * @param status 当前状态描述
     */
    void analysisProgress(int progress, const QString& status);

    /**
     * @brief 错误信号
     *
     * @param error 错误描述信息
     */
    void analysisError(const QString& error);

private slots:
    /**
     * @brief 处理AI响应完成
     */
    void onAIResponseReceived(const QString& result);

    /**
     * @brief 处理AI错误
     */
    void onAIError(const QString& error);

private:
    // ==================== 私有辅助函数 ====================

    /**
     * @brief 从数据库获取房源数据
     *
     * 根据查询条件从数据库获取房源数据
     *
     * @param condition 查询条件
     * @return QList<HouseData> 房源数据列表
     */
    QList<HouseData> fetchHouseDataFromDatabase(const QString& condition = QString());

    /**
     * @brief 格式化数据为AI可处理的格式
     *
     * 将HouseData列表转换为JSON格式，供AI分析使用
     *
     * @param houseDataList 原始房源数据
     * @return QString 格式化的JSON字符串
     */
    QString formatDataForAI(const QList<HouseData>& houseDataList);

    /**
     * @brief 发送数据给AI进行分析
     *
     * 调用DeepSeekClient进行AI分析
     *
     * @param formattedData 格式化的数据字符串
     * @return bool 发送是否成功
     */
    bool sendToAIAnalysis(const QString& formattedData);

    /**
     * @brief 导出为JSON格式
     */
    QString exportAsJson(const QString& result) const;

    /**
     * @brief 导出为HTML格式
     */
    QString exportAsHtml(const QString& result) const;

    /**
     * @brief 导出为纯文本格式
     */
    QString exportAsText(const QString& result) const;

    /**
     * @brief 保存到文件
     */
    bool saveToFile(const QString& content, const QString& filePath) const;

private:
    Mysql* m_database;                    // 数据库连接
    DeepSeekClient* m_aiClient;          // AI客户端
    QString m_latestAnalysisResult;      // 最新分析结果
    int m_lastDataCount;                 // 最后分析的数据条数
};

#endif // AIDATAINTERFACE_H
