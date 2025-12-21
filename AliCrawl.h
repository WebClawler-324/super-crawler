#ifndef ALICRAWL_H
#define ALICRAWL_H

#include <QObject>
#include <QWebEnginePage>
#include <QQueue>
#include <QSet>
#include <QMap>
#include <QList>
#include <QStringList>
#include "MYSQL.h"

class MainWindow;
namespace Ui { class MainWindow; }

#include "HouseInfo.h"

class AliCrawl : public QObject
{
    Q_OBJECT
private:
    QWebEnginePage *webPage;
    Ui::MainWindow* m_ui;
    MainWindow* m_mainWindow;

    QQueue<QString> urlQueue;
    QQueue<QString> searchUrlQueue;
    QSet<QString> crawledUrls;
    QMap<QString, int> urlDepth;
    int currentPageCount = 0;
    int targetPageCount = 1;

    QString cookieStr;
    QList<HouseInfo> houseDataList;
    QSet<QString> houseIdSet;
    QString currentCity;
    bool isHomeLoadedForSearch = false;
    QString pendingSearchKeyword;
    bool isProcessingSearchTask = false;

    Mysql *mysql;
    QString generateRandomPvid();
    QString generateLogId();
    QString getRandomUA();
    int getRandomInterval();

public:
    explicit AliCrawl(MainWindow *mainWindow, QWebEnginePage *webPage, Ui::MainWindow* ui);
    ~AliCrawl() override;

    QString cityToPinyin(const QString& cityName);
    QString getFirstLetter(int index);
    void extractHouseData(const QString& html);
    void showHouseCompareResult();
    void startHouseCrawl(const QString& city, int targetPages);

    static const int REQUEST_INTERVAL;
    static const int MAX_DEPTH;
    static const int MIN_REQUEST_INTERVAL;
    static const int MAX_REQUEST_INTERVAL;
    static const QStringList USER_AGENT_POOL;

    QQueue<QString> getSearchUrl(){ return searchUrlQueue; }
    int getCurrentPage(){ return currentPageCount; }

signals:
    void appendLogSignal(const QString& log);
    // 新增：触发爬取的信号（参数和startHouseCrawl一致）
    void startCrawlSignal(const QString& city, int targetPages);

private slots:
    void onInitFinishedLog();
    void onPageLoadFinished(bool ok);
    void processNextUrl();
    void processSearchUrl();
    void simulateHumanBehavior();
    void loadCookiesFromFile(const QString& filePath = "");
    void saveCookiesToFile(const QString& filePath = "");
    void extractAliData(const QString& html, const QString& currentUrl);
};

#endif // ALICRAWL_H
