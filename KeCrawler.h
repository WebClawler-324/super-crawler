// KeCrawler.h
#ifndef KECRAWLER_H
#define KECRAWLER_H

#include "BaseCrawler.h"

class KeCrawler : public BaseCrawler {
    Q_OBJECT
public:
    explicit KeCrawler(QObject *parent = nullptr, QWebEnginePage *page = nullptr)
        : BaseCrawler(parent, page) {
        // 贝壳特定初始化（如Cookie加载）
        loadCookiesFromFile("ke_cookies.txt");
    }

    void startCrawl(const QString& city, int targetPages) override {
        currentCity = city;
        targetPageCount = targetPages;
        // 贝壳URL生成逻辑（复用原startHouseCrawl核心代码）
        QString cityPinyin = cityToPinyin(city);
        QString url = QString("https://%1.ke.com/ershoufang/pg%2/")
                          .arg(cityPinyin).arg(targetPages);
        urlQueue.enqueue(url);
        emit appendLogSignal("贝壳爬取任务启动：" + url);
        processNextUrl(); // 开始处理队列
    }

private:
    // 复用原贝壳特有的方法（城市转拼音、数据提取等）
    QString cityToPinyin(const QString& cityName) { /* 原实现 */ }
    void extractHouseData(const QString& html) { /* 原实现，适配贝壳HTML */ }
    void processNextUrl() { /* 原实现，处理贝壳URL */ }
    void loadCookiesFromFile(const QString& path) { /* 原实现 */ }
};

#endif // KECRAWLER_H
