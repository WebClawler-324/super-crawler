#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QUrl>
#include <QTimer>
#include <QWebEngineSettings>
#include <QWebEngineHttpRequest>
#include <QMessageBox>
#include <QJsonObject>
#include <QUrlQuery>
#include <QStringConverter>
#include <QStringEncoder>  // 明确包含编码器头文件（Qt6.9.3 必需）
#include "CustomInfoDialog.h"
#include <QVBoxLayout>
#include "AIh/DeepSeekClient.h"
#include "C:/Users/21495/QTProgram/WebCrawler/GreaterModel/house_intent_model.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QWebEnginePage *crawlWebPage = createWebEnginePage(); // 给 Crawl 用
    QWebEnginePage *aliCrawlWebPage = createWebEnginePage(); // 给 AliCrawl 用

    // 1. 初始化 Crawl 实例（传入 MainWindow 指针、Web 页面、UI 指针）
    m_crawl = new Crawl(
        this,                  // MainWindow 作为 Crawl 的父对象（生命周期绑定）
        crawlWebPage, // 新建 Web 页面，父对象设为 MainWindow
        ui                     // 传入 UI 指针（Crawl 用于信号传递，不直接操作）
        );
    a_crawl = new AliCrawl(
        this,
        aliCrawlWebPage,
        ui
        );

    //初始化数据库类对象
    mysql=new Mysql();
    //连接数据库
    mysql->connectDatabase();


    // 2. 连接 Crawl 的日志信号 → MainWindow 的 UI 更新槽函数
    // 作用：Crawl 中 emit appendLogSignal(日志) 时，自动更新 textEdit
    connect(m_crawl, &Crawl::appendLogSignal, this, &MainWindow::updateLog, Qt::QueuedConnection);
    connect(a_crawl, &AliCrawl::appendLogSignal, this, &MainWindow::AliLog, Qt::QueuedConnection);

    setImage("Cover", ui->Image1);
    //初始化查找页数
    ui->pageSpin->setValue(1);
    //初始化地图
    displayMap();

}


MainWindow::~MainWindow()
{

    // ========== 2. 释放爬虫实例（WebPage 是爬虫的子对象，自动销毁） ==========
    delete m_crawl;
    delete a_crawl;
    delete ui;
    mysql->close();
}
// 槽函数接收 Crawl 的日志
void MainWindow::updateLog(const QString& log)
{
    ui->textEdit->append(log);
}

void MainWindow::AliLog(const QString& log){
    ui->textEdit2->append(log);
}

void MainWindow::on_searchCompareBtn_clicked()
{
    QString currentCity = ui->keywordEdit->text().trimmed();
    int targetPageCount = ui->pageSpin->value();

    if (currentCity.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入城市名！（如：北京、上海、广州）");
        return;
    }

   // ui->textEdit->clear();

   // m_crawl->startHouseCrawl(currentCity, targetPageCount);
   // a_crawl->startHouseCrawl(currentCity, targetPageCount);
    // ========== 线程安全触发：发射信号，两个实例并行执行 ==========
    emit m_crawl->startCrawlSignal(currentCity, targetPageCount); // 触发 Crawl
    emit a_crawl->startCrawlSignal(currentCity, targetPageCount); // 触发 AliCrawl

    ui->textEdit->append("⏳ 正在启动爬取任务...");
}


void MainWindow:: setImage(QString name,QLabel *imagelabel){
    QString path="C:/Users/21495/QTProgram/WebCrawler/"+name+".png";
    QPixmap pixmap(path);
    if (!pixmap.isNull()) {
        // 缩放到 QLabel 大小，保持比例
        pixmap = pixmap.scaled(imagelabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        imagelabel->setPixmap(pixmap);
    }
}

QWebEnginePage* MainWindow::createWebEnginePage() {
    QWebEnginePage *page = new QWebEnginePage();
    QWebEngineSettings *settings = page->settings();

    // 通用设置（适配两个爬虫的需求）
    settings->setAttribute(QWebEngineSettings::AutoLoadIconsForPage, false);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);

    return page;
}



void MainWindow::ModelSuggest(){

    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString cfgPath = QDir(exeDir).filePath("house_intent_config.txt");
    const QString weightPath = QDir(exeDir).filePath("house_intent_weights.bin");

    // 1) 确保配置文件存在：不存在则生成模板
    if (!QFile::exists(cfgPath)) {
        QString err;
        if (!HouseIntentModel::saveConfigTemplate(cfgPath, &err)) {
            qDebug() << "Failed to write config template:" << err;
        }
        qDebug() << "Config template created:" << cfgPath;
        qDebug() << "你可以编辑该文件后重新运行。";
    }

    // 2) 用配置文件构造模型
    QStringList logs;
    QString err;
    HouseIntentModel model(cfgPath, &logs, &err);
    if (!err.isEmpty()) qDebug() << "[Config load warning]" << err;

    qDebug() << "=== Config Load Logs ===";
    for (const auto& l : logs) qDebug().noquote() << l;

    // =========================================================
    // 3) 训练数据：10 套“问卷用虚拟房源”（格式同你原来的 trainSamples）
    // 字段顺序固定：
    // {"总价","户型","单价","楼层（含总层数）","朝向","建造年份"}
    // =========================================================
    QVector<QVector<QString>> trainSamples = {
    {"260万",  "2室2厅", "2000元/㎡",  "中楼层（共9层）", "北",     "2010年"},
    {"320万", "3室1厅", "15000元/㎡", "中楼层（共26层）", "南",    "2018年"},
    {"420万", "3室2厅", "17000元/㎡", "中楼层（共45层）", "南北",   "2021年"},
    {"560万", "4室2厅", "28000元/㎡", "中楼层（共60层）", "西南",   "2022年"},
    {"110万", "2室2厅", "9000元/㎡",  "低楼层（共11层）", "西",     "1998年"},
    };

    // 训练标签：默认 0.5（你后续把每个值替换成用户填写的意愿值 0~1）
    QVector<float> trainLabels;
    trainLabels.append(ui->comboBox->currentText().toFloat());
    trainLabels.append(ui->comboBox_2->currentText().toFloat());
    trainLabels.append(ui->comboBox_3->currentText().toFloat());
    trainLabels.append(ui->comboBox_4->currentText().toFloat());
    trainLabels.append(ui->comboBox_5->currentText().toFloat());
    // TODO: 例如用户填完后改成：
    // QVector<float> trainLabels = {0.10f, 0.35f, 0.80f, ...};

    // ======= 训练开始点 =======
    if (!model.trainMode(trainSamples, trainLabels, weightPath, &err)) {
        qDebug() << "Train failed:" << err;
    }
    // ======= 训练结束点（并保存权重） =======
    qDebug() << "Train OK. Weight saved to:" << weightPath;

    // =========================================================
    // 4) 测试/预测数据：10 套“正常但不同于训练集”的房源（用于看输出是否合理）
    // =========================================================
    QVector<QVector<QString>> testSamples =mysql->getInfo();

    logs.clear();

    // ======= 测试/预测开始点 =======
    auto pairs = model.testModeIndexedAutoNormalize(testSamples, weightPath, &logs, &err);
    // ======= 测试/预测结束点（返回 pairs + logs） =======

    if (!err.isEmpty()) qDebug() << "Test warning:" << err;

    qDebug() << "\n=== Pairs (index, pred) ===";

    if (trainSamples.isEmpty()) {
        ui->textEdit2_2->append("⚠️  暂无房源数据，无法展示适配结果！");
        return;
    }
    if (pairs.isEmpty()) {
        ui->textEdit2_2->append("⚠️  暂无适配结果！");
        return;
    }
    std::sort(pairs.begin(),pairs.end(),[](const QPair<int,float>& a,const QPair<int,float>& b){
        return a.second>b.second;
    });
    int count=0;
    for(auto& p:pairs){
        if(count==5) break;
        int houseIndex = p.first;
        if (houseIndex < 0 || houseIndex >= testSamples.size()) {
            ui->textEdit2_2->append(QString("❌ 第%1条适配结果无效（房源索引越界）").arg(count + 1));
            count++;
            continue;
        }

        // 取出房源数据
        const QVector<QString>& house = testSamples[houseIndex];
        float matchRate = p.second * 100;
         ui->textEdit2_2->append(QString("\n第%1条房源(适配度：%2%)：").arg(count+=1).arg(matchRate));
         ui->textEdit2_2->append(QString("小区名：%1").arg(house[0]));
         ui->textEdit2_2->append(QString("总价：%1万").arg(house[1]));
         ui->textEdit2_2->append(QString("单价：%1元").arg(house[2]));
         ui->textEdit2_2->append(QString("户型：%1").arg(house[3]));
         ui->textEdit2_2->append(QString("面积：%1㎡").arg(house[4]));
         ui->textEdit2_2->append(QString("楼层：%1").arg(house[5]));
         ui->textEdit2_2->append(QString("朝向：%1").arg(house[6]));
         ui->textEdit2_2->append(QString("建成年份：%1").arg(house[7]));
         ui->textEdit2_2->append(QString("房源链接：%1").arg(house[8]));
    }

    qDebug() << "\n=== Logs (normalize/errors) ===";
    // 归一化细节、解析失败、缺省处理等日志在 logs
    for (const auto& l : logs) qDebug().noquote() << l;
}

void MainWindow::generateBin(){
    QPieSeries *pieSeries = new QPieSeries();
    // 添加数据（标签+数值）
    double Two=0;
    double Four=0;
    double UFour=0;
    mysql->getPriceCout(Two,Four,UFour);
    qDebug()<<"200万以下占比:"<<Two;
    pieSeries->append("200万以下", Two);
    pieSeries->append("200-400万", Four);
    pieSeries->append("400万以上", UFour);
    foreach (QPieSlice *slice, pieSeries->slices()) {
        // 自定义标签：“类别名称: 数值%”（在数值后添加%，实现百分号显示）
        QString labelText = QString("%1: %2%").arg(slice->label()).arg(slice->value(), 0, 'f', 0);
        slice->setLabel(labelText); // 设置切片标签为“类别+数值+百分号”
        slice->setLabelVisible(true); // 确保标签可见
        slice->setLabelColor(QColor("#333333")); // 标签文字颜色（避免和切片颜色重叠）
        //设置标签在切片外部显示，避免遮挡
        slice->setLabelPosition(QPieSlice::LabelOutside);
    }
    // 1.2 设置饼图样式（匹配截图）
    pieSeries->setLabelsVisible(true); // 显示标签（可选）
    // 单独设置某一块的颜色
    pieSeries->slices().at(2)->setColor(QColor("#e53935"));
    pieSeries->slices().at(0)->setColor(QColor("#43a047"));

    // 1.3 创建饼图对象，绑定数据系列
    pieChart = new QChart();
    pieChart->addSeries(pieSeries);
    pieChart->setTitle("当前页房价分布"); // 图表标题
    pieChart->setMargins(QMargins(10, 0, 10, 10));
    pieChart->setAnimationOptions(QChart::SeriesAnimations); // 动画效果（可选）
    pieChart->legend()->setAlignment(Qt::AlignBottom); // 图例放在底部

    // 1.4 将饼图绑定到提升后的QChartView容器
    ui->widgetPieChart->setChart(pieChart);
    ui->widgetPieChart->setRenderHint(QPainter::Antialiasing); // 抗锯齿，更清晰

}

void MainWindow::generateZhu(){

    // 2 创建柱状图数据系列
    QBarSeries *barSeries = new QBarSeries();
    QBarSet *barSet = new QBarSet("小于100㎡");
    QBarSet *barSet2 = new QBarSet("100-200㎡");
    QBarSet *barSet3 = new QBarSet("大于200㎡");
    // 添加数据（完全保留原有O、Tw、Th变量，不做任何修改）
    double O=0,Tw=0,Th=0,total=0;
    mysql->getAreaCout(O,Tw,Th,total);

    *barSet << O;
    barSeries->append(barSet);

    *barSet2 << Tw;
    barSeries->append(barSet2);

    *barSet3 << Th;
    barSeries->append(barSet3);

    // 2.2 设置柱状图样式（保留原有颜色，删除无效的setVisible调用）
    barSet->setColor(QColor("#1e88e5")); // 蓝色：小于100㎡
    barSet2->setColor(QColor("#e53935")); // 红色：100-200㎡
    barSet3->setColor(QColor("#43a047")); // 绿色：200-300㎡

    // 2.3 创建坐标轴（删除无效的setLabelsAlignment调用，保留你的核心设置）
    QCategoryAxis *axisX = new QCategoryAxis();
    axisX->append("房源面积统计", 0); // 保留你需要的X轴标签
    axisX->setLabelsAngle(0); // 保留标签不旋转（无需对齐设置，默认居中效果可满足）

    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, total); // 完全保留你原有Y轴范围（0到20）
    axisY->setLabelFormat("%d"); // Y轴标签显示为整数，美观不报错
    axisY->setTitleText("房源数量"); // 补充Y轴标题，不影响编译

    // 2.4 创建柱状图对象，绑定数据+坐标轴（保留所有有效优化）
    barChart = new QChart();
    barChart->addSeries(barSeries);
    barChart->setTitle("房源面积分布");
    barChart->setAnimationOptions(QChart::SeriesAnimations); // 保留动画效果
    // 绑定坐标轴（原有逻辑不变，确保正常显示）
    barChart->setAxisX(axisX, barSeries);
    barChart->setAxisY(axisY, barSeries);
    // 优化图例显示（所有调用均有效，无编译错误）
    barChart->legend()->setAlignment(Qt::AlignBottom); // 图例底部显示
    barChart->legend()->setVisible(true); // 强制显示图例，即使数值为0
    barChart->legend()->setLabelColor(QColor("#333333")); // 图例文字颜色清晰

    // 优化柱状图间距，避免三个barSet重叠（有效调用，无错误）
    barSeries->setBarWidth(0.8); // 设置柱状图宽度（0-1之间，0.8更美观）

    // 2.5 将柱状图绑定到提升后的QChartView容器（原有逻辑不变，抗锯齿有效）
    ui->widgetBarChart->setChart(barChart);
    ui->widgetBarChart->setRenderHint(QPainter::Antialiasing); // 抗锯齿，显示更清晰
    // 容器自适应大小（有效调用，无编译错误）
    ui->widgetBarChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void MainWindow:: generateTabel(){
    // 2. 关闭自动拉伸（关键！避免列宽被QT自动覆盖）
    QHeaderView *horizontalHeader = ui->tableWidget->horizontalHeader();
    // 锁定列宽
    horizontalHeader->setSectionResizeMode(QHeaderView::Fixed);

    // 3. 设置7列宽度（索引0~6对应第1~7列，像素值可按需修改）
    ui->tableWidget->setColumnWidth(0, 200);
    ui->tableWidget->setColumnWidth(1, 200);
    ui->tableWidget->setColumnWidth(2, 200);
    ui->tableWidget->setColumnWidth(3, 200);
    ui->tableWidget->setColumnWidth(4, 200);
    ui->tableWidget->setColumnWidth(5, 200);
    ui->tableWidget->setColumnWidth(6, 200);
    ui->tableWidget->setColumnWidth(7, 200); // 第8列
    //填充列
    mysql->generateTable(ui->tableWidget,tableOriginalData);

}
void MainWindow::displayMap(){
    QWidget *mapPage = ui->stackedWidget->findChild<QWidget*>("MapPage");
    if (mapPage == nullptr)
    {
        qDebug() << "未找到栈页面中的MapPage！";
        return;
    }
    QWidget *mapWidget = mapPage->findChild<QWidget*>("MapWidget");
    if (mapWidget == nullptr)
    {
        qDebug() << "未在MapPage中找到MapWidget！";
        return;
    }
    // 步骤3：给MapWidget创建并设置垂直布局（核心：布局挂载到MapWidget上）
    QVBoxLayout *mainLayout = new QVBoxLayout(mapWidget); // 布局父对象设为mapWidget，自动绑定
    mainLayout->setContentsMargins(20, 20, 20, 20); // 内边距
    mainLayout->setSpacing(10); // 控件间距

    // 步骤4：创建自定义Profile和Page（你的原有逻辑，不变）
    customProfile = new QWebEngineProfile(this);
    customProfile->setHttpCacheType(QWebEngineProfile::NoCache);
    customProfile->clearHttpCache();

    customPage = new QWebEnginePage(customProfile, this);
    QWebEngineSettings *settings = customPage->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    settings->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);

    // 步骤5：创建WebView并绑定自定义Page，添加到MapWidget的布局中
    webView = new QWebEngineView(this);
    webView->setPage(customPage); // 替换默认Page
    mainLayout->addWidget(webView, 1); // 1表示拉伸因子，让WebView填满MapWidget

    // ========== 加载页面 ==========
    connect(customPage, &QWebEnginePage::loadFinished, this, [this](bool success) {
        mapPageLoaded = success;
        qDebug() << "页面加载完成，状态：" << (success ? "成功" : "失败");
    });

    QString htmlPath = QCoreApplication::applicationDirPath() + "/web/map.html";
    qDebug() << "HTML加载路径：" << htmlPath;
    webView->load(QUrl::fromLocalFile(htmlPath));

}
// 辅助工具函数：提取字符串中的纯数字（使用 QRegularExpression，兼容 Qt5/Qt6）
double extractPureNumber(const QString &text)
{
    // 正则表达式：匹配整数或小数（保留正数，满足房价/面积场景）
    QRegularExpression regExp("(\\d+(\\.\\d+)?)");
    QRegularExpressionMatchIterator it = regExp.globalMatch(text); // 全局匹配所有数字片段
    QString numStr;

    // 提取所有数字片段并拼接
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        numStr += match.captured(1); // 获取第一个捕获组的内容（纯数字）
    }

    // 转换为数字，失败则返回0
    bool ok;
    double num = numStr.toDouble(&ok);
    return ok ? num : 0.0;
}

void MainWindow::filterTableData()
{
    // 1. 获取筛选关键词并拆分多条件（按逗号分割，支持综合过滤）
    QString originalFilterText = ui->lineEdit_filter->text().trimmed();
    QTableWidget *tableWidget = ui->tableWidget;
    QString tempFilterText = originalFilterText;
    tempFilterText = tempFilterText.replace("，", ","); // 替换中文逗号为英文逗号
    // 拆分多条件：按逗号分割 + 清理每个条件的前后空格 + 过滤空条件
    QStringList filterConditions = tempFilterText.split(',', Qt::SkipEmptyParts);
    for (int i = 0; i < filterConditions.size(); i++)
    {
        filterConditions[i] = filterConditions[i].trimmed(); // 清理每个条件的前后空格
    }
    filterConditions.removeAll(""); // 移除空条件（避免无效匹配）

    // 2. 清空表格原有数据
    tableWidget->clearContents();
    tableWidget->setRowCount(0);

    // 3. 无筛选条件时，填充所有原始数据
    if (filterConditions.isEmpty())
    {
        fillTableWithData(tableOriginalData);
        return;
    }

    // 4. 遍历原始数据，筛选同时满足所有条件的数据
    QList<QStringList> matchedData;
for (const QStringList &rowData : tableOriginalData)
    {
        bool allConditionsMatched = true; // 标记：是否满足所有筛选条件

        // 遍历每个拆分后的筛选条件，必须全部满足才有效
    for (const QString &singleCondition : filterConditions)
        {
            bool conditionMatched = false; // 标记：当前行是否满足该单个条件
            QString lowerSingleCondition = singleCondition.toLower();
            double userInputNum = extractPureNumber(singleCondition); // 提取当前条件的纯数字
            bool hasUserNum = (userInputNum != 0.0);
            int targetColumn = -1;

            // ========== 步骤1：判断当前单个条件对应的目标列（总价/单价/面积） ==========
            if (lowerSingleCondition.contains("万"))
                targetColumn = 2;  // 总价列
            else if (lowerSingleCondition.contains("元"))
                targetColumn = 3; // 单价列
            else if (lowerSingleCondition.contains("平米"))
                targetColumn = 4; // 面积列

            // ========== 步骤2：匹配总价/单价/面积列（数值范围匹配，绝对值差≤50） ==========
            if (targetColumn != -1 && hasUserNum)
            {
                if (rowData.size() > targetColumn)
                {
                    QString cellText = rowData.at(targetColumn).trimmed();
                    double cellNum = extractPureNumber(cellText);
                    if (cellNum != 0.0)
                    {
                        double diff = qAbs(userInputNum - cellNum);
                        // 数值差值满足条件，标记该单个条件匹配成功
                        if (diff <= 50.0)
                        {
                            conditionMatched = true;
                            qDebug() << "数值条件匹配成功：" << singleCondition
                                     << "，表格数据" << cellNum << "，差值" << diff;
                        }
                    }
                }
            }
            // ========== 步骤3：无特殊单位，全列精准匹配（兼容户型等字符串，排除无效户型） ==========
            else
            {
                for (int col = 0; col < rowData.size(); col++)
                {
                    QString cellText = rowData.at(col).trimmed().toLower();
                    QString cleanCellText = cellText.remove(QChar(12288)); // 去除全角空格
                    bool colIsMatch = false;

                    // 对总价/单价/面积列，尝试无单位数值匹配（差值≤20）
                    if (hasUserNum && (col == 2 || col == 3 || col == 4))
                    {
                        double cellNum = extractPureNumber(cleanCellText);
                        if (cellNum != 0.0)
                        {
                            double diff = qAbs(userInputNum - cellNum);
                            if (diff <= 20.0)
                            {
                                colIsMatch = true;
                            }
                        }
                    }
                    // 其他列：字符串匹配（区分户型查询和普通文本查询，优化匹配逻辑）
                    if (!colIsMatch)
                    {
                        // 过滤无效户型（仅对户型查询生效）
                        QStringList invalidHouseTypes = {"未知", "", "无", "未标注"};
                        // 定义户型关键字正则，判断当前查询是否为户型查询
                        QRegularExpression houseTypeRegex("室|厅|卫", QRegularExpression::CaseInsensitiveOption);
                        bool isHouseTypeQuery = lowerSingleCondition.contains(houseTypeRegex);

                        // 仅当是户型查询时，才过滤无效户型
                        if (isHouseTypeQuery && invalidHouseTypes.contains(cleanCellText, Qt::CaseInsensitive))
                        {
                            continue;
                        }

                        // 精准匹配：区分户型查询（需关键字校验）和普通文本查询（纯包含匹配）
                        QString cleanSingleCondition = lowerSingleCondition.remove(QChar(12288));
                        if (isHouseTypeQuery)
                        {
                            // 户型查询：保留原有逻辑，必须包含户型关键字
                            if (cleanCellText.contains(cleanSingleCondition) &&
                                cleanCellText.contains(houseTypeRegex))
                            {
                                colIsMatch = true;
                            }
                        }
                        else
                        {
                            // 普通文本查询（如小区名称：富力城）：纯大小写不敏感包含匹配，无需户型关键字
                            if (cleanCellText.contains(cleanSingleCondition, Qt::CaseInsensitive))
                            {
                                colIsMatch = true;
                            }
                        }

                    }

                    if (colIsMatch)
                    {
                        conditionMatched = true; // 该单个条件匹配成功
                        break; // 跳出列循环，继续下一个筛选条件
                    }
                }
            }

            // ========== 关键：只要有一个条件不满足，当前行直接作废 ==========
            if (!conditionMatched)
            {
                allConditionsMatched = false;
                break; // 跳出条件循环，继续下一行数据
            }
        }

        // 所有筛选条件都满足，才加入匹配数据
        if (allConditionsMatched)
        {
            matchedData.append(rowData);
        }
    }

    // 填充匹配到的数据
    fillTableWithData(matchedData);
}

void MainWindow::fillTableWithData(const QList<QStringList> &data)
{

    if(data.empty()){
       QMessageBox::information(this,"筛选失败","无匹配信息");
    }else{
        QTableWidget *tableWidget = ui->tableWidget;
        tableWidget->setRowCount(data.size()); // 设置表格行数
        int currentRow = 0;
        for (const QStringList &rowData : data)
        {
            // 填充每一列数据
            for (int col = 0; col < rowData.size() && col < tableWidget->columnCount(); col++)
            {
                tableWidget->setItem(currentRow, col, new QTableWidgetItem(rowData.at(col)));
            }
            currentRow++;
        }
        QString count=QString::number( currentRow);
        QMessageBox::information(this,"筛选成功","共有"+count+"行匹配或近似数据");
    }

}

//到首页
void MainWindow::on_pushButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}
//到推荐页
void MainWindow::on_searchCompareBtn_2_clicked()
{
    ui->stackedWidget->setCurrentIndex(2);
}


//显示推荐结果
void MainWindow::on_pushButton_2_clicked()
{
    ui->textEdit2_2->clear();
    ModelSuggest();
}
//从推荐页回到首页
void MainWindow::on_pushButton_3_clicked()
{
     ui->stackedWidget->setCurrentIndex(1);
}


//数首页到数据页按钮
void MainWindow::on_searchCompareBtn_3_clicked()
{
    ui->stackedWidget->setCurrentIndex(3);
    //从数据库统计信息生成饼图
    generateBin();
    generateZhu();
    generateTabel();
}

//从信息页回到首页
void MainWindow::on_pushButton_4_clicked()
{
     ui->stackedWidget->setCurrentIndex(1);
}

//查看信息
void MainWindow::on_pushButton_5_clicked()
{
    //当前选中行索引
    int selectedRow = ui->tableWidget->currentRow();
    QString showData;//用于展示

    int columnCount = ui->tableWidget->columnCount(); // 获取表格总列数
    for (int col = 0; col < columnCount; col++)
    {
        QTableWidgetItem *item = ui->tableWidget->item(selectedRow, col);
        // 处理空单元格
        QString cellText = (item != nullptr) ? item->text() : "空值";
        QString fieldName;
        switch (col)
        {
        case 0: fieldName = "房源标题："; break;
        case 1: fieldName = "小区名称："; break;
        case 2: fieldName = "价格："; break;
        case 3: fieldName = "单价："; break;
        case 4: fieldName = "面积："; break;
        case 5: fieldName = "户型："; break;
        case 6: fieldName = "楼层："; break;
        case 7: fieldName = "房源链接："; break;
        default: fieldName = "未知字段："; break;
        }
        showData += fieldName + cellText + "\n"; // 换行分隔，显示更整洁

    }

    //实例化自定义弹窗并显示数据
    CustomInfoDialog *infoDialog = new CustomInfoDialog(this); // 父窗口设为this，弹窗居中显示
    infoDialog->setInfoText("第" + QString::number(selectedRow + 1) + "行信息：\n" + showData); // 设置数据
    infoDialog->exec(); // 以模态方式显示弹窗（用户必须关闭弹窗才能操作主窗口）

    // 模态弹窗关闭后，自动释放内存
    delete infoDialog;
}

void MainWindow::findHouse(QString address){
    if (address.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入地址！");
        return;
    }

    if (!mapPageLoaded) {
        QMessageBox::information(this, "提示", "地图正在加载，请稍后再试。");
        qDebug() << "地图尚未加载完成，跳过JS调用";
        return;
    }

    QString escaped = address;
    escaped.replace("\\", "\\\\")
        .replace("\"", "\\\"")
        .replace("\n", "\\n")
        .replace("\r", "\\r");

    const QString js = QStringLiteral(
                           "if (typeof addressToPoint === 'function') { "
                           "addressToPoint(\"%1\"); "
                           "} else { console.error('addressToPoint 未定义'); }").arg(escaped);

    if (webView && webView->page()) {
        webView->page()->runJavaScript(js);
    }

    qDebug() << "Qt发送定位地址：" << address;
}

//地图找房
void MainWindow::on_FininMap_clicked()
{
    ui->stackedWidget->setCurrentIndex(4);

}

//从地图页返回首页
void MainWindow::on_Back_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}

//地图找房
void MainWindow::on_pushButton_6_clicked()
{
    QString address = ui->FindHouse->text().trimmed();
    findHouse(address);
}

//数据分析页地图找房
void MainWindow::on_pushButton_7_clicked()
{
    int selectedRow = ui->tableWidget->currentRow();

    int columnCount = ui->tableWidget->columnCount(); // 获取表格总列数

    QTableWidgetItem *item = ui->tableWidget->item(selectedRow, 1);
     // 获取到小区名
    QString commName = (item != nullptr) ? item->text() : "空值";
    ui->FindHouse->setText(commName);

    if(commName=="空值"){
        QMessageBox::warning(this,"错误","无小区名,无法定位");
    }else{
        findHouse(commName);
        //跳到地图页面
        ui->stackedWidget->setCurrentIndex(4);
    }

}

//刷新数据表
void MainWindow::on_pushButton_8_clicked()
{
     mysql->generateTable(ui->tableWidget,tableOriginalData);
}

//筛选房源
void MainWindow::on_Select_clicked()
{
    filterTableData();
}

//AI->首页
void MainWindow::on_Back_2_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}
//到AI
void MainWindow::on_FininMap_2_clicked()
{
    ui->stackedWidget->setCurrentIndex(5);
}


// AI分析按钮点击处理
void MainWindow::on_analyzeBtn_clicked()
{
    ui->AItextEdit->clear();
    ui->AItextEdit->append("正在生成AI分析报告，请稍候...\n");
    // 创建房源数据
    mysql->getToJas(houseDataArray);
    // 创建AI分析提示词
    QString prompt = "基于以下房源JSON数据，生成1份简洁的中文分析报告（300-500字）："
                     "1. 核心结论：房源数量、均价、主力户型；"
                     "2. 性价比推荐：1-3套总价低/户型好的房源；"
                     "3. 购买建议：1-3条针对性建议；"
                     "格式要求：用换行分隔，无Markdown，无特殊符号，纯文本！"
                     "房源数据：" + QJsonDocument(houseDataArray).toJson(QJsonDocument::Compact);

    // 调用DeepSeek AI进行分析
    DeepSeekClient *client = new DeepSeekClient(this);

    // 初始化API密钥
    if (!client->initialize()) {
        ui->AItextEdit->append("\n❌ AI分析失败：API密钥未配置");
        return;
    }

    connect(client, &DeepSeekClient::responseReceived, this, &MainWindow::onAnalysisCompleted);
    connect(client, &DeepSeekClient::errorOccurred, [this](const QString& error) {
        ui->AItextEdit->append("\n❌ AI分析失败：\n" + error);
    });

    client->sendMessage(prompt);
}

// 清空分析按钮点击处理
void MainWindow::on_clearAnalysisBtn_clicked()
{
    ui->AItextEdit->clear();
    ui->AItextEdit->setPlainText("AI分析报告将显示在这里...");
}

// 询问按钮点击处理
void MainWindow::on_askQuestionBtn_clicked()
{
    QString question = ui->questionInput->text().trimmed();
    if (question.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入您的问题！");
        return;
    }

    // 创建房源数据
    mysql->getToJas(houseDataArray);
    ui->AItextEdit->clear();
    QString thinkingMessages[] = {
        "🤔 正在深度思考您的问题...",
        "🔍 正在分析和整理信息...",
        "💭 正在组织最合适的回答...",
        "🤖 大模型思考中..."
    };
    int randomIndex = QRandomGenerator::global()->bounded(4);
    ui->AItextEdit->append(thinkingMessages[randomIndex] + "\n");

    // 创建智能问答的提示词
    QString prompt = QString("你是一个友好的智能助手，名叫\"房源小助手\"，可以回答各种问题。\n\n"
                             "【房源数据】（当用户询问房源相关问题时使用）：\n%1\n\n"
                             "【用户问题】：%2\n\n"
                             "【回答指南】：\n"
                             "• 房源问题：基于房源数据详细回答房价、户型、位置等信息\n"
                             "• 日常问题：自然友好地回答日期、天气、笑话、常识等\n"
                             "• 保持友好语气，可以加适当的表情符号\n"
                             "• 如果问题不明确，可以幽默地请求澄清\n"
                             "• 回答要简洁但信息丰富")
                         .arg(QJsonDocument(houseDataArray).toJson(QJsonDocument::Compact))
                         .arg(question);

    // 调用DeepSeek AI进行问答
    DeepSeekClient *client = new DeepSeekClient(this);

    // 初始化API密钥
    if (!client->initialize()) {
        ui->AItextEdit->append("\n❌ AI问答失败：API密钥未配置");
        return;
    }

    connect(client, &DeepSeekClient::responseReceived, this, &MainWindow::onQuestionAnswered);
    connect(client, &DeepSeekClient::errorOccurred, [this](const QString& error) {
        ui->AItextEdit->append("\n❌ AI问答失败：\n" + error);
    });

    client->sendMessage(prompt);
}

// AI分析完成回调
void MainWindow::onAnalysisCompleted(const QString& result)
{
    ui->AItextEdit->clear();
    ui->AItextEdit->setPlainText("📊 AI智能分析报告\n\n" + result);
}

// AI问答完成回调
void MainWindow::onQuestionAnswered(const QString& result)
{
    ui->AItextEdit->clear();
    QString question = ui->questionInput->text();
    ui->AItextEdit->setPlainText(QString("❓ 您的问题：%1\n\n🤖 AI回答：\n%2")
                                         .arg(question)
                                         .arg(result));
}


