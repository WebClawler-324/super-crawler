// BaseCrawler.h
#ifndef BASECRAWLER_H
#define BASECRAWLER_H

#include <QObject>
#include <QWebEnginePage>
#include <QQueue>
#include <QSet>
#include<QRandomGenerator>
#include "HouseData.h"
#include "MYSQL.h"

class BaseCrawler : public QObject {
    Q_OBJECT
public:
    explicit BaseCrawler(QObject *parent = nullptr, QWebEnginePage *page = nullptr)
        : QObject(parent), webPage(page), mysql(new Mysql()) {
        if (!webPage) webPage = new QWebEnginePage(this);
        mysql->connectDatabase();
    }

    virtual ~BaseCrawler() {
        webPage->deleteLater();
        delete mysql;
    }

    // 纯虚函数：启动爬取（子类实现具体网站逻辑）
    virtual void startCrawl(const QString& city, int targetPages) = 0;

signals:
    void appendLogSignal(const QString& log);

protected:
    QWebEnginePage *webPage;
    Mysql *mysql;
    QQueue<QString> urlQueue;       // 待爬URL队列
    QSet<QString> crawledUrls;      // 已爬URL去重
    QList<HouseData> houseDataList; // 房源数据列表
    QString currentCity;            // 当前城市
    int targetPageCount;            // 目标页数

    // 通用工具函数（子类可复用）
    QString generateRandomUA() {
        static const QStringList UA_POOL = {
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/138.0.0.0",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_0) Safari/605.1.15"
        };
        return UA_POOL.at(QRandomGenerator::global()->bounded(UA_POOL.size()));
    }

    // 通用数据存储（插入数据库）
    void saveToDB(const HouseData& data) {
        mysql->insertInfo(data);
    }
};

#endif // BASECRAWLER_H
