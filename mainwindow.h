#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWebEnginePage>
#include <QQueue>
#include <QSet>
#include <QMap>
#include <QList>
#include <QStringList>
#include<QLabel>
#include "Crawl.h"
#include "AliCrawl.h"
#include "CrawlThreadManager.h"
#include <QThread>
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_pushButton_clicked();                        // 启动
    void on_searchCompareBtn_clicked();                  // 搜索对比按钮（城市二手房）
    void updateLog(const QString& log);
    void AliLog(const QString &log);
    // 接收日志并显示到UI
  //  void showLog(const QString& log);

private:
    Ui::MainWindow *ui;
    CrawlThreadManager *crawlManager; // 爬虫线程管理器
    //两个独立线程
    QThread *crawlThread;    // 对应 Crawl 实例（m_crawl）
    QThread *aliCrawlThread; // 对应 AliCrawl 实例（a_crawl）
    Crawl* m_crawl;
    AliCrawl* a_crawl;
    // 辅助函数：创建独立的 QWebEnginePage（每个爬虫单独用，避免冲突）
    QWebEnginePage* createWebEnginePage();

    int targetPageCount;
    QString currentCity;
    void setImage(QString,QLabel *);


};

#endif // MAINWINDOW_H
