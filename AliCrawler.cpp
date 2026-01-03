#include "AliCrawl.h"
#include <QUrl>
#include <QTimer>
#include <QWebEngineSettings>
#include <QWebEngineHttpRequest>
#include <QRegularExpressionMatchIterator>
#include <QDateTime>
#include <QRandomGenerator>
#include <QByteArray>
#include <algorithm>
#include <QFile>
#include <QTextStream>
#include<QUrlQuery>

// 类内静态常量初始化（保持不变）
const int AliCrawl::REQUEST_INTERVAL = 3500;
const int AliCrawl::MAX_DEPTH = 1;
const int AliCrawl::MIN_REQUEST_INTERVAL = 9000;
const int AliCrawl::MAX_REQUEST_INTERVAL = 16000;
const QStringList AliCrawl::USER_AGENT_POOL = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36"
};

// 城市+行政区编码映射表
// 外层key=城市名，内层key=区名，value=区级编码
QMap<QString, QMap<QString, QString>> AliCrawl::getRegionCodeMap() {
    QMap<QString, QMap<QString, QString>> regionCodeMap;

    // 1. 北京（示例：核心城区编码）
    QMap<QString, QString> beijingRegions;
    beijingRegions["东城区"] = "110101";
    beijingRegions["西城区"] = "110102";
    beijingRegions["朝阳区"] = "110105";
    beijingRegions["海淀区"] = "110108";
    beijingRegions["丰台区"] = "110106";
    beijingRegions["石景山区"] = "110107";
    beijingRegions["通州区"] = "110112";
    beijingRegions["昌平区"] = "110114";
    regionCodeMap["北京"] = beijingRegions;

    // 2. 上海（示例：核心城区编码）
    QMap<QString, QString> shanghaiRegions;
    shanghaiRegions["浦东新区"] = "310115";
    shanghaiRegions["黄浦区"] = "310101";
    shanghaiRegions["静安区"] = "310106";
    shanghaiRegions["徐汇区"] = "310104";
    shanghaiRegions["闵行区"] = "310112";
    shanghaiRegions["杨浦区"] = "310110";
    regionCodeMap["上海"] = shanghaiRegions;

    // 3. 广州（示例：核心城区编码）
    QMap<QString, QString> guangzhouRegions;
    guangzhouRegions["天河区"] = "440106";
    guangzhouRegions["越秀区"] = "440104";
    guangzhouRegions["海珠区"] = "440105";
    guangzhouRegions["番禺区"] = "440113";
    guangzhouRegions["白云区"] = "440111";
    regionCodeMap["广州"] = guangzhouRegions;

    // 4. 杭州（示例：核心城区编码）
    QMap<QString, QString> hangzhouRegions;
    hangzhouRegions["西湖区"] = "330106";
    hangzhouRegions["滨江区"] = "330108";
    hangzhouRegions["余杭区"] = "330110";
    hangzhouRegions["萧山区"] = "330109";
    hangzhouRegions["拱墅区"] = "330105";
    regionCodeMap["杭州"] = hangzhouRegions;

    // 可继续扩展其他城市的区级编码
    return regionCodeMap;
}

//城市/区县转编码
QString AliCrawl::regionToCode(const QString& cityName, const QString& districtName) {
    QMap<QString, QMap<QString, QString>> regionCodeMap = getRegionCodeMap();

    // 如果传入了区名，优先匹配区级编码
    if (!districtName.isEmpty()) {
        if (regionCodeMap.contains(cityName) && regionCodeMap[cityName].contains(districtName)) {
            return regionCodeMap[cityName][districtName]; // 返回区级编码（如北京朝阳区→110105）
        }
        emit appendLogSignal(QString("⚠️ 未支持「%1-%2」的区级编码，请手动添加到regionCodeMap！").arg(cityName, districtName));
        return "";
    }

    //返回城市级编码
    QMap<QString, QString> cityCodeMap = {
        {"北京", "110000"}, {"上海", "310000"}, {"广州", "440100"},
        {"深圳", "440300"}, {"杭州", "330100"}, {"南京", "320100"},
        {"成都", "510100"}, {"重庆", "500000"}, {"武汉", "420100"},
        {"西安", "610100"}, {"天津", "120000"}, {"苏州", "320500"}
    };
    if (cityCodeMap.contains(cityName)) {
        return cityCodeMap[cityName];
    }

    emit appendLogSignal(QString("⚠️ 未支持「%1」的城市编码，请手动添加！").arg(cityName));
    return "";
}

QString AliCrawl::getFirstLetter(int index) {
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
void AliCrawl::simulateHumanBehavior() {
    QStringList jsScrolls = {
        QString("window.scrollTo(0, %1);").arg(QRandomGenerator::global()->bounded(300, 500)),
        QString("window.scrollTo(0, %1);").arg(QRandomGenerator::global()->bounded(800, 1200)),
        QString("window.scrollTo(0, document.body.scrollHeight * 0.8);"),
        QString("window.scrollTo(0, document.body.scrollHeight);")
    };

    int delay = 0;
    for (QString js : jsScrolls) {
        QTimer::singleShot(delay, this, [this, js]() {
            if (this == nullptr || webPage == nullptr) return;
            webPage->runJavaScript(js);
            emit appendLogSignal("🤖 模拟滚动：" + js);
        });
        delay += 2500;
    }

    int totalStayTime = 8000 + QRandomGenerator::global()->bounded(4000);
    emit appendLogSignal("🤖 模拟浏览停留：" + QString::number(totalStayTime/1000) + "秒");
}

//Cookie管理
void AliCrawl::loadCookiesFromFile(const QString& filePath) {
    QFile file(filePath.isEmpty() ? "ali_cookies.txt" : filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit appendLogSignal(QString("⚠️ 阿里Cookie加载失败：%1（使用默认）").arg(file.fileName()));
        cookieStr = "aliyungf_tc=xxx; x5sec=xxx; cna=xxx; isg=xxx; l=xxx; tfstk=xxx; cookie2=xxx; t=xxx";
        return;
    }

    QTextStream in(&file);
    cookieStr = in.readAll().trimmed();
    file.close();
    emit appendLogSignal(QString("✅ 阿里Cookie加载成功（前50字符）：%1...").arg(cookieStr.left(50)));
}

void AliCrawl::saveCookiesToFile(const QString& filePath) {
    QString savePath = filePath.isEmpty() ? "ali_cookies.txt" : filePath;
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit appendLogSignal(QString("⚠️ 阿里Cookie保存失败：%1").arg(savePath));
        return;
    }

    QTextStream out(&file);
    out << cookieStr;
    file.close();
    emit appendLogSignal(QString("✅ 阿里Cookie保存至：%1").arg(savePath));
}

// 工具函数
QString AliCrawl::generateRandomPvid() {
    const QString chars = "0123456789abcdefghijklmnopqrstuvwxyz";
    QString pvid;
    QRandomGenerator* gen = QRandomGenerator::global();
    pvid += "ali_";
    for (int i = 0; i < 28; i++) {
        pvid += chars.at(gen->bounded(chars.length()));
    }
    return pvid;
}

QString AliCrawl::generateLogId() {
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    int random = QRandomGenerator::global()->bounded(10000, 99999);
    return QString("log_%1_%2").arg(timestamp).arg(random);
}

QString AliCrawl::getRandomUA() {
    int index = QRandomGenerator::global()->bounded(USER_AGENT_POOL.size());
    return USER_AGENT_POOL.at(index);
}

int AliCrawl::getRandomInterval() {
    return QRandomGenerator::global()->bounded(MIN_REQUEST_INTERVAL, MAX_REQUEST_INTERVAL);
}

//构造函数
AliCrawl::AliCrawl(MainWindow *mainWindow, QWebEnginePage *webPageParam, Ui::MainWindow* ui)
    : QObject(nullptr)
    , webPage(nullptr)
    , m_ui(ui)
    , m_mainWindow(mainWindow)
    , isProcessingSearchTask(false)
    , currentPageCount(0)
    , targetPageCount(1)
    , isHomeLoadedForSearch(false)
{
    mysql = new Mysql();
    mysql->connectDatabase();

    if (webPageParam != nullptr) {
        webPage = webPageParam;
        webPage->setParent(this);
    } else {
        webPage = new QWebEnginePage(this);
    }

    connect(this, &AliCrawl::startCrawlSignal, this, &AliCrawl::startHouseCrawl, Qt::QueuedConnection);

    QWebEngineSettings* settings = webPage->settings();
    settings->setAttribute(QWebEngineSettings::AutoLoadIconsForPage, false);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);

    connect(webPage, &QWebEnginePage::loadFinished, this, &AliCrawl::onPageLoadFinished);
    loadCookiesFromFile();

    int delayMs = 1500 + QRandomGenerator::global()->bounded(2500);
    QTimer::singleShot(delayMs, this, &AliCrawl::onInitFinishedLog);
}

AliCrawl::~AliCrawl() {
    if (webPage != nullptr) {
        webPage->deleteLater();
        webPage = nullptr;
    }

    urlQueue.clear();
    searchUrlQueue.clear();
    crawledUrls.clear();
    urlDepth.clear();
    houseDataList.clear();
    houseIdSet.clear();
    mysql->close();

    emit appendLogSignal("🔌 阿里房产爬虫实例已销毁");
}

void AliCrawl::onInitFinishedLog() {
    emit appendLogSignal("✅ 阿里房产爬虫初始化完成");
    emit appendLogSignal("💡 使用说明：输入城市名+区名（可选），点击爬取按钮（支持：北京-朝阳区、上海-浦东新区等）");
}

// 处理普通URL
void AliCrawl::processNextUrl() {
    if (urlQueue.isEmpty()) {
        emit appendLogSignal("\n=== 阿里房产首页爬取完成 ===");
        return;
    }

    QString currentUrl = urlQueue.dequeue();
    emit appendLogSignal("\n📌 加载页面：" + currentUrl);

    QUrl reqUrl(currentUrl);
    QWebEngineHttpRequest request{reqUrl};

    QString randomUA = getRandomUA();
    request.setHeader(QByteArray("User-Agent"), randomUA.toUtf8());
    request.setHeader(QByteArray("Referer"), QByteArray("https://huodong.taobao.com/"));
    request.setHeader(QByteArray("Accept"), QByteArray("text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"));
    request.setHeader(QByteArray("Accept-Encoding"), QByteArray("gzip, deflate, br"));
    request.setHeader(QByteArray("Accept-Language"), QByteArray("zh-CN,zh;q=0.9,en;q=0.8"));
    request.setHeader(QByteArray("Cache-Control"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Connection"), QByteArray("keep-alive"));
    request.setHeader(QByteArray("Pragma"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Sec-Fetch-Dest"), QByteArray("document"));
    request.setHeader(QByteArray("Sec-Fetch-Mode"), QByteArray("navigate"));
    request.setHeader(QByteArray("Sec-Fetch-Site"), QByteArray("same-origin"));

    if (!cookieStr.isEmpty()) {
        request.setHeader(QByteArray("Cookie"), cookieStr.toUtf8());
    } else {
        emit appendLogSignal("⚠️ 无阿里Cookie，可能触发风控！");
    }

    webPage->load(request);
}

// 页面加载完成
void AliCrawl::onPageLoadFinished(bool ok) {
    if (this == nullptr || webPage == nullptr) return;

    QString currentUrl = webPage->url().toString();
    bool isSearchTask = isProcessingSearchTask;

    bool isRiskPage = currentUrl.contains("safe.ali.com") ||
                      currentUrl.contains("verify") ||
                      currentUrl.contains("security") ||
                      currentUrl.contains("captcha") ||
                      currentUrl.contains("antispam");
    if (isRiskPage) {
        emit appendLogSignal("❌ 触发阿里风控：" + currentUrl);
        emit appendLogSignal("💡 解决方案：1.更新ali_cookies.txt 2.降低爬取频率 3.更换IP");
        searchUrlQueue.clear();
        isProcessingSearchTask = false;
        return;
    }

    if (!ok) {
        emit appendLogSignal("❌ 加载失败：" + currentUrl);
        if (isSearchTask) {
            QTimer::singleShot(16000, this, &AliCrawl::processSearchUrl);
        } else {
            QTimer::singleShot(9000, this, &AliCrawl::processNextUrl);
        }
        return;
    }

    emit appendLogSignal("✅ 页面加载成功：" + currentUrl);
    simulateHumanBehavior();

    int renderDelay = currentUrl.contains("ershoufang") || currentUrl.contains("pm/default/pc/4b05fb")
                          ? 22000 + QRandomGenerator::global()->bounded(8000)
                          : 5000 + QRandomGenerator::global()->bounded(3000);
    if (currentUrl.contains("pm/default/pc/4b05fb")) {
        emit appendLogSignal("⏳ 等待房源渲染：" + QString::number(renderDelay/1000) + "秒");
    }

    QTimer::singleShot(renderDelay, this, [this, currentUrl, isSearchTask]() {
        if (this == nullptr || webPage == nullptr) return;

        webPage->toHtml([this, currentUrl, isSearchTask](const QString& html) {
            bool hasHouseNode = html.contains("div class=\"house-item\"") ||
                                html.contains("div class=\"item-wrap\"") ||
                                html.contains("div class=\"property-item\"");
            emit appendLogSignal(QString("📋 HTML包含房源节点：%1").arg(hasHouseNode ? "是" : "否"));

            if (isSearchTask && currentUrl.contains("pm/default/pc/4b05fb")) {
                extractHouseData(html);
                currentPageCount++;

                isProcessingSearchTask = false;
                QString nextLog = QString("✅ 第%1页爬取完成，准备显示结果...").arg(targetPageCount);
                QTimer::singleShot(1000, this, &AliCrawl::showHouseCompareResult);
                emit appendLogSignal(nextLog);
            } else if (!isSearchTask) {
                extractAliData(html, currentUrl);
                QTimer::singleShot(9000, this, &AliCrawl::processNextUrl);
            }
        });
    });
}

// 解析普通页面
void AliCrawl::extractAliData(const QString& html, const QString& currentUrl) {
    emit appendLogSignal("🔍 解析阿里页面...");

    QRegularExpression cityRegex(R"(<a\s+href=["'](https?://huodong.taobao.com/[^"']*)["']\s+class=["']city-item["'].*?>([\s\S]*?)</a>)",
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator cityIt = cityRegex.globalMatch(html);
    while (cityIt.hasNext()) {
        QRegularExpressionMatch match = cityIt.next();
        QString cityUrl = match.captured(1).trimmed();
        QString cityName = match.captured(2).trimmed();
        cityName.remove(QRegularExpression("<[^>]*>"));

        if (!crawledUrls.contains(cityUrl) && urlDepth[currentUrl] < MAX_DEPTH) {
            crawledUrls.insert(cityUrl);
            urlQueue.enqueue(cityUrl);
            urlDepth[cityUrl] = urlDepth[currentUrl] + 1;
            emit appendLogSignal("🏙️ 城市：" + cityName + " | 链接：" + cityUrl);
        }
    }

    emit appendLogSignal("✅ 解析完成：" + currentUrl);
}

// 提取房源数据（
void AliCrawl::extractHouseData(const QString& html)
{
    emit appendLogSignal("🔍 开始提取阿里二手房房源数据...");

    // houseRegex：去掉价格锚点（?:起拍价|当前价）
    QRegularExpression houseRegex(
        R"(<div\s+[^>]*?>[\s\S]*?)"
        R"(<span\s+class=["']text["']\s+numberoflines=["']2["'])"  // 稳定锚点1：标题span
        R"([\s\S]{0,2000}?)"  // 宽泛匹配：标题span → 24px价格span（无需关心价格关键词）
        R"(<span\s+class=["']text["'].*?font-size:\s*24px)"  // 稳定锚点2：24px价格span
        R"([\s\S]*?</div>)",
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption
        );


    QRegularExpressionMatchIterator houseIt = houseRegex.globalMatch(html);
    int extractCount = 0;
    int totalBlockCount = 0;

    QRegularExpressionMatchIterator countIt = houseRegex.globalMatch(html);
    while (countIt.hasNext()) {
        countIt.next();
        totalBlockCount++;
    }
    emit appendLogSignal(QString("📋 共识别到%1个房源容器").arg(totalBlockCount));

    bool hasTitleSpan = html.contains(QRegularExpression(R"(<span\s+class=["']text["']\s+numberoflines=["']2["'])"));
    bool hasCurrentPrice = html.contains("当前价");
    bool has24pxPrice = html.contains(QRegularExpression(R"(<span\s+class=["']text["'].*?font-size:\s*24px)"));
    emit appendLogSignal(QString("📌 调试：HTML包含标题span=%1，包含当前价=%2，包含24px价格span=%3")
                             .arg(hasTitleSpan ? "是" : "否")
                             .arg(hasCurrentPrice ? "是" : "否")
                             .arg(has24pxPrice ? "是" : "否"));

    if (totalBlockCount == 0 && hasTitleSpan && hasCurrentPrice && has24pxPrice) {
        emit appendLogSignal("⚠️  警告：HTML包含核心锚点，但未匹配到房源容器，可能是正则范围过窄");
    } else if (totalBlockCount == 0) {
        emit appendLogSignal("⚠️  警告：HTML未包含核心锚点，可能是页面加载失败或HTML结构变化");
        emit appendLogSignal(QString("📌 调试：HTML长度=%1字符").arg(html.length()));
        return;
    }

    while (houseIt.hasNext()) {
        QRegularExpressionMatch houseMatch = houseIt.next();
        QString houseHtml = houseMatch.captured(0).trimmed();
        //状态获取
        QRegularExpression endFlagRegex(
            R"(<div[^>]*?>[\s\S]*?已结束[\s\S]*?<span\s+class=["']text["'][\s\S]*?</span>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption
            );
        QRegularExpressionMatch endMatch = endFlagRegex.match(houseHtml);
        if (endMatch.hasMatch()) {
            emit appendLogSignal("🚫 识别到“已结束”标识，标记为无效房源，跳过处理");
            continue; // 跳过当前房源，处理下一个
        }

        extractCount++;
        emit appendLogSignal(QString("\n=================================================="));
        emit appendLogSignal(QString("🏠 正在处理第%1个房源").arg(extractCount));
        emit appendLogSignal(QString("=================================================="));

        QString title = "未知";
        QString communityName = "未知";
        QString totalPrice = "未知";
        QString evalPrice = "未知";
        QString unitPrice = "未知";
        QString houseType = "未知";
        QString area = "未知";
        QString orientation = "未知";
        QString floor = "未知";
        QString buildingYear = "未知";
        QString houseUrl = "未知";
        QString city = "未知";
        QString region = "未知";
        QString location = "未知";

        QRegularExpression titleRegex(
            R"(<span\s+class=["']text["']\s+numberoflines=["']2["']\s+title=["']([^"']+)["'])",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch titleMatch = titleRegex.match(houseHtml);
        if (titleMatch.hasMatch()) {
            title = titleMatch.captured(1).trimmed();
            emit appendLogSignal(QString("✅ 房源标题：%1").arg(title));
        } else {
            QRegularExpression titleTextRegex(
                R"(<span\s+class=["']text["']\s+numberoflines=["']2["'].*?>([\s\S]*?)</span>)",
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
                );
            QRegularExpressionMatch titleTextMatch = titleTextRegex.match(houseHtml);
            if (titleTextMatch.hasMatch()) {
                title = titleTextMatch.captured(1).trimmed();
                title.remove(QRegularExpression("<[^>]*>"));
                title.replace(QRegularExpression("\\s+"), " ");
                emit appendLogSignal(QString("✅ 兜底提取标题：%1").arg(title));
            } else {
                emit appendLogSignal("❌ 标题提取失败");
            }
        }

        QStringList nonHouseKeywords = {
            "车位", "车库", "商用", "店面", "门市",
            "写字楼", "办公", "厂房", "仓库", "工业",
            "公寓式办公", "商办", "商住两用", "摊位", "柜台","储藏间",
            "广场","商场"
        };

        bool isNonHouse = false;
        foreach (const QString& keyword, nonHouseKeywords) {
            if (title.contains(keyword, Qt::CaseInsensitive)) {
                isNonHouse = true;
                break;
            }
        }
        if (isNonHouse) {
            emit appendLogSignal(QString("🚫 过滤非住宅房源：标题包含关键词（%1），跳过处理").arg(title));
            continue;
        }

        QRegularExpression baseInfoRegex(
            R"(<span\s+class=["']text["']\s+numberoflines=["']1["'].*?>([\s\S]*?)</span>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch baseInfoMatch = baseInfoRegex.match(houseHtml);
        if (baseInfoMatch.hasMatch()) {
            QString baseText = baseInfoMatch.captured(1).trimmed();
            baseText.remove(QRegularExpression("<[^>]*>"));
            baseText.replace(QRegularExpression("\\s+"), " ");
            emit appendLogSignal(QString("📋 基础信息原始文本：%1").arg(baseText));

            QStringList baseList = baseText.split("|", Qt::SkipEmptyParts);
            QList<bool> isItemMatched(baseList.size(), false);
            for (int i = 0; i < baseList.size(); ++i) {
                baseList[i] = baseList[i].trimmed();
            }

            // 优先按固定索引提取小区名
            if (!baseList.isEmpty()) {
                QString firstItem = baseList[0];
                if (!firstItem.isEmpty()) {
                    communityName = firstItem;
                    emit appendLogSignal(QString("✅ 小区名（优先索引提取）：%1").arg(communityName));
                    isItemMatched[0] = true; // 标记为已匹配
                }
            }

            // 优先按固定索引提取面积
            int listSize = baseList.size();

            for (int i = 1; i <= (listSize >= 3 ? listSize - 3 : listSize - 1); ++i) {
                QString item = baseList[i];
                if (isItemMatched[i] || item.isEmpty()) continue;

                bool isArea = item.contains(QRegularExpression("\\d+(\\.\\d+)?")) && (item.contains("㎡") || item.contains("m²"));
                if (isArea) {
                    QRegularExpression areaNumRegex(R"(\d+(\.\d+)?)");
                    QRegularExpressionMatch areaNumMatch = areaNumRegex.match(item);
                    area = areaNumMatch.hasMatch() ? areaNumMatch.captured(0) + " ㎡" : item.replace("m²", "㎡");
                    emit appendLogSignal(QString("✅ 面积（中间元素匹配）：%1").arg(area));
                    isItemMatched[i] = true;
                    break;
                }
            }

            // 优先按固定索引提取房型
            QRegularExpression houseTypeRegex("^(?:(\\d+|多)室)?(?:(\\d+|多)厅)(?:(\\d+|多)卫)?$",
                                              QRegularExpression::CaseInsensitiveOption);
            // 遍历中间元素匹配户型
            for (int i = 1; i <= (listSize >= 3 ? listSize - 3 : listSize - 1); ++i) {
                QString item = baseList[i];
                if (isItemMatched[i] || item.isEmpty()) {
                    continue;
                }

                // 使用优化后的正则匹配户型
                if (houseTypeRegex.match(item).hasMatch()) {
                    houseType = item;
                    emit appendLogSignal(QString("✅ 户型（中间元素匹配）：%1").arg(houseType));
                    isItemMatched[i] = true;
                    break;
                }
            }

            // 优先按固定索引提取区域信息
            if (listSize >= 2) {
                // 倒数第二个元素：优先作为城市
                QString penultimateItem = baseList[listSize - 2];
                if (!penultimateItem.isEmpty() && city == "未知") {
                    city = penultimateItem;
                    emit appendLogSignal(QString("✅ 城市（倒数第二个元素提取）：%1").arg(city));
                    isItemMatched[listSize - 2] = true;
                }
                // 倒数第一个元素：优先作为区域
                QString lastItem = baseList[listSize - 1];
                if (!lastItem.isEmpty() && region == "未知") {
                    region = lastItem;
                    emit appendLogSignal(QString("✅ 区域（倒数第一个元素提取）：%1").arg(region));
                    isItemMatched[listSize - 1] = true;
                }
            }


            // 小区名兜底
            if (communityName == "未知" || communityName.isEmpty()) {
                QList<QString> communityCandidates;
                QRegularExpression hasChineseRegex("\\p{Script=Han}+", QRegularExpression::CaseInsensitiveOption);
                for (int i = 0; i < baseList.size(); ++i) {
                    QString item = baseList[i];
                    if (!isItemMatched[i] && !item.isEmpty() && hasChineseRegex.match(item).hasMatch()) {
                        if (!item.contains("室") && !item.contains("厅") && !item.contains("卫")) {
                            communityCandidates.append(item);
                        }
                    }
                }
                if (!communityCandidates.isEmpty()) {
                    QString longestCommunity = communityCandidates.first();
                    foreach (QString candidate, communityCandidates) {
                        if (candidate.length() > longestCommunity.length()) {
                            longestCommunity = candidate;
                        }
                    }
                    communityName = longestCommunity;
                    emit appendLogSignal(QString("✅ 小区名（兜底提取）：%1（长度：%2字）").arg(communityName).arg(communityName.length()));
                } else {
                    communityName = "未知";
                    emit appendLogSignal("✅ 小区名（兜底提取）：未知（无有效候选项）");
                }
            }

            //  城市/区域兜底
            if (city == "未知" || region == "未知") {
                QRegularExpression pureChineseRegex("^\\p{Script=Han}+$", QRegularExpression::CaseInsensitiveOption);
                for (int i = 0; i < baseList.size(); ++i) {
                    QString item = baseList[i];
                    if (isItemMatched[i] || item.isEmpty()) continue;

                    if (pureChineseRegex.match(item).hasMatch()) {
                        if (city == "未知" && item.length() <= 4) {
                            city = item;
                            emit appendLogSignal(QString("✅ 城市（兜底提取）：%1").arg(city));
                            isItemMatched[i] = true;
                        } else if (region == "未知") {
                            region = item;
                            emit appendLogSignal(QString("✅ 区域（兜底提取）：%1").arg(region));
                            isItemMatched[i] = true;
                        }
                    }
                }
            }



            // 位置拼接
            location = city != "未知" && region != "未知" ? QString("%1市%2区").arg(city, region) :
                           city != "未知" ? QString("%1市").arg(city) : "未知";
            if (location != "未知") {
                emit appendLogSignal(QString("✅ 位置：%1").arg(location));
            }
        } else {
            emit appendLogSignal("❌ 未找到基础信息容器（numberoflines=1的text span）");
        }
        QRegularExpression totalPriceRegex(
            R"((?:当前价|起拍价|一口价)[\s\S]*?)"
            R"(<span\s+class=["']text["'].*?font-size:\s*24px.*?>(\s*[\d.]+)\s*</span>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch priceMatch = totalPriceRegex.match(houseHtml);
        if (priceMatch.hasMatch()) {
            QString priceNum = priceMatch.captured(1).trimmed();
            totalPrice = QString("%1 万").arg(priceNum);
            emit appendLogSignal(QString("✅ 总价：%1").arg(totalPrice));
        } else {
            emit appendLogSignal("❌ 总价提取失败（未找到24px价格数字）");
        }

        QRegularExpression evalPriceRegex(
            R"((?:评估价|市场价))"
            R"([\s\S]{0,300}?)"
            R"(<span\s+class=["']text["'].*?>(\d+(?:\.\d+)?)万</span>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch evalMatch = evalPriceRegex.match(houseHtml);
        if (evalMatch.hasMatch()) {
            QString priceType = evalMatch.captured(1).trimmed();
            QString evalNum = evalMatch.captured(2).trimmed();
            evalPrice = QString("%1 万").arg(evalNum);
            emit appendLogSignal(QString("✅ %1：%2").arg(priceType, evalPrice));
        } else {
            emit appendLogSignal("❌ 评估价/市场价提取失败");
        }

        QRegularExpression floorRegex(R"((\d+层))", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch floorMatch = floorRegex.match(title);
        if (floorMatch.hasMatch()) {
            floor = floorMatch.captured(1).trimmed();
            emit appendLogSignal(QString("✅ 楼层：%1").arg(floor));
        } else {
            emit appendLogSignal("❌ 楼层提取失败（标题中无明确楼层）");
        }

        QStringList dirWords = {"东南", "西南", "东北", "西北", "南", "北", "东", "西"};
        QString dirResult;
        foreach (const QString& dir, dirWords) {
            if (title.contains(dir) || (baseInfoMatch.hasMatch() && baseInfoMatch.captured(1).contains(dir))) {
                dirResult += dir + " ";
            }
        }
        orientation = dirResult.trimmed().isEmpty() ? "未知" : dirResult.trimmed();
        if (orientation != "未知") {
            emit appendLogSignal(QString("✅ 朝向：%1").arg(orientation));
        } else {
            emit appendLogSignal("❌ 朝向提取失败");
        }

        QRegularExpression urlRegex(
            R"(<a\s+[^>]*?href=["']([^"']+)["'].*?>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch urlMatch = urlRegex.match(houseHtml);
        if (urlMatch.hasMatch()) {
            houseUrl = urlMatch.captured(1).trimmed();
            if (houseUrl.startsWith("//")) {
                houseUrl = "https:" + houseUrl;
            } else if (!houseUrl.startsWith("http") && !houseUrl.isEmpty()) {
                houseUrl = "https://huodong.taobao.com" + houseUrl;
            }
            emit appendLogSignal(QString("✅ 房源链接：%1").arg(houseUrl));
        } else {
            emit appendLogSignal("❌ 房源链接提取失败");
        }

        if (totalPrice != "未知" && area != "未知") {
            QString priceStr = totalPrice.remove(" 万").trimmed();
            QString areaStr = area.remove(" ㎡").trimmed();
            bool priceOk = false, areaOk = false;
            double price = priceStr.toDouble(&priceOk);
            double areaVal = areaStr.toDouble(&areaOk);
            if (priceOk && areaOk && areaVal > 0) {
                unitPrice = QString("%1 元/㎡").arg(QString::number((price * 10000) / areaVal, 'f', 0));
                emit appendLogSignal(QString("✅ 单价（计算）：%1").arg(unitPrice));
            } else {
                unitPrice = "计算失败";
                emit appendLogSignal("❌ 单价计算失败");
            }
        } else {
            unitPrice = "计算失败";
            emit appendLogSignal("❌ 单价计算失败（总价或面积缺失）");
        }

        if (unitPrice!="计算失败" && !houseUrl.isEmpty() && !houseIdSet.contains(houseUrl)) {
            houseIdSet.insert(houseUrl);
            HouseInfo data;
            data.city = currentCity;
            data.houseTitle = title;
            data.communityName = communityName;
            data.price = totalPrice;
            data.evalPrice = evalPrice;
            data.unitPrice = unitPrice;
            data.houseType = houseType;
            data.area = area;
            data.orientation = orientation;
            data.floor = floor;
            data.buildingYear = buildingYear;
            data.houseUrl = houseUrl;
            data.region = region;
            data.decoration = "未知";
            data.location = location;
            data.rent = "未知";

            houseDataList.append(data);
            emit appendLogSignal(QString("🎉 房源存储成功：%1").arg(title));
        } else {
            if (houseIdSet.contains(houseUrl)) {
                emit appendLogSignal("⚠️  房源已重复，跳过存储");
            } else {
                emit appendLogSignal("⚠️  核心字段缺失，跳过存储");
            }
        }
    }

    emit appendLogSignal(QString("\n=================================================="));
    emit appendLogSignal(QString("📊 提取完成：共识别%1个房源容器，成功存储%2条有效房源").arg(totalBlockCount).arg(houseDataList.size()));
    emit appendLogSignal("==================================================\n");
}

//  处理搜索URL
void AliCrawl::processSearchUrl() {
    if (searchUrlQueue.isEmpty()) {
        if (currentPageCount >= targetPageCount) {
            showHouseCompareResult();
        }
        return;
    }

    QString currentSearchUrl = searchUrlQueue.dequeue();
    emit appendLogSignal("\n📌 加载房源页：" + currentSearchUrl);

    QUrl reqUrl(currentSearchUrl);
    QWebEngineHttpRequest request{reqUrl};
    QString randomUA = getRandomUA();
    request.setHeader(QByteArray("User-Agent"), randomUA.toUtf8());

    request.setHeader(QByteArray("Referer"), QByteArray("https://huodong.taobao.com/"));
    request.setHeader(QByteArray("Accept"), QByteArray("text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"));
    request.setHeader(QByteArray("Accept-Encoding"), QByteArray("gzip, deflate, br"));
    request.setHeader(QByteArray("Accept-Language"), QByteArray("zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6"));
    request.setHeader(QByteArray("Cache-Control"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Connection"), QByteArray("keep-alive"));
    request.setHeader(QByteArray("Sec-Ch-Ua"), QByteArray("\"Chromium\";v=\"139\", \"Not=A?Brand\";v=\"8\", \"Google Chrome\";v=\"139\""));
    request.setHeader(QByteArray("Sec-Ch-Ua-Mobile"), QByteArray("?0"));
    request.setHeader(QByteArray("Sec-Ch-Ua-Platform"), QByteArray("\"Windows\""));
    request.setHeader(QByteArray("Sec-Fetch-Dest"), QByteArray("document"));
    request.setHeader(QByteArray("Sec-Fetch-Mode"), QByteArray("navigate"));
    request.setHeader(QByteArray("Sec-Fetch-Site"), QByteArray("same-origin"));
    request.setHeader(QByteArray("Sec-Fetch-User"), QByteArray("?1"));
    request.setHeader(QByteArray("Upgrade-Insecure-Requests"), QByteArray("1"));

    if (!cookieStr.isEmpty()) {
        request.setHeader(QByteArray("Cookie"), cookieStr.toUtf8());
    }

    webPage->load(request);
}

//展示结果
void AliCrawl::showHouseCompareResult() {
    emit appendLogSignal("\n" + QString("=").repeated(80));
    emit appendLogSignal("=== " + currentCity + "阿里二手房对比结果（共" + QString::number(houseDataList.size()) + "条）===");
    emit appendLogSignal(QString("=").repeated(80));

    std::sort(houseDataList.begin(), houseDataList.end(), [](const HouseInfo& a, const HouseInfo& b) {
        auto parsePrice = [](const QString& priceStr) -> double {
            QString temp = priceStr;
            if (temp == "未知" || !temp.contains(QRegularExpression(R"(\d+)"))) {
                return 1e18;
            }
            temp.remove("万").remove(",").remove(" ").trimmed();
            bool ok;
            double price = temp.toDouble(&ok);
            return ok ? price : 1e18;
        };
        return parsePrice(a.price) < parsePrice(b.price);
    });

    for (int i = 0; i < houseDataList.size(); i++) {
        HouseInfo data = houseDataList[i];
        emit appendLogSignal("\n【" + QString::number(i + 1) + "】" + QString("-").repeated(75));
        emit appendLogSignal("🏠 标题：" + data.houseTitle);
        emit appendLogSignal("🌍 区域：" + data.region + " | 🏘️ 小区：" + data.communityName);
        emit appendLogSignal("💰 总价：" + data.price + " | 📊 评估价：" + (data.evalPrice.isEmpty() ? "待说明" : data.evalPrice));
        emit appendLogSignal("💵 单价：" + data.unitPrice + " | 🏠 年租：" + (data.rent == "未知" ? "无" : data.rent));
        emit appendLogSignal("📐 户型/面积：" + data.houseType + " / " + data.area);
        emit appendLogSignal("🧭 楼层/朝向：" + data.floor + " / " + data.orientation);
        emit appendLogSignal("🏗️ 年代：" + (data.buildingYear == "未知" ? "待补充" : data.buildingYear));
        emit appendLogSignal("🔗 链接：" + data.houseUrl);
        emit appendLogSignal("-" + QString("-").repeated(78));
    }

    if (!houseDataList.isEmpty()) {
        emit appendLogSignal("\n🔥 对比总结：");

        HouseInfo cheapest = houseDataList.first();
        if (cheapest.price != "未知") {
            emit appendLogSignal("✅ 最低价：" + cheapest.communityName + " - " + cheapest.price +
                                 "（区域：" + cheapest.region + " | 单价：" + cheapest.unitPrice + "）");
        } else {
            emit appendLogSignal("✅ 最低价：暂无有效价格房源");
        }

        double totalPrice = 0;
        int validPriceCount = 0;
        double totalUnitPrice = 0;
        int validUnitPriceCount = 0;

        for (auto& house : houseDataList) {
            QString priceStr = house.price;
            if (priceStr != "未知" && priceStr.contains(QRegularExpression(R"(\d+)"))) {
                priceStr.remove("万").remove(",").remove(" ").trimmed();
                bool ok;
                double price = priceStr.toDouble(&ok);
                if (ok) {
                    totalPrice += price;
                    validPriceCount++;
                }
            }

            QString unitPriceStr = house.unitPrice;
            if (unitPriceStr != "未知" && unitPriceStr != "计算失败" && unitPriceStr.contains(QRegularExpression(R"(\d+)"))) {
                unitPriceStr.remove("元/㎡").remove(",").remove(" ").trimmed();
                bool ok;
                double unitPrice = unitPriceStr.toDouble(&ok);
                if (ok) {
                    totalUnitPrice += unitPrice;
                    validUnitPriceCount++;
                }
            }
            //mysql->insertAlInfo(house); // 需启用时取消注释

        }

        if (validPriceCount > 0) {
            emit appendLogSignal("✅ 总价均价：" + QString::number(totalPrice / validPriceCount, 'f', 1) + " 万");
        } else {
            emit appendLogSignal("✅ 总价均价：暂无有效价格数据");
        }

        if (validUnitPriceCount > 0) {
            emit appendLogSignal("✅ 单价均价：" + QString::number(totalUnitPrice / validUnitPriceCount, 'f', 0) + " 元/㎡");
        } else {
            emit appendLogSignal("✅ 单价均价：暂无有效单价数据");
        }

        int rentHouseCount = std::count_if(houseDataList.begin(), houseDataList.end(), [](const HouseInfo& house) {
            return house.rent != "未知" && !house.rent.isEmpty();
        });
        if (rentHouseCount > 0) {
            emit appendLogSignal("✅ 带年租房源：" + QString::number(rentHouseCount) + " 条");
        }
    } else {
        emit appendLogSignal("\n⚠️  暂无有效房源数据");
    }

    emit appendLogSignal("\n" + QString("=").repeated(80));
    emit appendLogSignal("=== 阿里房源对比完成 ===");
    emit appendLogSignal(QString("=").repeated(80));
}

//用QUrlQuery构建URL
void AliCrawl::startHouseCrawl(const QString& cityWithDistrict, int targetPages) {
    // 拆分城市和区名（格式："北京-朝阳区" 或 "北京"）
    QStringList cityDistrict = cityWithDistrict.split("-", Qt::SkipEmptyParts);
    QString cityName = cityDistrict.size() >= 1 ? cityDistrict[0].trimmed() : "";
    QString districtName = cityDistrict.size() >= 2 ? cityDistrict[1].trimmed() : "";

    if (cityName.isEmpty()) {
        emit appendLogSignal("❌ 请输入城市名（格式：城市名 或 城市名-区名，如：北京-朝阳区）！");
        return;
    }

    currentCity = districtName.isEmpty() ? cityName : QString("%1-%2").arg(cityName, districtName);
    targetPageCount = qBound(1, targetPages, 5);
    emit appendLogSignal("=== 爬取「" + currentCity + "」阿里二手房（第" + QString::number(targetPageCount) + "页）===");

    // 清空旧数据
    searchUrlQueue.clear();
    houseDataList.clear();
    houseIdSet.clear();
    currentPageCount = 0;
    isProcessingSearchTask = true;

    // 获取区级/城市级编码
    QString locationCode = regionToCode(cityName, districtName);
    if (locationCode.isEmpty()) {
        emit appendLogSignal("❌ 编码获取失败，无法生成URL！");
        isProcessingSearchTask = false;
        return;
    }
    emit appendLogSignal("🏙️ 编码：" + currentCity + " → " + locationCode);

    // 生成随机参数
    QString pvid = generateRandomPvid();
    QString logId = generateLogId();


    QUrl baseUrl("https://huodong.taobao.com/wow/pm/default/pc/4b05fb");
    QUrlQuery query;

    QString keywordRaw = "二手房";
    query.addQueryItem("keyword", keywordRaw);


    query.addQueryItem("fcatV4Ids", "[\"206058503\"]"); // 原始JSON格式，更易读
    query.addQueryItem("locationCodes", QString("[\"%1\"]").arg(locationCode));
    query.addQueryItem("page", QString::number(targetPageCount));
    query.addQueryItem("pvid", pvid);
    query.addQueryItem("logid", logId);
    query.addQueryItem("h_n_purpose", "[\"1\"]");
    query.addQueryItem("structFieldMap", "{\"h_n_purpose\": [\"1\"]}");


    baseUrl.setQuery(query);
    QString houseUrl = baseUrl.toString(); // 生成最终合法URL


    searchUrlQueue.enqueue(houseUrl);
    emit appendLogSignal("📌 待爬URL（阿里巴巴普通住宅页面）：" + houseUrl);

    QWebEngineHttpRequest request{baseUrl}; // 直接传入QUrl，无需手动转换
    request.setHeader(QByteArray("User-Agent"), getRandomUA().toUtf8());
    if (!cookieStr.isEmpty()) {
        request.setHeader(QByteArray("Cookie"), cookieStr.toUtf8());
    }

    webPage->load(request);
    isHomeLoadedForSearch = false;
    pendingSearchKeyword.clear();
}
