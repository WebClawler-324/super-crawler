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


    setImage("Cover", ui->Image1);


    ui->pageSpin->setValue(1);
   // ui->pageSpin->setRange(1, 2);
}


MainWindow::~MainWindow()
{

    if (m_crawl) {
        m_crawl->deleteLater();
        m_crawl = nullptr;
    }
    delete ui;
}
// 槽函数接收 Crawl 的日志
void MainWindow::updateLog(const QString& log)
{
    ui->textEdit->append(log);
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

    m_crawl->startHouseCrawl(currentCity, targetPageCount);

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

