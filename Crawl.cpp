#include "Crawl.h"
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
#include <QStringEncoder>
#include <QFile>
#include <QTextStream>
#include<QChar>

// 类内静态常量初始化
const int Crawl::REQUEST_INTERVAL = 3000;
const int Crawl::MAX_DEPTH = 1;
const int Crawl::MIN_REQUEST_INTERVAL = 8000;
const int Crawl::MAX_REQUEST_INTERVAL = 15000;
const QStringList Crawl::USER_AGENT_POOL = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 11.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_6) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36"
};
// 全局变量（不变）
QList<HouseData> houseDataList;
QSet<QString> houseIdSet;
QString currentCity;
bool isHomeLoadedForSearch = false;
QString pendingSearchKeyword;

//嵌套映射：城市→区→对应拼音
QMap<QString, QMap<QString, QString>> Crawl::getRegionCodeMap() {
    QMap<QString, QMap<QString, QString>> regionPinyinMap;

    // 1. 北京（区域拼音，适配安居客URL规则）
    QMap<QString, QString> beijingRegions;
    beijingRegions["东城区"] = "dongcheng";
    beijingRegions["西城区"] = "xicheng";
    beijingRegions["朝阳区"] = "chaoyang";
    beijingRegions["海淀区"] = "haidian";
    beijingRegions["丰台区"] = "fengtai";
    beijingRegions["石景山区"] = "shijingshan";
    beijingRegions["通州区"] = "tongzhou";
    beijingRegions["昌平区"] = "changping";
    regionPinyinMap["北京"] = beijingRegions;

    // 2. 上海（区域拼音）
    QMap<QString, QString> shanghaiRegions;
    shanghaiRegions["浦东新区"] = "pudongxin";
    shanghaiRegions["黄浦区"] = "huangpu";
    shanghaiRegions["静安区"] = "jingan";
    shanghaiRegions["徐汇区"] = "xuhui";
    shanghaiRegions["闵行区"] = "minhang";
    shanghaiRegions["杨浦区"] = "yangpu";
    regionPinyinMap["上海"] = shanghaiRegions;

    // 3. 广州（区域拼音）
    QMap<QString, QString> guangzhouRegions;
    guangzhouRegions["天河区"] = "tianhe";
    guangzhouRegions["越秀区"] = "yuexiu";
    guangzhouRegions["海珠区"] = "haizhu";
    guangzhouRegions["番禺区"] = "panyu";
    guangzhouRegions["白云区"] = "baiyun";
    regionPinyinMap["广州"] = guangzhouRegions;

    // 4. 杭州（区域拼音）
    QMap<QString, QString> hangzhouRegions;
    hangzhouRegions["西湖区"] = "xihu";
    hangzhouRegions["滨江区"] = "binjiang";
    hangzhouRegions["余杭区"] = "yuhang";
    hangzhouRegions["萧山区"] = "xiaoshan";
    hangzhouRegions["拱墅区"] = "gongshu";
    regionPinyinMap["杭州"] = hangzhouRegions;

    return regionPinyinMap;
}

// ===================== 城市/区县转拼音（适配安居客）=====================
QString Crawl::regionToCode(const QString& cityName, const QString& districtName) {
    QMap<QString, QMap<QString, QString>> regionPinyinMap = getRegionCodeMap();

    // 优先返回区域拼音
    if (!districtName.isEmpty()) {
        if (regionPinyinMap.contains(cityName) && regionPinyinMap[cityName].contains(districtName)) {
            return regionPinyinMap[cityName][districtName];
        }
        emit appendLogSignal(QString("⚠️ 未支持「%1-%2」的区域拼音，请手动添加到getRegionCodeMap！").arg(cityName, districtName));
        return "";
    }

    //返回安居客城市拼音
    QMap<QString, QString> cityPinyinMap = {
        {"北京", "bj"}, {"上海", "sh"}, {"广州", "gz"},
        {"深圳", "sz"}, {"杭州", "hz"}, {"南京", "nj"},
        {"成都", "cd"}, {"重庆", "cq"}, {"武汉", "wh"},
        {"西安", "xa"}, {"天津", "tj"}, {"苏州", "sz"}
    };
    if (cityPinyinMap.contains(cityName)) {
        return cityPinyinMap[cityName];
    }

    emit appendLogSignal(QString("⚠️ 未支持「%1」的城市拼音，请手动添加到cityPinyinMap！").arg(cityName));
    return "";
}

//根据区位码获取首字母
QString Crawl::getFirstLetter(int index) {
    const QStringList letters = {"A", "B", "C", "D", "E", "F", "G", "H", "J", "K", "L", "M", "N",
                                 "O", "P", "Q", "R", "S", "T", "W", "X", "Y", "Z"};
    const int ranges[] = {16, 54, 90, 128, 154, 179, 205, 231, 285, 316, 347, 373, 399,
                          410, 429, 452, 475, 508, 534, 558, 584, 611, 676};

    for (int i = 0; i < letters.size(); i++) {
        if (index < ranges[i]) {
            return letters[i];
        }
    }
    return "A";
}

// 模拟真人行为
void Crawl::simulateHumanBehavior() {
    QStringList jsScrolls = {
        QString("window.scrollTo(0, %1);").arg(QRandomGenerator::global()->bounded(400, 600)),
        QString("window.scrollTo(0, %1);").arg(QRandomGenerator::global()->bounded(1000, 1500)),
        QString("window.scrollTo(0, document.body.scrollHeight);")
    };

    int delay = 0;
    for (QString js : jsScrolls) {
        QTimer::singleShot(delay, this, [this, js]() {
            if (this == nullptr || webPage == nullptr) return;
            webPage->runJavaScript(js);
            emit appendLogSignal("🤖 模拟真人滚动：执行JS：" + js);
        });
        delay += 2000;
    }

    int totalStayTime = 7000 + QRandomGenerator::global()->bounded(3000);
    emit appendLogSignal("🤖 模拟真人浏览：总停留" + QString::number(totalStayTime/1000) + "秒");
}

// Cookie管理函数（保留ke_cookies.txt文件名不变）
void Crawl::loadCookiesFromFile(const QString& filePath) {
    QFile file(filePath.isEmpty() ? "ke_cookies.txt" : filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit appendLogSignal(QString("⚠️ Cookie文件加载失败：%1（将使用默认Cookie）").arg(file.fileName()));
        // 安居客默认Cookie（可忽略，主要依赖用户手动配置）
        cookieStr = "anjuke_uuid=7477f2d0-8c9c-4e0d-b746-7a9f8499c9c8; s_ViewType=1; select_city=110000; city=beijing;";
        return;
    }

    QTextStream in(&file);
    cookieStr = in.readAll().trimmed();
    file.close();
    emit appendLogSignal(QString("✅ 成功加载Cookie：%1").arg(cookieStr.isEmpty() ? "无" : "已加载（来自ke_cookies.txt）"));
}

void Crawl::saveCookiesToFile(const QString& filePath) {
    QString savePath = filePath.isEmpty() ? "ke_cookies.txt" : filePath;
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit appendLogSignal(QString("⚠️ Cookie文件保存失败：%1").arg(savePath));
        return;
    }

    QTextStream out(&file);
    out << cookieStr;
    file.close();
    emit appendLogSignal(QString("✅ Cookie已保存到：%1").arg(savePath));
}

// 工具函数（不变）
QString Crawl::generateRandomPvid() {
    const QString chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    QString pvid;
    QRandomGenerator* gen = QRandomGenerator::global();
    for (int i = 0; i < 32; i++) {
        pvid += chars.at(gen->bounded(chars.length()));
    }
    return pvid;
}

QString Crawl::generateLogId() {
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    int random = QRandomGenerator::global()->bounded(1000, 9999);
    return QString::number(timestamp) + QString::number(random);
}

QString Crawl::getRandomUA() {
    int index = QRandomGenerator::global()->bounded(USER_AGENT_POOL.size());
    return USER_AGENT_POOL.at(index);
}

int Crawl::getRandomInterval() {
    return QRandomGenerator::global()->bounded(MIN_REQUEST_INTERVAL, MAX_REQUEST_INTERVAL);
}

Crawl::Crawl(MainWindow *mainWindow, QWebEnginePage *webPageParam, Ui::MainWindow* ui)
    : QObject(nullptr)
    , webPage(nullptr)
    , m_ui(ui)
    , m_mainWindow(mainWindow)
    , isProcessingSearchTask(false)
    , currentPageCount(0)
    , targetPageCount(1)
{
    //初始化数据库类对象
    mysql=new Mysql();
    //连接数据库
    mysql->connectDatabase();

    // webPage 初始化
    if (webPageParam != nullptr) {
        webPage = webPageParam;
        webPage->setParent(this);
    } else {
        webPage = new QWebEnginePage(this);
    }
    connect(this, &Crawl::startCrawlSignal, this, &Crawl::startHouseCrawl, Qt::QueuedConnection);

    // WebEngine配置
    QWebEngineSettings* settings =webPage->settings();
    settings->setAttribute(QWebEngineSettings::AutoLoadIconsForPage, false);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);

    // 连接页面加载完成信号
    connect(webPage, &QWebEnginePage::loadFinished, this, &Crawl::onPageLoadFinished);

    // 加载 Cookie（保留ke_cookies.txt）
    loadCookiesFromFile();

    int delayMs = 1000 + QRandomGenerator::global()->bounded(2000);
    QTimer::singleShot(
        delayMs,
        this,
        SLOT(onInitFinishedLog())
        );
}

Crawl::~Crawl() {
    // 清理 Web
    if (webPage != nullptr) {
        webPage->deleteLater();
        webPage = nullptr;
    }

    // 清空队列，释放资源
    urlQueue.clear();
    searchUrlQueue.clear();
    crawledUrls.clear();
    urlDepth.clear();
    houseDataList.clear();
    houseIdSet.clear();
    //与数据库断联
    mysql->close();

    emit appendLogSignal("🔌 Crawl 实例已安全销毁，资源释放完成");
}

void Crawl::onInitFinishedLog() {
    emit appendLogSignal("✅ 浏览器环境初始化完成，可开始爬取安居客（低风控模式）");
    emit appendLogSignal("💡 使用说明：在输入框输入城市名（如：北京、上海），点击搜索对比按钮");
}

//处理普通URL（适配安居客首页）
void Crawl::processNextUrl() {
    if (urlQueue.isEmpty()) {
        emit appendLogSignal("\n=== 安居客首页爬取完成 ===");
        return;
    }

    QString currentUrl = urlQueue.dequeue();
    emit appendLogSignal("\n📌 正在加载：" + currentUrl);

    QUrl url(currentUrl);
    QWebEngineHttpRequest request(url);

    QString randomUA = getRandomUA();
    request.setHeader(QByteArray("User-Agent"), randomUA.toUtf8());
    request.setHeader(QByteArray("Referer"), QByteArray("https://www.anjuke.com/"));
    request.setHeader(QByteArray("Accept"), QByteArray("text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"));
    request.setHeader(QByteArray("Accept-Encoding"), QByteArray("gzip, deflate, br"));
    request.setHeader(QByteArray("Accept-Language"), QByteArray("zh-CN,zh;q=0.9,en;q=0.8"));
    request.setHeader(QByteArray("Cache-Control"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Connection"), QByteArray("keep-alive"));
    request.setHeader(QByteArray("Pragma"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Sec-Ch-Ua"), QByteArray("\"Chromium\";v=\"138\", \"Not=A?Brand\";v=\"8\", \"Google Chrome\";v=\"138\""));
    request.setHeader(QByteArray("Sec-Ch-Ua-Mobile"), QByteArray("?0"));
    request.setHeader(QByteArray("Sec-Ch-Ua-Platform"), QByteArray("\"Windows\""));
    request.setHeader(QByteArray("Sec-Fetch-Dest"), QByteArray("document"));
    request.setHeader(QByteArray("Sec-Fetch-Mode"), QByteArray("navigate"));
    request.setHeader(QByteArray("Sec-Fetch-Site"), QByteArray("same-origin"));
    request.setHeader(QByteArray("Upgrade-Insecure-Requests"), QByteArray("1"));

    if (!cookieStr.isEmpty()) {
        request.setHeader(QByteArray("Cookie"), cookieStr.toUtf8());
    } else {
        emit appendLogSignal("⚠️ 无有效Cookie，可能触发风控！");
    }

    webPage->load(request);
}

//页面加载完成槽函数（适配安居客风控检测）
void Crawl::onPageLoadFinished(bool ok) {
    if (this == nullptr || webPage == nullptr) return;

    QString currentUrl = webPage->url().toString();
    bool isSearchTask = isProcessingSearchTask;

    // 处理首页加载完成后的搜索任务（安居客首页）
    if (isHomeLoadedForSearch && currentUrl.contains("anjuke.com") && !currentUrl.contains("sale")) {
        emit appendLogSignal("✅ 安居客首页加载完成，延迟4-6秒后开始爬取二手房...");
        int homeDelay = 4000 + QRandomGenerator::global()->bounded(2000);

        QTimer::singleShot(
            homeDelay,
            this,
            [this]() {
                emit appendLogSignal("🔍 开始执行二手房爬取任务...");
                processSearchUrl();
            }
            );

        isHomeLoadedForSearch = false;
        pendingSearchKeyword.clear();
        return;
    }

    // 安居客风控检测（验证页关键词适配）
    bool isRiskPage = currentUrl.contains("verify", Qt::CaseInsensitive) ||
                      currentUrl.contains("captcha", Qt::CaseInsensitive) ||
                      currentUrl.contains("security", Qt::CaseInsensitive) ||
                      currentUrl.contains("antispam", Qt::CaseInsensitive) ||
                      currentUrl.contains("safe", Qt::CaseInsensitive);
    if (isRiskPage) {
        emit appendLogSignal("❌ 触发安居客风控！跳转至验证页：" + currentUrl);
        emit appendLogSignal("💡 解决方案：");
        emit appendLogSignal("  1. 关闭VPN/代理，使用本地IP；");
        emit appendLogSignal("  2. 降低爬取频率，单次仅爬1页；");
        emit appendLogSignal("  3. 重新获取安居客Cookie并更新ke_cookies.txt。");
        searchUrlQueue.clear();
        isProcessingSearchTask = false;
        return;
    }

    // 加载失败处理
    if (!ok) {
        emit appendLogSignal("❌ 加载失败：" + currentUrl);
        if (isSearchTask) {
            emit appendLogSignal("⚠️ 房源页加载失败，15秒后重试...");
            QTimer::singleShot(15000, this, &Crawl::processSearchUrl);
        } else {
            QTimer::singleShot(8000, this, &Crawl::processNextUrl);
        }
        return;
    }

    // 加载成功 模拟真人行为
    emit appendLogSignal("✅ 页面加载成功：" + currentUrl);
    simulateHumanBehavior();

    // 渲染延迟
    int renderDelay = currentUrl.contains("sale")
                          ? 15000 + QRandomGenerator::global()->bounded(5000)
                          : 4000 + QRandomGenerator::global()->bounded(3000);
    if (currentUrl.contains("sale")) {
        emit appendLogSignal("⏳ 房源页等待完全渲染（" + QString::number(renderDelay/1000) + "秒）...");
    }

    // 渲染延迟后提取数据
    QTimer::singleShot(renderDelay, this, [this, currentUrl, isSearchTask]() {
        if (this == nullptr || webPage == nullptr) return;

        webPage->toHtml([this, currentUrl, isSearchTask](const QString& html) {
            bool hasHouseNode = html.contains("div class=\"house-item\"") || html.contains("li class=\"house-list-item\"");
            emit appendLogSignal(QString("📋 获取到HTML：%1房源节点").arg(hasHouseNode ? "包含" : "不包含"));

            // 提取安居客房源数据
            if (isSearchTask && currentUrl.contains("sale")) {
                extractHouseData(html);
                currentPageCount++;

                // 处理下一页或结束
                QString nextLog;
                isProcessingSearchTask = false;
                nextLog = QString("✅ 第%1页爬取完成，无下一页（一次只爬1页），准备显示结果...").arg(targetPageCount);
                QTimer::singleShot(1000, this, &Crawl::showHouseCompareResult);

                emit appendLogSignal(nextLog);
            } else if (!isSearchTask) {
                extractKeData(html, currentUrl);
                QTimer::singleShot(8000, this, &Crawl::processNextUrl);
            }
        });
    });
}

//解析普通页面（适配安居客）
void Crawl::extractKeData(const QString& html, const QString& baseUrl)
{
    emit appendLogSignal("🔍 开始解析安居客页面...");

    QRegularExpression cityRegex(R"(<a\s+href=["'](https?://[^.]+.anjuke.com/)["']\s+class=["']city-item["'].*?>([\s\S]*?)</a>)",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator cityIt = cityRegex.globalMatch(html);
    while (cityIt.hasNext()) {
        QRegularExpressionMatch match = cityIt.next();
        QString cityUrl = match.captured(1).trimmed();
        QString cityName = match.captured(2).trimmed();
        cityName.remove(QRegularExpression("<[^>]*>"));

        if (!crawledUrls.contains(cityUrl) && urlDepth[baseUrl] < MAX_DEPTH) {
            crawledUrls.insert(cityUrl);
            urlQueue.enqueue(cityUrl);
            urlDepth[cityUrl] = urlDepth[baseUrl] + 1;
            emit appendLogSignal("🏙️  城市：" + cityName + " | 链接：" + cityUrl);
        }
    }

    emit appendLogSignal("✅ 解析完成：" + baseUrl);
    emit appendLogSignal("————————————————");
}

// 提取安居客房源数据（核心修改：适配安居客页面结构）
void Crawl::extractHouseData(const QString& html)
{
    emit appendLogSignal("🔍 开始提取安居客二手房房源数据...");

    // ========== 修正后的外层房源正则：捕获完整标签，优化标志 ==========
    QRegularExpression houseRegex(
        R"(<div[^>]*?class=["']\s*property\s*["'][^>]*>([\s\S]*?)(?=<div[^>]*?class=["']\s*property\s*["']|$))",
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
        );
    QRegularExpressionMatchIterator houseIt = houseRegex.globalMatch(html);
    int extractCount = 0;

    while (houseIt.hasNext()) {
        QRegularExpressionMatch houseMatch = houseIt.next();
        QString houseHtml = houseMatch.captured(0).trimmed();

        // 调试日志（保留）
        bool hasH3CoreClass = houseHtml.contains("property-content-title-name");
        bool hasPriceClass = houseHtml.contains("property-price-total-num");
        bool hasCommunityClass = houseHtml.contains("property-content-info-comm-name");
        bool hasHouseTypeClass = houseHtml.contains("property-content-info-attribute");
        emit appendLogSignal(QString("\n🔍 房源片段调试：长度=%1 | 包含h3核心class=%2 | 包含总价class=%3 | 包含小区名class=%4 | 包含户型class=%5")
                                 .arg(houseHtml.length())
                                 .arg(hasH3CoreClass ? "是" : "否")
                                 .arg(hasPriceClass ? "是" : "否")
                                 .arg(hasCommunityClass ? "是" : "否")
                                 .arg(hasHouseTypeClass ? "是" : "否"));

        // 跳过空片段 + 无效片段（无核心标题class，或缺少关键字段class）
        if (houseHtml.isEmpty() || !hasH3CoreClass) {
            continue;
        }
        // ========== 初始化字段（关键修改1：将面积、朝向等初始化为空字符串，而非“未知”） ==========
        QString title = "未知";
        QString communityName = "未知";
        QString totalPrice = "未知";
        QString unitPrice = "未知";
        QString houseType = "未知";
        QString area = ""; // 初始化为空，确保 isEmpty() 返回 true
        QString orientation = ""; // 初始化为空，确保 isEmpty() 返回 true
        QString floor = ""; // 初始化为空，确保 isEmpty() 返回 true
        QString buildingYear = ""; // 初始化为空，确保 isEmpty() 返回 true
        QString houseUrl = "未知";

        // ========== 标题提取：父容器+h3双重锁定（兼容所有3种样式） ==========
        // 主正则：兼容title和class属性任意顺序，提取title属性值（优先级更高）
        QRegularExpression titleRegex(
            R"(<h3[^>]*?(?:title=["']([^"']+)["'][^>]*?class|class=["'][^"']*property-content-title-name[^"']*["'][^>]*?title=["']([^"']+)["'])[^>]*>.*?</h3>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch titleMatch = titleRegex.match(houseHtml);
        if (titleMatch.hasMatch()) {
            // 捕获组1（title在前）或捕获组2（class在前），取非空值
            QString title1 = titleMatch.captured(1).trimmed();
            QString title2 = titleMatch.captured(2).trimmed();
            title = !title1.isEmpty() ? title1 : title2;
            // 清理连续空格（保留有效空格）
            title.replace(QRegularExpression(R"(\s+)"), " ");
            emit appendLogSignal(QString("✅ 标题提取成功：%1").arg(title));
        } else {
            // 兜底：匹配带目标class的h3，提取标签内文本（兼容任意属性顺序）
            QRegularExpression titleFallbackRegex(
                R"(<h3[^>]*class=["'][^"']*property-content-title-name[^"']*["'][^>]*>(.*?)</h3>)",
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
                );
            QRegularExpressionMatch fallbackMatch = titleFallbackRegex.match(houseHtml);
            if (fallbackMatch.hasMatch()) {
                title = fallbackMatch.captured(1).trimmed();
                title.replace(QRegularExpression(R"(\s+)"), " ");
                emit appendLogSignal(QString("✅ 标题提取成功（来自标签内文本）：%1").arg(title));
            } else {
                emit appendLogSignal("❌ 未匹配到带property-content-title-name的h3标签");
            }
        }

        //小区名提取
        QRegularExpression communityRegex(
            R"(<p\s+[^>]*class=["'][^"']*?property-content-info-comm-name[^"']*?["'][^>]*>([\s\S]*?)</p>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch communityMatch = communityRegex.match(houseHtml);
        if (communityMatch.hasMatch()) {
            communityName = communityMatch.captured(1).trimmed();
            communityName.remove(QRegularExpression("<[^>]*>"));
            communityName.replace(QRegularExpression(R"(\s+)"), " ");
            emit appendLogSignal(QString("✅ 小区名提取成功：%1").arg(communityName));
        } else {
            emit appendLogSignal("❌ 小区名提取失败");
            communityName = "未知";
        }

        //总价提取
        QRegularExpression totalPriceNumRegex(
            R"(<span\s+[^>]*class=["'][^"']*?property-price-total-num[^"']*?["'][^>]*>([\d.]+)</span>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch totalPriceNumMatch = totalPriceNumRegex.match(houseHtml);
        if (totalPriceNumMatch.hasMatch()) {
            QString priceNum = totalPriceNumMatch.captured(1).trimmed();
            QRegularExpression totalPriceTextRegex(
                R"(<span\s+[^>]*class=["'][^"']*?property-price-total-text[^"']*?["'][^>]*>(万)</span>)",
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
                );
            QRegularExpressionMatch unitMatch = totalPriceTextRegex.match(houseHtml);
            QString priceUnit = unitMatch.hasMatch() ? unitMatch.captured(1).trimmed() : "";
            totalPrice = priceNum + priceUnit;
            emit appendLogSignal(QString("✅ 房源总价提取成功：%1").arg(totalPrice));
        } else {
            emit appendLogSignal("❌ 房源总价提取失败（未匹配到总价数字）");
        }

        //单价提取
        QRegularExpression unitPriceRegex(
            R"(<p\s+[^>]*class=["'][^"']*?property-price-average[^"']*?["'][^>]*>([\s\S]*?)</p>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch unitPriceMatch = unitPriceRegex.match(houseHtml);
        if (unitPriceMatch.hasMatch()) {
            QString priceText = unitPriceMatch.captured(1).trimmed();
            priceText.replace(QRegularExpression(R"(\s+)"), " ");
            priceText = priceText.trimmed();
            if (!priceText.isEmpty()) {
                unitPrice = priceText;
                emit appendLogSignal(QString("✅ 房源单价提取成功：%1").arg(unitPrice));
            } else {
                emit appendLogSignal("❌ 房源单价提取失败（提取文本为空）");
            }
        } else {
            emit appendLogSignal("❌ 房源单价提取失败（未匹配到单价容器）");
        }

        // 户型提取
        QRegularExpression houseTypeRegex(
            R"(<p\s+[^>]*class=["'][^"']*property-content-info-text[^"']*property-content-info-attribute[^"']*["'][^>]*>([\s\S]*?)</p>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch houseTypeMatch = houseTypeRegex.match(houseHtml);
        if (houseTypeMatch.hasMatch()) {
            QString typeHtml = houseTypeMatch.captured(1).trimmed();
            typeHtml.remove(QRegularExpression("<span[^>]*>"));
            typeHtml.remove(QRegularExpression("</span>"));
            typeHtml.replace(QRegularExpression(R"(\s+)"), "");
            typeHtml = typeHtml.trimmed();
            if (!typeHtml.isEmpty()) {
                houseType = typeHtml;
                emit appendLogSignal(QString("✅ 户型提取成功：%1").arg(houseType));
            } else {
                emit appendLogSignal("❌ 户型提取失败（文本为空）");
            }
        } else {
            emit appendLogSignal("❌ 户型提取失败（未匹配到户型容器）");
        }

        // 基础信息列表构建
        QRegularExpression baseInfoRegex(
            R"(<p\s+[^>]*class=["'][^"']*property-content-info-text[^"']*["'][^>]*>([\s\S]*?)</p>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatchIterator baseInfoIt = baseInfoRegex.globalMatch(houseHtml);
        QList<QString> baseInfoList;

        while (baseInfoIt.hasNext()) {
            QRegularExpressionMatch infoMatch = baseInfoIt.next();
            QString infoHtml = infoMatch.captured(1).trimmed();
            infoHtml.remove(QRegularExpression("<[^>]*>"));       // 移除所有HTML标签
            infoHtml.replace(QRegularExpression(R"(\s+)"), " ");  // 多个空白替换为单个空格
            infoHtml = infoHtml.trimmed();
            if (!infoHtml.isEmpty()) {
                baseInfoList.append(infoHtml);
            }
        }

        // 调试日志：确认baseInfoList内容（保留）
        emit appendLogSignal(QString("📝 基础信息列表长度：%1").arg(baseInfoList.size()));
        for (int i = 0; i < baseInfoList.size(); i++) {
            emit appendLogSignal(QString("📝 索引%1：%2").arg(i).arg(baseInfoList.at(i)));
        }

        // 定义朝向关键字列表
        QStringList dirWords = {"东南", "西南", "东北", "西北", "南", "北", "东", "西"};

        // ========== 关键修改2：改用带索引的for循环，获取元素索引值 + 成功提取数据 ==========
        // 遍历baseInfoList，同时获取索引i和对应info（解决“获取索引值”需求）
        for (int i = 0; i < baseInfoList.size(); i++) {
            QString info = baseInfoList.at(i); // 当前元素
            int currentIndex = i; // 当前元素的索引（你需要的索引值）

            // 1. 提取面积（兼容数字与㎡之间有空格，同时打印索引）
            if (area.isEmpty()) {
                QRegularExpression areaFormatRegex(R"(^\d+(\.\d+)?\s*㎡$)");
                QRegularExpressionMatch areaMatch = areaFormatRegex.match(info);
                if (areaMatch.hasMatch()) {
                    area = info;
                    // 打印面积对应的索引值，满足你的需求
                    emit appendLogSignal(QString("✅ 面积提取成功：%1 | 对应索引：%2").arg(area).arg(currentIndex));
                    continue;
                }
            }

            // 2. 提取朝向（兼容组合朝向，同时打印索引）
            if (orientation.isEmpty()) {
                bool isDirection = false;
                foreach (QString dir, dirWords) {
                    if (info.contains(dir)) {
                        isDirection = true;
                        orientation = info;
                        break;
                    }
                }
                if (isDirection) {
                    // 打印朝向对应的索引值，满足你的需求
                    emit appendLogSignal(QString("✅ 朝向提取成功：%1 | 对应索引：%2").arg(orientation).arg(currentIndex));
                    continue;
                }
            }

            // 3. 提取建造年份（兼容空格，同时打印索引）
            if (buildingYear.isEmpty()) {
                QRegularExpression yearRegex(R"(^\d{4}\s*年建造$)");
                QRegularExpressionMatch yearMatch = yearRegex.match(info);
                if (yearMatch.hasMatch()) {
                    buildingYear = info;
                    // 打印年份对应的索引值，满足你的需求
                    emit appendLogSignal(QString("✅ 建造年份提取成功：%1 | 对应索引：%2").arg(buildingYear).arg(currentIndex));
                    continue;
                }
            }

            // 4. 提取楼层（同时打印索引）
            if (floor.isEmpty()) {
                QRegularExpression floorRegex(R"(\层)");
                QRegularExpressionMatch floorMatch = floorRegex.match(info);
                if (floorMatch.hasMatch()) {
                    floor = info;
                    // 打印楼层对应的索引值，满足你的需求
                    emit appendLogSignal(QString("✅ 楼层提取成功：%1 | 对应索引：%2").arg(floor).arg(currentIndex));
                    continue;
                }
            }
        }

        // ========== 关键修改3：提取完成后，给空变量赋值“未知”（兜底） ==========
        if (area.isEmpty()) {
            area = "未知";
        }
        if (orientation.isEmpty()) {
            orientation = "未知";
        }
        if (floor.isEmpty()) {
            floor = "未知";
        }
        if (buildingYear.isEmpty()) {
            buildingYear = "未知";
        }

        // 原有提示逻辑（可保留，也可删除，不影响核心功能）
        if (baseInfoList.size() >= 1) {
            if (area == "未知") {
                emit appendLogSignal(QString("⚠️  第1个基础信息非面积格式：%1").arg(baseInfoList.at(0)));
            }
        } else {
            emit appendLogSignal(QString("⚠️  基础信息列表为空，无法提取面积"));
        }

        if (baseInfoList.size() >= 2) {
            if (orientation == "未知") {
                emit appendLogSignal(QString("⚠️  第2个基础信息非朝向格式：%1").arg(baseInfoList.at(1)));
            }
        } else if (baseInfoList.size() >= 1) {
            emit appendLogSignal(QString("⚠️  基础信息列表长度不足2条，无法提取朝向"));
        }

        if (baseInfoList.size() >= 3) {
            if (floor == "未知") {
                emit appendLogSignal(QString("⚠️  第3个基础信息非楼层格式：%1").arg(baseInfoList.at(2)));
            }
            if (buildingYear == "未知") {
                if (baseInfoList.size() >= 4) {
                    emit appendLogSignal(QString("⚠️  第4个基础信息非建造年份格式：%1").arg(baseInfoList.at(3)));
                } else {
                    emit appendLogSignal(QString("⚠️  基础信息列表长度不足4条，无法提取建造年份"));
                }
            }
        } else {
            emit appendLogSignal(QString("⚠️  基础信息列表长度不足3条，无法提取楼层/建造年份"));
        }

        // ========== 链接提取（原有逻辑，保留） ==========
        QRegularExpression urlRegex(
            R"(<a\s+[^>]*?href=["']([^"']+)["'][^>]*>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );

        QRegularExpressionMatch urlMatch = urlRegex.match(houseHtml);
        if (urlMatch.hasMatch()) {
            houseUrl = urlMatch.captured(1).trimmed();
            if (!houseUrl.startsWith("http")) {
                houseUrl = "https://beijing.anjuke.com" + houseUrl;
            }
            emit appendLogSignal(QString("✅ 提取链接成功：%1").arg(houseUrl));
        } else {
            emit appendLogSignal("❌ 链接提取失败）");
        }

        // ========== 过滤无效房源并存储 ==========
        if (!title.isEmpty() && title != "未知" && !houseUrl.isEmpty() && !houseIdSet.contains(houseUrl)) {
            houseIdSet.insert(houseUrl);
            HouseData data;
            data.city = currentCity;
            data.houseTitle = title;
            data.communityName = communityName;
            data.price = totalPrice;
            data.unitPrice = unitPrice;
            data.houseType = houseType;
            data.area = area;
            data.orientation = orientation;
            data.floor = floor;
            data.buildingYear = buildingYear;
            data.houseUrl = houseUrl;
            houseDataList.append(data);
            extractCount++;

            emit appendLogSignal(QString("🎉 最终提取成功：小区=%1 | 总价=%2 | 户型=%3 | 面积=%4 | 朝向=%5")
                                     .arg(communityName, totalPrice, houseType, area, orientation));
        } else {
            emit appendLogSignal("🚫 该房源无有效信息，已过滤");
        }
    }

    emit appendLogSignal(QString("\n📊 提取完成：共%1条有效房源").arg(extractCount));
}

// 处理安居客房源页URL
void Crawl::processSearchUrl()
{
    if (searchUrlQueue.isEmpty()) {
        if (currentPageCount >= targetPageCount) {
            showHouseCompareResult();
        }
        return;
    }

    QString currentSearchUrl = searchUrlQueue.dequeue();
    emit appendLogSignal("\n📌 正在加载房源页：" + currentSearchUrl);

    QUrl url(currentSearchUrl);
    QWebEngineHttpRequest request(url);
    QString randomUA = getRandomUA();
    request.setHeader(QByteArray("User-Agent"), randomUA.toUtf8());
    request.setHeader(QByteArray("Referer"), QByteArray("https://www.anjuke.com/"));
    request.setHeader(QByteArray("Accept"), QByteArray("text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"));
    request.setHeader(QByteArray("Accept-Encoding"), QByteArray("gzip, deflate, br"));
    request.setHeader(QByteArray("Accept-Language"), QByteArray("zh-CN,zh;q=0.9,en;q=0.8"));
    request.setHeader(QByteArray("Cache-Control"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Connection"), QByteArray("keep-alive"));
    request.setHeader(QByteArray("Pragma"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Sec-Ch-Ua"), QByteArray("\"Chromium\";v=\"138\", \"Not=A?Brand\";v=\"8\", \"Google Chrome\";v=\"138\""));
    request.setHeader(QByteArray("Sec-Ch-Ua-Mobile"), QByteArray("?0"));
    request.setHeader(QByteArray("Sec-Ch-Ua-Platform"), QByteArray("\"Windows\""));
    request.setHeader(QByteArray("Sec-Fetch-Dest"), QByteArray("document"));
    request.setHeader(QByteArray("Sec-Fetch-Mode"), QByteArray("navigate"));
    request.setHeader(QByteArray("Sec-Fetch-Site"), QByteArray("same-origin"));
    request.setHeader(QByteArray("Upgrade-Insecure-Requests"), QByteArray("1"));

    if (!cookieStr.isEmpty()) {
        request.setHeader(QByteArray("Cookie"), cookieStr.toUtf8());
        emit appendLogSignal("🍪 本次请求携带Cookie（前50字符）: " + cookieStr.left(50) + "...");
    } else {
        emit appendLogSignal("⚠️ 无有效Cookie，可能触发风控！");
    }

    webPage->load(request);
}

// 展示房源对比结果（不变）
void Crawl::showHouseCompareResult()
{
    emit appendLogSignal("\n" + QString("=").repeated(60));
    emit appendLogSignal("=== " + currentCity + "二手房房源对比结果（共" + QString::number(houseDataList.size()) + "条有效房源）===");
    emit appendLogSignal(QString("=").repeated(60));

    std::sort(houseDataList.begin(), houseDataList.end(), [](const HouseData& a, const HouseData& b) {
        QString priceA = a.price;
        QString priceB = b.price;
        priceA.remove("万").remove(",").trimmed();
        priceB.remove("万").remove(",").trimmed();
        return priceA.toDouble() < priceB.toDouble();
    });

    for (int i = 0; i < houseDataList.size(); i++) {
        HouseData data = houseDataList[i];
        emit appendLogSignal("\n【" + QString::number(i + 1) + "】");
        emit appendLogSignal("🏠 房源标题：" + data.houseTitle);
        emit appendLogSignal("🏘️  小区名称：" + data.communityName);
        emit appendLogSignal("💰 总价：" + data.price + " | 单价：" + data.unitPrice);
        emit appendLogSignal("📐 户型/面积：" + data.houseType + " / " + data.area);

        QString floorAndOri = "";
        if (data.floor != "未知" && data.orientation != "未知") {
            floorAndOri = data.floor + " " + data.orientation;
        } else if (data.floor != "未知") {
            floorAndOri = data.floor;
        } else if (data.orientation != "未知") {
            floorAndOri = data.orientation;
        } else {
            floorAndOri = "未知";
        }
        emit appendLogSignal("🧭 楼层和朝向：" + floorAndOri);

        emit appendLogSignal("🎨 装修/年代：" + data.decoration + " / " + data.buildingYear);
        emit appendLogSignal("🔗 房源链接：" + data.houseUrl);
        emit appendLogSignal("-" + QString("-").repeated(58));
    }

    if (!houseDataList.isEmpty()) {
        emit appendLogSignal("\n🔥 对比总结：");

        HouseData cheapest = houseDataList.first();
        emit appendLogSignal("✅ 总价最低房源：" + cheapest.communityName + " - " + cheapest.houseType);
        emit appendLogSignal("   总价：" + cheapest.price + " | 单价：" + cheapest.unitPrice);

        double totalPriceSum = 0;
        int validPriceCount = 0;
        for (auto& house : houseDataList) {
            QString priceStr = house.price;
            priceStr.remove("万").remove(",").trimmed();
            bool ok;
            double price = priceStr.toDouble(&ok);
            if (ok) {
                totalPriceSum += price;
                validPriceCount++;
            }
            //mysql->insertInfo(house);
        }

        if (validPriceCount > 0) {
            double avgPrice = totalPriceSum / validPriceCount;
            emit appendLogSignal("✅ 房源均价：" + QString::number(avgPrice, 'f', 1) + " 万");
        }

        QMap<QString, int> houseTypeCount;
        for (auto& house : houseDataList) {
            houseTypeCount[house.houseType]++;
        }
        emit appendLogSignal("✅ 户型分布：");
        for (auto it = houseTypeCount.begin(); it != houseTypeCount.end(); it++) {
            emit appendLogSignal("   " + it.key() + "：" + QString::number(it.value()) + "套");
        }
    }

    emit appendLogSignal("\n=== 房源对比完成（低风控模式）===");
}

// 启动安居客爬取（核心修改：URL适配安居客）
void Crawl::startHouseCrawl(const QString& cityWithDistrict, int targetPages)
{
    // ========== 1. 拆分城市/区县 ==========
    QStringList cityDistrictParts = cityWithDistrict.split("-", Qt::SkipEmptyParts);
    QString pureCityName = cityDistrictParts.size() >= 1 ? cityDistrictParts[0].trimmed() : "";
    QString districtName = cityDistrictParts.size() >= 2 ? cityDistrictParts[1].trimmed() : "";

    // 基础校验
    if (pureCityName.isEmpty()) {
        emit appendLogSignal("❌ 输入格式错误！请输入：城市名 或 城市名-区县名（示例：北京 或 北京-朝阳区）");
        return;
    }

    // ========== 2. 页码风控限制 ==========
    targetPageCount = qBound(1, targetPages, 5);
    if (targetPageCount != targetPages) {
        emit appendLogSignal(QString("⚠️  风控调整：页码%1超出范围（1-5），自动修正为%2").arg(targetPages).arg(targetPageCount));
    }

    // ========== 3. 清空历史数据 ==========
    searchUrlQueue.clear();
    houseDataList.clear();
    houseIdSet.clear();
    currentPageCount = 0;
    isProcessingSearchTask = true;

    // ========== 4. 获取城市/区域拼音 ==========
    QString cityPinyin = regionToCode(pureCityName, "");
    QString districtPinyin = regionToCode(pureCityName, districtName);
    if (cityPinyin.isEmpty()) {
        emit appendLogSignal("❌ 无法获取「" + pureCityName + "」的城市拼音，终止爬取！");
        isProcessingSearchTask = false;
        return;
    }

    // ========== 5. 生成安居客URL（核心修改：https://bj.anjuke.com/sale/pg1/） ==========
    QString houseUrl;
    if (!districtName.isEmpty() && !districtPinyin.isEmpty()) {
        // 区级URL：https://bj.anjuke.com/sale/chaoyang/pg1/
        houseUrl = QString("https://%1.anjuke.com/sale/%2/p%3/")
                       .arg(cityPinyin)
                       .arg(districtPinyin)
                       .arg(targetPageCount);
    } else {
        // 城市级URL：https://bj.anjuke.com/sale/pg1/
        houseUrl = QString("https://%1.anjuke.com/sale/p%2/")
                       .arg(cityPinyin)
                       .arg(targetPageCount);
    }

    // ========== 6. 日志输出 + 入队URL ==========
    QString crawlScope = districtName.isEmpty() ? pureCityName : QString("%1-%2").arg(pureCityName, districtName);
    emit appendLogSignal("=== 低风控模式：爬取「" + crawlScope + "」二手房房源（第" + QString::number(targetPageCount) + "页）===");
    emit appendLogSignal("⚠️  风控提醒：单次只爬1页，目标页码范围1-5！");
    emit appendLogSignal("⚠️  请确保ke_cookies.txt中的Cookie是安居客登录后最新抓取的！");
    emit appendLogSignal("————————————————");
    emit appendLogSignal("🏙️  拼音映射：" + crawlScope + " → 城市拼音：" + cityPinyin + (districtPinyin.isEmpty() ? "" : " | 区域拼音：" + districtPinyin));
    emit appendLogSignal("📌 待爬取房源页：" + houseUrl);

    searchUrlQueue.enqueue(houseUrl);

    // ========== 7. 访问安居客首页建立会话 ==========
    emit appendLogSignal("🏠 第一步：先访问安居客首页建立会话...");
    QString homeUrl = "https://www.anjuke.com/"; // 安居客首页
    QWebEngineHttpRequest homeRequest(homeUrl);
    QString homeUA = getRandomUA();

    // 设置请求头
    homeRequest.setHeader(QByteArray("User-Agent"), homeUA.toUtf8());
    homeRequest.setHeader(QByteArray("Referer"), QByteArray(""));
    homeRequest.setHeader(QByteArray("Accept"), QByteArray("text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"));
    homeRequest.setHeader(QByteArray("Accept-Encoding"), QByteArray("gzip, deflate, br"));
    homeRequest.setHeader(QByteArray("Accept-Language"), QByteArray("zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6"));

    // 携带Cookie
    if (!cookieStr.isEmpty()) {
        homeRequest.setHeader(QByteArray("Cookie"), cookieStr.toUtf8());
    }

    // 启动首页加载
    webPage->load(homeRequest);
    isHomeLoadedForSearch = true;
    pendingSearchKeyword = crawlScope;
    currentCity = crawlScope;
}



