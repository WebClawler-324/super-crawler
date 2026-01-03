# AI数据接口使用指南

## 概述

`AIDataInterface` 类提供了两个主要接口，用于处理房源数据的AI分析流程：

1. **数据传入接口**：从数据库获取数据并发送给AI分析
2. **数据传出接口**：接收AI分析结果并导出

## 类结构

```cpp
class AIDataInterface : public QObject
{
    Q_OBJECT

public:
    // 数据传入接口
    bool importDataAndAnalyze(const QString& queryCondition = QString());
    bool analyzeHouseData(const QList<HouseData>& houseDataList);

    // 数据传出接口
    QString exportAnalysisResult(const QString& format = "text", const QString& filePath = QString());
    QString getLatestAnalysisResult() const;

    // 配置接口
    void setDatabaseConnection(Mysql* db);
    void setAIClient(DeepSeekClient* aiClient);

signals:
    void analysisCompleted(const QString& result, int dataCount);
    void analysisProgress(int progress, const QString& status);
    void analysisError(const QString& error);
};
```

## 使用步骤

### 1. 初始化接口对象

```cpp
#include "AIDataInterface.h"
#include "MYSQL.h"
#include "DeepSeekClient.h"

// 创建接口对象
AIDataInterface* dataInterface = new AIDataInterface(this);

// 设置数据库连接
Mysql* database = new Mysql();
database->connectDatabase();
dataInterface->setDatabaseConnection(database);

// 设置AI客户端
DeepSeekClient* aiClient = new DeepSeekClient(this);
// 确保已设置环境变量 Deepseek_test1
dataInterface->setAIClient(aiClient);

// 连接信号
connect(dataInterface, &AIDataInterface::analysisCompleted,
        this, &YourClass::onAnalysisCompleted);
connect(dataInterface, &AIDataInterface::analysisProgress,
        this, &YourClass::onAnalysisProgress);
connect(dataInterface, &AIDataInterface::analysisError,
        this, &YourClass::onAnalysisError);
```

### 2. 使用数据传入接口

#### 方法1：从数据库自动获取数据

```cpp
// 获取全部房源数据进行分析
bool success = dataInterface->importDataAndAnalyze();

// 或者指定查询条件（预留接口，当前版本未实现复杂查询）
bool success = dataInterface->importDataAndAnalyze("city=北京");
```

#### 方法2：直接传入房源数据

```cpp
// 如果你已经有房源数据列表
QList<HouseData> myHouseData = getHouseDataFromSomewhere();
bool success = dataInterface->analyzeHouseData(myHouseData);
```

### 3. 处理分析结果

```cpp
void YourClass::onAnalysisCompleted(const QString& result, int dataCount)
{
    // result: AI分析结果文本
    // dataCount: 分析的数据条数

    qDebug() << "分析完成！处理了" << dataCount << "条数据";
    qDebug() << "分析结果：" << result;

    // 保存结果
    ui->resultTextEdit->setPlainText(result);
}

void YourClass::onAnalysisProgress(int progress, const QString& status)
{
    // progress: 进度百分比 0-100
    // status: 当前状态描述

    ui->progressBar->setValue(progress);
    ui->statusLabel->setText(status);
}

void YourClass::onAnalysisError(const QString& error)
{
    // 处理错误
    QMessageBox::warning(this, "分析错误", error);
}
```

### 4. 使用数据传出接口

#### 获取最新结果

```cpp
QString latestResult = dataInterface->getLatestAnalysisResult();
```

#### 导出结果

```cpp
// 导出为纯文本
QString textResult = dataInterface->exportAnalysisResult("text");

// 导出为JSON格式
QString jsonResult = dataInterface->exportAnalysisResult("json");

// 导出为HTML格式
QString htmlResult = dataInterface->exportAnalysisResult("html");

// 保存到文件
QString filePath = "D:/analysis_result.txt";
QString saveResult = dataInterface->exportAnalysisResult("text", filePath);
// saveResult 将返回成功消息或错误信息
```

## 信号说明

- `analysisCompleted(const QString& result, int dataCount)`: 分析完成
- `analysisProgress(int progress, const QString& status)`: 分析进度更新
- `analysisError(const QString& error)`: 分析过程中发生错误

## 数据流程

```
数据库 → AIDataInterface → 格式化 → AI分析 → 结果 → 导出/显示
```

## 注意事项

1. **初始化顺序**：必须先调用 `setDatabaseConnection()` 和 `setAIClient()` 设置必要的组件
2. **异步操作**：数据传入接口是异步的，通过信号返回结果
3. **错误处理**：通过 `analysisError` 信号报告错误，务必连接此信号
4. **API密钥**：确保已设置环境变量 `Deepseek_test1`
5. **内存管理**：接口对象由调用者管理，需要手动删除

## 扩展接口

队友可以在以下方法中添加更多功能：

- `fetchHouseDataFromDatabase()`: 添加复杂的查询条件支持
- `formatDataForAI()`: 支持不同的数据格式
- `exportAsJson()`/`exportAsHtml()`: 添加更多导出格式
- 导出功能：添加数据库保存、网络传输等功能
