#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QUrl>
#include <QTimer>
#include <QWebEngineSettings>
#include <QWebEngineHttpRequest>
#include <QRegularExpressionMatchIterator>
#include <QMessageBox>
#include <QDateTime>
#include <QRandomGenerator>
#include <QByteArray>
#include <algorithm>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QStringConverter>
#include <QStringEncoder>  // 明确包含编码器头文件（Qt6.9.3 必需）
#include <QFile>
#include <QTextStream>
#include "DeepSeekClient.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 初始化数据库
    m_database = new Mysql();
    m_database->connectDatabase();

    crawlThread = new QThread(this);
    aliCrawlThread = new QThread(this);

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

    m_crawl->moveToThread(crawlThread);
    a_crawl->moveToThread(aliCrawlThread);

    // 2. 连接 Crawl 的日志信号 → MainWindow 的 UI 更新槽函数
    // 作用：Crawl 中 emit appendLogSignal(日志) 时，自动更新 textEdit
    connect(m_crawl, &Crawl::appendLogSignal, this, &MainWindow::updateLog, Qt::QueuedConnection);
    connect(a_crawl, &AliCrawl::appendLogSignal, this, &MainWindow::AliLog, Qt::QueuedConnection);

    setImage("Cover", ui->Image1);

    ui->pageSpin->setValue(1);

    // 连接AI分析按钮信号
    connect(ui->analyzeBtn, &QPushButton::clicked, this, &MainWindow::on_analyzeBtn_clicked);
    connect(ui->clearAnalysisBtn, &QPushButton::clicked, this, &MainWindow::on_clearAnalysisBtn_clicked);
    connect(ui->askQuestionBtn, &QPushButton::clicked, this, &MainWindow::on_askQuestionBtn_clicked);

    // ========== 6. 启动线程（线程进入事件循环，等待爬取信号） ==========
    crawlThread->start();
    aliCrawlThread->start();
}


MainWindow::~MainWindow()
{
    // 停止 Crawl 对应的线程
    crawlThread->quit();
    if (!crawlThread->wait(5000)) { // 等待 5 秒，确保任务完成
        crawlThread->terminate();   // 超时强制终止（万不得已）
        crawlThread->wait();
    }
    // 停止 AliCrawl 对应的线程
    aliCrawlThread->quit();
    if (!aliCrawlThread->wait(5000)) {
        aliCrawlThread->terminate();
        aliCrawlThread->wait();
    }

    // ========== 2. 释放爬虫实例（WebPage 是爬虫的子对象，自动销毁） ==========
    delete m_crawl;
    delete a_crawl;
    delete m_database;
    delete ui;
}
// 槽函数接收 Crawl 的日志
void MainWindow::updateLog(const QString& log)
{
    ui->textEdit->append(log);
}

void MainWindow::AliLog(const QString& log){
    ui->textEdit2->append(log);
}



void MainWindow::on_pushButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);

}


void MainWindow::on_searchCompareBtn_clicked()
{
    QString currentCity = ui->keywordEdit->text().trimmed();
    int targetPageCount = ui->pageSpin->value();

    if (currentCity.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入城市名！（如：北京、上海、广州）");
        return;
    }

    ui->textEdit->clear();

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

// AI分析按钮点击处理
void MainWindow::on_analyzeBtn_clicked()
{
    ui->aiAnalysisText->clear();
    ui->aiAnalysisText->append("正在生成AI分析报告，请稍候...\n");

    // 创建测试房源数据
    QJsonArray houseDataArray;

    // Q1
    QJsonObject q1;
    q1["总价"] = "480万";
    q1["户型"] = "4室2厅";
    q1["单价"] = "20000元/㎡";
    q1["楼层"] = "高楼层（共60层）";
    q1["朝向"] = "南";
    q1["年代"] = "2021年";
    houseDataArray.append(q1);

    // Q2
    QJsonObject q2;
    q2["总价"] = "90万";
    q2["户型"] = "1室1厅";
    q2["单价"] = "6000元/㎡";
    q2["楼层"] = "低楼层（共7层）";
    q2["朝向"] = "北";
    q2["年代"] = "1985年";
    houseDataArray.append(q2);

    // Q3
    QJsonObject q3;
    q3["总价"] = "250万";
    q3["户型"] = "2室1厅";
    q3["单价"] = "12000元/㎡";
    q3["楼层"] = "低楼层（共18层）";
    q3["朝向"] = "东南";
    q3["年代"] = "1950年";
    houseDataArray.append(q3);

    // Q4
    QJsonObject q4;
    q4["总价"] = "200万";
    q4["户型"] = "2室2厅";
    q4["单价"] = "11000元/㎡";
    q4["楼层"] = "中楼层（共24层）";
    q4["朝向"] = "东北";
    q4["年代"] = "2008年";
    houseDataArray.append(q4);

    // Q5
    QJsonObject q5;
    q5["总价"] = "250万";
    q5["户型"] = "3室1厅";
    q5["单价"] = "15000元/㎡";
    q5["楼层"] = "中楼层（共18层）";
    q5["朝向"] = "南北";
    q5["年代"] = "1998年";
    houseDataArray.append(q5);

    // Q6
    QJsonObject q6;
    q6["总价"] = "250万";
    q6["户型"] = "3室1厅";
    q6["单价"] = "15000元/㎡";
    q6["楼层"] = "中楼层（共18层）";
    q6["朝向"] = "西北";
    q6["年代"] = "1998年";
    houseDataArray.append(q6);

    // Q7
    QJsonObject q7;
    q7["总价"] = "320万";
    q7["户型"] = "2室1厅";
    q7["单价"] = "19000元/㎡";
    q7["楼层"] = "低楼层（共33层）";
    q7["朝向"] = "南";
    q7["年代"] = "2016年";
    houseDataArray.append(q7);

    // Q8
    QJsonObject q8;
    q8["总价"] = "160万";
    q8["户型"] = "2室1厅";
    q8["单价"] = "9000元/㎡";
    q8["楼层"] = "顶层（共11层）";
    q8["朝向"] = "东";
    q8["年代"] = "2005年";
    houseDataArray.append(q8);

    // Q9
    QJsonObject q9;
    q9["总价"] = "380万";
    q9["户型"] = "3室2厅";
    q9["单价"] = "18000元/㎡";
    q9["楼层"] = "高楼层（共33层）";
    q9["朝向"] = "东南";
    q9["年代"] = "2015年";
    houseDataArray.append(q9);

    // Q10
    QJsonObject q10;
    q10["总价"] = "260万";
    q10["户型"] = "2室2厅";
    q10["单价"] = "12000元/㎡";
    q10["楼层"] = "低楼层（共60层）";
    q10["朝向"] = "西南";
    q10["年代"] = "2020年";
    houseDataArray.append(q10);

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
        ui->aiAnalysisText->append("\n❌ AI分析失败：API密钥未配置");
        return;
    }

    connect(client, &DeepSeekClient::responseReceived, this, &MainWindow::onAnalysisCompleted);
    connect(client, &DeepSeekClient::errorOccurred, [this](const QString& error) {
        ui->aiAnalysisText->append("\n❌ AI分析失败：\n" + error);
    });

    client->sendMessage(prompt);
}

// 清空分析按钮点击处理
void MainWindow::on_clearAnalysisBtn_clicked()
{
    ui->aiAnalysisText->clear();
    ui->aiAnalysisText->setPlainText("AI分析报告将显示在这里...");
}

// 询问按钮点击处理
void MainWindow::on_askQuestionBtn_clicked()
{
    QString question = ui->questionInput->text().trimmed();
    if (question.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入您的问题！");
        return;
    }

    ui->aiAnalysisText->clear();
    QString thinkingMessages[] = {
        "🤔 正在深度思考您的问题...",
        "🔍 正在分析和整理信息...",
        "💭 正在组织最合适的回答...",
        "🤖 大模型思考中..."
    };
    int randomIndex = QRandomGenerator::global()->bounded(4);
    ui->aiAnalysisText->append(thinkingMessages[randomIndex] + "\n");

    // 创建测试房源数据
    QJsonArray houseDataArray;

    // Q1-Q10的数据（与分析报告使用相同的数据）
    // Q1
    QJsonObject q1;
    q1["总价"] = "480万";
    q1["户型"] = "4室2厅";
    q1["单价"] = "20000元/㎡";
    q1["楼层"] = "高楼层（共60层）";
    q1["朝向"] = "南";
    q1["年代"] = "2021年";
    houseDataArray.append(q1);

    // Q2
    QJsonObject q2;
    q2["总价"] = "90万";
    q2["户型"] = "1室1厅";
    q2["单价"] = "6000元/㎡";
    q2["楼层"] = "低楼层（共7层）";
    q2["朝向"] = "北";
    q2["年代"] = "1985年";
    houseDataArray.append(q2);

    // Q3
    QJsonObject q3;
    q3["总价"] = "250万";
    q3["户型"] = "2室1厅";
    q3["单价"] = "12000元/㎡";
    q3["楼层"] = "低楼层（共18层）";
    q3["朝向"] = "东南";
    q3["年代"] = "1950年";
    houseDataArray.append(q3);

    // Q4
    QJsonObject q4;
    q4["总价"] = "200万";
    q4["户型"] = "2室2厅";
    q4["单价"] = "11000元/㎡";
    q4["楼层"] = "中楼层（共24层）";
    q4["朝向"] = "东北";
    q4["年代"] = "2008年";
    houseDataArray.append(q4);

    // Q5
    QJsonObject q5;
    q5["总价"] = "250万";
    q5["户型"] = "3室1厅";
    q5["单价"] = "15000元/㎡";
    q5["楼层"] = "中楼层（共18层）";
    q5["朝向"] = "南北";
    q5["年代"] = "1998年";
    houseDataArray.append(q5);

    // Q6
    QJsonObject q6;
    q6["总价"] = "250万";
    q6["户型"] = "3室1厅";
    q6["单价"] = "15000元/㎡";
    q6["楼层"] = "中楼层（共18层）";
    q6["朝向"] = "西北";
    q6["年代"] = "1998年";
    houseDataArray.append(q6);

    // Q7
    QJsonObject q7;
    q7["总价"] = "320万";
    q7["户型"] = "2室1厅";
    q7["单价"] = "19000元/㎡";
    q7["楼层"] = "低楼层（共33层）";
    q7["朝向"] = "南";
    q7["年代"] = "2016年";
    houseDataArray.append(q7);

    // Q8
    QJsonObject q8;
    q8["总价"] = "160万";
    q8["户型"] = "2室1厅";
    q8["单价"] = "9000元/㎡";
    q8["楼层"] = "顶层（共11层）";
    q8["朝向"] = "东";
    q8["年代"] = "2005年";
    houseDataArray.append(q8);

    // Q9
    QJsonObject q9;
    q9["总价"] = "380万";
    q9["户型"] = "3室2厅";
    q9["单价"] = "18000元/㎡";
    q9["楼层"] = "高楼层（共33层）";
    q9["朝向"] = "东南";
    q9["年代"] = "2015年";
    houseDataArray.append(q9);

    // Q10
    QJsonObject q10;
    q10["总价"] = "260万";
    q10["户型"] = "2室2厅";
    q10["单价"] = "12000元/㎡";
    q10["楼层"] = "低楼层（共60层）";
    q10["朝向"] = "西南";
    q10["年代"] = "2020年";
    houseDataArray.append(q10);

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
        ui->aiAnalysisText->append("\n❌ AI问答失败：API密钥未配置");
        return;
    }

    connect(client, &DeepSeekClient::responseReceived, this, &MainWindow::onQuestionAnswered);
    connect(client, &DeepSeekClient::errorOccurred, [this](const QString& error) {
        ui->aiAnalysisText->append("\n❌ AI问答失败：\n" + error);
    });

    client->sendMessage(prompt);
}

// AI分析完成回调
void MainWindow::onAnalysisCompleted(const QString& result)
{
    ui->aiAnalysisText->clear();
    ui->aiAnalysisText->setPlainText("📊 AI智能分析报告\n\n" + result);
}

// AI问答完成回调
void MainWindow::onQuestionAnswered(const QString& result)
{
    ui->aiAnalysisText->clear();
    QString question = ui->questionInput->text();
    ui->aiAnalysisText->setPlainText(QString("❓ 您的问题：%1\n\n🤖 AI回答：\n%2")
                                   .arg(question)
                                   .arg(result));
}

