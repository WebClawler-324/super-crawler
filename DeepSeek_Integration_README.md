# DeepSeek大模型接入集成说明

## 概述
本模块实现了DeepSeek大模型的接入功能，用于对房源数据进行AI分析和报告生成。提供了完整的数据输入输出接口，方便与其他模块集成。

## 主要文件
- `DeepSeekClient.h/cpp` - DeepSeek API客户端核心实现
- `DataInterface.h/cpp` - 数据输入输出接口
- `MYSQL.h/cpp` - 数据库操作（已修复数据转换逻辑）
- `IntegrationValidator.h/cpp` - 集成验证工具
- `DataIntegrationTest.h/cpp` - 数据集成测试
- `LLMClient.h/cpp` - 原有LLM客户端（已添加数据转换功能）

## 功能特性
1. **数据格式转换**：将QString格式的房源数据转换为JSON格式
2. **DeepSeek API集成**：使用环境变量`Deepseek_test1`中的API密钥
3. **智能分析报告**：基于用户指定的提示词生成300-500字的中文分析报告
4. **统一接口**：提供标准的数据输入输出接口
5. **数据库集成**：完整的数据库连接、插入、查询功能
6. **集成验证**：自动化测试和验证工具

## 数据字段
房源数据包含以下字段：
- `houseTitle` - 房源标题
- `communityName` - 小区名称
- `price` - 总价（万）
- `unitPrice` - 单价（元/㎡）
- `houseType` - 户型（几室几厅）
- `area` - 面积（㎡）
- `floor` - 楼层
- `orientation` - 朝向
- `buildingYear` - 建造年代
- `houseUrl` - 房源链接
- `city` - 城市

## 使用方法

### 1. 初始化和配置
```cpp
// 创建数据接口
DataInterface *dataInterface = new DataInterface();

// 设置回调函数
dataInterface->setCallbacks(
    [](const QString& report) {
        qDebug() << "分析报告：" << report;
    },
    [](const QString& error) {
        qDebug() << "错误：" << error;
    },
    []() {
        // 处理数据输入请求
        qDebug() << "需要从数据库获取数据";
    }
);

// 初始化DeepSeek客户端
if (!dataInterface->initializeDeepSeekClient()) {
    qDebug() << "DeepSeek客户端初始化失败";
    return;
}
```

### 2. 数据库操作
```cpp
// 连接数据库
Mysql mysql;
mysql.connectDatabase();

// 插入数据
HouseData house;
house.communityName = "阳光小区";
house.price = "320万";
house.area = "120㎡";
// ... 设置其他字段
mysql.insertInfo(house);

// 查询所有数据
QList<HouseData> allData = mysql.getAllHouseData();
```

### 3. 数据分析流程
```cpp
// 方法1：直接设置数据进行分析
QList<HouseData> houseDataList = mysql.getAllHouseData();
dataInterface->setHouseDataDirectly(houseDataList);
dataInterface->startAIAnalysis();

// 方法2：请求数据库数据（用于异步处理）
dataInterface->inputHouseDataFromDatabase();
// 在回调函数中处理数据获取和分析
```

### 4. 验证集成
```cpp
#include "IntegrationValidator.h"

// 生成完整验证报告
QString report = IntegrationValidator::generateValidationReport();
qDebug() << report;

// 或单独验证各组件
bool dbOk = IntegrationValidator::validateDatabaseConnection();
bool apiOk = IntegrationValidator::validateDeepSeekClient();
bool fullOk = IntegrationValidator::validateFullIntegration();
```

## 数据库配置
确保MySQL数据库配置正确：
- 数据库名：`HouseDB`
- 用户名：`root`
- 密码：`qwqwasas25205817`
- 端口：`3306`
- 主机：`localhost`

数据库表结构：
```sql
CREATE TABLE houseinfo (
    houseTitle VARCHAR(255),
    communityName VARCHAR(255),
    price DOUBLE,
    unitPrice DOUBLE,
    houseType VARCHAR(100),
    area DOUBLE,
    floor VARCHAR(100),
    orientation VARCHAR(50),
    buildingYear INT,
    houseUrl VARCHAR(500)
);
```

## AI分析报告格式
生成的分析报告包含以下内容：
1. **核心结论**：房源数量、均价、主力户型
2. **性价比推荐**：1-3套总价低/户型好的房源
3. **购买建议**：1-3条针对性建议

报告格式要求：
- 纯文本格式，无Markdown标记
- 用换行分隔，无特殊符号
- 300-500字中文内容

## 环境配置
确保系统环境变量中已配置：
```
Deepseek_test1=你的DeepSeek_API密钥
```

## 接口说明

### DataInterface类
- **初始化**：`initializeDeepSeekClient()`
- **回调设置**：`setCallbacks(onReport, onError, onDataRequest)`
- **数据输入**：`setHouseDataDirectly()`, `inputHouseDataFromDatabase()`
- **分析启动**：`startAIAnalysis()`
- **客户端访问**：`getDeepSeekClient()`

### Mysql类
- **连接**：`connectDatabase()`
- **插入**：`insertInfo(const HouseData&)`
- **查询**：`getAllHouseData()`
- **关闭**：`close()`

### DeepSeekClient类
- **数据转换**：`convertHouseDataToJson()`
- **分析请求**：`requestAnalysisReport()`
- **回调设置**：`setCallbacks()`

## 测试和验证

### 运行集成测试
```cpp
#include "DataIntegrationTest.h"

DataIntegrationTest *test = new DataIntegrationTest();
test->runAllTests();
```

### 验证报告示例
```
=== 项目集成验证报告 ===

1. 数据库连接: ✓ 通过
2. 数据格式转换: ✓ 通过
3. DeepSeek API客户端: ✓ 通过

=== 数据流验证 ===
数据库 → JSON转换 → AI分析: ✓ 完整流程正常

=== 配置检查 ===
DeepSeek API密钥: 已配置
```

## 注意事项
1. 确保MySQL服务正在运行
2. 确保网络连接正常，DeepSeek API可访问
3. API密钥需要有效且有足够的使用额度
4. 数据量过大时可能需要分批处理
5. 错误信息会通过回调函数传递

## 故障排除

### 数据库连接失败
- 检查MySQL服务是否启动
- 验证数据库名、用户名、密码是否正确
- 检查防火墙设置

### API调用失败
- 确认环境变量 `Deepseek_test1` 已设置
- 检查网络连接
- 验证API密钥有效性

### 数据转换失败
- 检查HouseData结构体字段是否完整
- 验证数据格式是否符合预期
- 查看调试日志获取详细错误信息

## 扩展性
- 如需支持其他AI模型，可以继承`DataInterface`类
- 数据字段可以根据需求扩展
- 提示词可以根据具体需求调整
- 可以添加更多验证和测试功能
