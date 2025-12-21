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
{
    ui->setupUi(this);

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
