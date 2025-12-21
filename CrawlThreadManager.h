#ifndef CRAWLTHREADMANAGER_H
#define CRAWLTHREADMANAGER_H

#include <QThread>
#include <QWebEnginePage>
#include "Crawl.h"
#include "AliCrawl.h"

class CrawlThreadManager : public QObject {
    Q_OBJECT
public:
    // 构造函数接收主线程的UI和窗口指针（用于信号传递）
    explicit CrawlThreadManager(MainWindow *mainWindow, Ui::MainWindow *ui, QObject *parent = nullptr)
        : QObject(parent), m_mainWindow(mainWindow), m_ui(ui) {}

    // 启动贝壳爬虫线程（修正参数传递和资源初始化）
    void startKeCrawl(const QString& city, int pages) {
        // 1. 创建线程和独立的Web页面（每个爬虫必须有自己的QWebEnginePage）
        QThread *keThread = new QThread(this);
        QWebEnginePage *kePage = new QWebEnginePage(); // 子线程专用Web页面
        Crawl *keCrawl = new Crawl(m_mainWindow, kePage, m_ui); // 正确传递构造参数

        // 2. 移动爬虫到子线程（Web页面会随爬虫一起进入子线程）
        keCrawl->moveToThread(keThread);

        // 3. 连接线程启动信号到爬取入口
        connect(keThread, &QThread::started, keCrawl, [=]() {
            keCrawl->startHouseCrawl(city, pages); // 调用Crawl的爬取方法
        });

        // 4. 日志信号转发（跨线程自动使用QueuedConnection）
        connect(keCrawl, &Crawl::appendLogSignal, this, &CrawlThreadManager::onLogReceived);

        // 5. 资源自动释放链
        // 爬虫完成后销毁自身 → 触发线程退出 → 线程销毁
        connect(keCrawl, &Crawl::destroyed, keThread, &QThread::quit);
        connect(keThread, &QThread::finished, keThread, &QThread::deleteLater);
        // 确保Web页面随爬虫一起销毁（避免内存泄漏）
        //connect(keCrawl, &Crawl::destroyed, kePage, &QWebEnginePage::deleteLater);

        // 6. 启动线程
        keThread->start();
    }

    // 启动阿里爬虫线程（同上逻辑）
    void startAliCrawl(const QString& city, int pages) {
        QThread *aliThread = new QThread(this);
        QWebEnginePage *aliPage = new QWebEnginePage(); // 阿里专用Web页面
        AliCrawl *aliCrawl = new AliCrawl(m_mainWindow, aliPage, m_ui); // 正确传递构造参数

        aliCrawl->moveToThread(aliThread);

        connect(aliThread, &QThread::started, aliCrawl, [=]() {
            aliCrawl->startHouseCrawl(city, pages); // 调用AliCrawl的爬取方法
        });

        connect(aliCrawl, &AliCrawl::appendLogSignal, this, &CrawlThreadManager::onLogReceived);

        // 资源释放链
        connect(aliCrawl, &AliCrawl::destroyed, aliThread, &QThread::quit);
        connect(aliThread, &QThread::finished, aliThread, &QThread::deleteLater);
        //connect(aliCrawl, &AliCrawl::destroyed, aliPage, &QWebEnginePage::deleteLater);

        aliThread->start();
    }

signals:
    void logReceived(const QString& log); // 转发日志到主线程UI

private slots:
    void onLogReceived(const QString& log) {
        emit logReceived(log); // 透传日志信号
    }

private:
    MainWindow *m_mainWindow; // 存储主线程窗口指针（用于信号接收）
    Ui::MainWindow *m_ui;     // 存储UI指针（用于爬虫类内部信号传递）
};

#endif // CRAWLTHREADMANAGER_H
