#ifndef AIDIALOGUECLIENT_H
#define AIDIALOGUECLIENT_H

#include <QObject>
#include <QString>
#include <QList>
#include "../HouseData.h"
#include "../MYSQL.h"
#include "DeepSeekClient.h"

class AIDialogueClient : public QObject
{
    Q_OBJECT

public:
    explicit AIDialogueClient(QObject *parent = nullptr);
    ~AIDialogueClient();

    // 设置数据库连接
    void setDatabase(Mysql* db);

    // 处理用户查询
    void processUserQuery(const QString& userQuery);

signals:
    // 对话完成信号
    void dialogueCompleted(const QString& response);

    // 错误信号
    void dialogueError(const QString& error);

private slots:
    void onAiResponseReceived(const QString& response);
    void onAiError(const QString& error);

private:
    Mysql* m_database;
    DeepSeekClient* m_aiClient;

    // 解析用户查询，返回查询类型和参数
    QPair<QString, QVariantMap> parseQuery(const QString& query);

    // 执行数据库查询
    QList<HouseData> executeDatabaseQuery(const QString& queryType, const QVariantMap& params);

    // 生成AI提示词
    QString generateAiPrompt(const QString& userQuery, const QList<HouseData>& results);

    // 格式化房源数据为字符串
    QString formatHouseData(const QList<HouseData>& data);
};

#endif // AIDIALOGUECLIENT_H
