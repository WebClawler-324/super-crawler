#ifndef CRAWL_H
#define CRAWL_H

#include <QObject>
#include <QWebEnginePage>
#include <QQueue>
#include <QSet>
#include <QMap>
#include <QList>
#include <QStringList>
#include "MYSQL.h"

// 关键修正1：避免循环包含 + 正确前向声明
class MainWindow; // 前向声明 MainWindow（仅用指针，不包含头文件）
namespace Ui { class MainWindow; } // 前向声明 UI 命名空间（替代 #include "ui_mainwindow.h"）

#include "HouseData.h" // 包含房源数据结构体（必须在前面声明之后）
#include "LLMClient.h"

class Crawl : public QObject
{
    Q_OBJECT
private:
    // ===================== 核心成员变量（与实现匹配）=====================
    QWebEnginePage *webPage;
    Ui::MainWindow* m_ui;          // 存储 UI 指针（仅用于信号传递，不直接操作）
    MainWindow* m_mainWindow;      // 存储窗口实例（稳定接收者）

    // ===================== 爬取控制相关 =====================
    QQueue<QString> urlQueue;
    QQueue<QString> searchUrlQueue;
    QSet<QString> crawledUrls;
    QMap<QString, int> urlDepth;
    int currentPageCount = 0;      // 直接初始化（避免未初始化风险）
    int targetPageCount = 1;       // 直接初始化（默认爬1页）

    // ===================== 数据存储相关（关键修正4：删除全局变量，用类内成员）=====================
    QString cookieStr;
    QList<HouseData> houseDataList; // 类内成员（替代全局变量，避免冲突）
    QSet<QString> houseIdSet;       // 类内成员（替代全局变量，避免冲突）
    QString currentCity;
    bool isHomeLoadedForSearch = false; // 直接初始化
    QString pendingSearchKeyword;

    // ===================== 工具函数（私有）=====================
    QString generateRandomPvid();
    QString generateLogId();
    QString getRandomUA();
    int getRandomInterval();

    // 搜索任务标志位（直接初始化）
    bool isProcessingSearchTask = false;

    //添加 LLMClient 成员变量（大模型客户端）
    LLMClient *m_llmClient;

    //把房子数据写入数据库
    Mysql *mysql;
    void IntoDB();

    // 新增：区域相关函数声明
     QMap<QString, QMap<QString, QString>> getRegionCodeMap(); // 区域映射表
     QString regionToCode(const QString& cityName, const QString& regionName); // 区域转编码

     // 分步加载状态枚举
     enum LoadStep {
         Step_LoadHome,    // 第一步：加载贝壳首页
         Step_LoadCity,    // 第二步：加载城市房源页（仅区级需要）
         Step_LoadTarget   // 第三步：加载目标房源页
     };

     QString cityHouseUrl;           // 城市级房源页URL（区级爬取时用）
     bool isCityPageLoading; // 标记当前是否在加载城市页（新增）
     bool isDistrictPageLoading; // 标记当前是否在加载区级页（新增）
     int loadStep = 0;

public:
    // 构造函数（参数不变，保持与实现一致）
    explicit Crawl(MainWindow *mainWindow, QWebEnginePage *webPage, Ui::MainWindow* ui);
    ~Crawl() override; // 关键修正2：删除 =default，手动实现析构函数（清理资源）

    // ===================== 核心函数声明（与实现一致）=====================
    QString cityToPinyin(const QString& cityName);
    QString getFirstLetter(int index);
    void extractHouseData(const QString& html);
    void showHouseCompareResult();

    // 新增：启动房源爬取的接口（供 MainWindow 调用）
    void startHouseCrawl(const QString& city, int targetPages);
    // ===================== 静态常量声明（类内共享）=====================
    static const int REQUEST_INTERVAL;
    static const int MAX_DEPTH;
    static const int MIN_REQUEST_INTERVAL;
    static const int MAX_REQUEST_INTERVAL;
    static const QStringList USER_AGENT_POOL;
    static const int MIN_INTER_PAGE_DELAY ;  // 分页间最小延迟30秒
    static const int MAX_INTER_PAGE_DELAY ;  // 分页间最大延迟45秒
    static const int MIN_PAGE_STAY_TIME;    // 每页最小停留15秒
    static const int MAX_PAGE_STAY_TIME;    // 每页最大停留25秒

    QQueue<QString> getSearchUrl(){
        return searchUrlQueue;
    }

    int getCurrentPage(){
        return currentPageCount;
    }
signals:
    // 日志信号（用于主线程更新 UI）
    void appendLogSignal(const QString& log);
    void startCrawlSignal(const QString& city, int targetPages);

private slots:
    void onInitFinishedLog();

    // ===================== 核心业务槽函数（与实现一致）=====================
    void onPageLoadFinished(bool ok);
    void processNextUrl();
    void processSearchUrl();
    void simulateHumanBehavior();

    // ===================== Cookie管理函数（与实现一致）=====================
    void loadCookiesFromFile(const QString& filePath = "");
    void saveCookiesToFile(const QString& filePath = "");

    // ===================== 数据解析函数（关键修正3：参数名与实现统一）=====================
    void extractKeData(const QString& html, const QString& currentUrl); // 原 baseUrl → currentUrl（与cpp一致）


};

#endif // CRAWL_H
