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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , webPage(nullptr)
{
    // 1. 初始化 Crawl 实例（传入 MainWindow 指针、Web 页面、UI 指针）
    m_crawl = new Crawl(
        this,                  // MainWindow 作为 Crawl 的父对象（生命周期绑定）
        new QWebEnginePage(this), // 新建 Web 页面，父对象设为 MainWindow
        ui                     // 传入 UI 指针（Crawl 用于信号传递，不直接操作）
        );

    // 2. 连接 Crawl 的日志信号 → MainWindow 的 UI 更新槽函数
    // 作用：Crawl 中 emit appendLogSignal(日志) 时，自动更新 textEdit
    connect(m_crawl, &Crawl::appendLogSignal, this, &MainWindow::updateLog);

    ui->setupUi(this);
    webPage = new QWebEnginePage(this);
    QWebEngineSettings* settings = webPage->settings();


    ui->pageSpin->setValue(1);
    ui->pageSpin->setRange(1, 2);
}

// ===================== 析构函数（不变）=====================
MainWindow::~MainWindow()
{
    delete ui;
}
// 槽函数：接收 Crawl 的日志，更新 UI 的 textEdit
void MainWindow::updateLog(const QString& log)
{
    ui->textEdit->append(log);
}

// ===================== 首页爬取按钮（不变）=====================
void MainWindow::on_pushButton_clicked()
{
    ui->textEdit->clear();
    QString startUrl = "https://www.ke.com/";


    ui->textEdit->append("=== 开始爬取贝壳找房首页 ===");
    ui->textEdit->append("初始URL：" + startUrl);
    ui->textEdit->append("————————————————");

}

// ===================== 搜索对比按钮（修正后）=====================
void MainWindow::on_searchCompareBtn_clicked()
{
    // 1. 第一步：先获取 UI 输入（城市名 + 目标页数）
    QString currentCity = ui->keywordEdit->text().trimmed(); // 加 trimmed() 避免空格导致“城市名为空”
    int targetPageCount = ui->pageSpin->value();

    // 2. 第二步：输入校验（先校验，不通过直接返回，避免后续无效操作）
    // 校验1：城市名不能为空
    if (currentCity.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入城市名！（如：北京、上海、广州）");
        return;
    }
    // 校验2：页数限制（最多2页，同步 UI 显示）
    if (targetPageCount > 2) {
        targetPageCount = 2;
        ui->pageSpin->setValue(2);
    }

    // 3. 第三步：清空 UI 日志框（准备显示新爬取日志）
    ui->textEdit->clear();

    // 4. 第四步：调用 Crawl 的接口，启动爬取（核心逻辑）
    m_crawl->startHouseCrawl(currentCity, targetPageCount);

    // （可选）添加“正在启动”提示，让用户知道按钮有效
    ui->textEdit->append("⏳ 正在启动爬取任务...");
}
