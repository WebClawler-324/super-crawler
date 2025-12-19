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

// ===================== 城市名转拼音（修正：用 m_ui + 信号）=====================
QString Crawl::cityToPinyin(const QString& cityName) { // 去掉 ui 参数，用成员变量 m_ui
    QMap<QString, QString> cityPinyinMap = {
        {"北京", "bj"}, {"上海", "sh"}, {"广州", "gz"}, {"深圳", "sz"}, {"杭州", "hz"},
        {"南京", "nj"}, {"成都", "cd"}, {"重庆", "cq"}, {"武汉", "wh"}, {"西安", "xa"},
        {"天津", "tj"}, {"苏州", "sz"}
    };
    if (cityPinyinMap.contains(cityName)) {
        return cityPinyinMap[cityName];
    }

    QString pinyin;
    QStringEncoder encoder("GBK");

    if (!encoder.isValid()) {
        // 发送信号，由主线程更新 UI（避免跨线程）
        emit appendLogSignal("⚠️ 拼音转换失败：系统不支持 GBK 编码，返回城市名小写");
        return cityName.toLower();
    }

    for (QChar c : cityName) {
        if (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FA5) {
            QByteArray gbkBytes = encoder.encode(QString(c));
            if (gbkBytes.size() == 2) {
                uchar highByte = gbkBytes.at(0);
                uchar lowByte = gbkBytes.at(1);
                if (highByte >= 0xB0 && highByte <= 0xF7 && lowByte >= 0xA1 && lowByte <= 0xFE) {
                    int 区位码 = (highByte - 0xB0) * 94 + (lowByte - 0xA1);
                    QString firstLetter = getFirstLetter(区位码);
                    pinyin += firstLetter.toLower();
                }
            }
        } else {
            pinyin += c.toLower();
        }
    }

    return pinyin.isEmpty() ? cityName.toLower() : pinyin;
}

// ===================== 辅助函数：根据区位码获取首字母（不变）=====================
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

// ===================== 模拟真人行为（修正：用 m_mainWindow + 信号）=====================
void Crawl::simulateHumanBehavior() { // 去掉 ui 参数
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

// Cookie管理函数
void Crawl::loadCookiesFromFile(const QString& filePath) {
    QFile file(filePath.isEmpty() ? "ke_cookies.txt" : filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit appendLogSignal(QString("⚠️ Cookie文件加载失败：%1（将使用默认Cookie）").arg(file.fileName()));
        cookieStr = "s_ViewType=1; select_city=110000; city=beijing; lianjia_uuid=7477f2d0-8c9c-4e0d-b746-7a9f8499c9c8; "
                    "UM_distinctid=18c4f5e0d8d4b-0a63850565d14f-26031d51-144000-18c4f5e0d8e38a; "
                    "CNZZDATA1252603592=1732567464-1690000000-%7C1690000000; _smt_uid=64c7e0d8.5f3e1b92; "
                    "Hm_lvt_9152f8221cb6243a53c83b95a46c988=1690000000; Hm_lpvt_9152f8221cb6243a53c83b95a46c988=1690000000";
        return;
    }

    QTextStream in(&file);
    cookieStr = in.readAll().trimmed();
    file.close();
    emit appendLogSignal(QString("✅ 成功加载Cookie：%1").arg(cookieStr.isEmpty() ? "无" : "已加载（来自文件）"));
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

// 工具函数
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
    , m_ui(ui)          // 初始化：存储 UI 指针（借用，不管理生命周期）
    , m_mainWindow(mainWindow) // 初始化：存储窗口实例（稳定接收者）
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
        webPage->setParent(this); // 让 Crawl 管理 webPage 生命周期
    } else {
        webPage = new QWebEnginePage(this);
    }

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

    // 加载 Cookie
    loadCookiesFromFile();

    int delayMs = 1000 + QRandomGenerator::global()->bounded(2000);
    QTimer::singleShot(
        delayMs,
        this,  // 接收者改为 this，匹配槽函数所属类
        SLOT(onInitFinishedLog())
        );
}

Crawl::~Crawl() {
    // 清理 Web
    if (webPage != nullptr) {
        webPage->deleteLater(); // 延迟销毁，避免阻塞事件循环
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
    emit appendLogSignal("✅ 浏览器环境初始化完成，可开始爬取贝壳找房（低风控模式）");
    emit appendLogSignal("💡 使用说明：在输入框输入城市名（如：北京、上海），点击搜索对比按钮");
}

//处理普通URL
void Crawl::processNextUrl() {
    if (urlQueue.isEmpty()) {
        emit appendLogSignal("\n=== 贝壳找房首页爬取完成 ===");
        return;
    }

    QString currentUrl = urlQueue.dequeue();
    emit appendLogSignal("\n📌 正在加载：" + currentUrl);

    QUrl url(currentUrl);
    QWebEngineHttpRequest request(url);

    QString randomUA = getRandomUA();
    request.setHeader(QByteArray("User-Agent"), randomUA.toUtf8());
    request.setHeader(QByteArray("Referer"), QByteArray("https://www.ke.com/"));
    request.setHeader(QByteArray("Accept"), QByteArray("text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"));
    request.setHeader(QByteArray("Accept-Encoding"), QByteArray("gzip, deflate, br")); // 移除zstd
    request.setHeader(QByteArray("Accept-Language"), QByteArray("zh-CN,zh;q=0.9,en;q=0.8"));
    request.setHeader(QByteArray("Cache-Control"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Connection"), QByteArray("keep-alive"));
    // 移除 DNT:1（冗余，普通用户不携带）
    request.setHeader(QByteArray("Pragma"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Sec-Ch-Ua"), QByteArray("\"Chromium\";v=\"138\", \"Not=A?Brand\";v=\"8\", \"Google Chrome\";v=\"138\"")); // 更新版本
    request.setHeader(QByteArray("Sec-Ch-Ua-Mobile"), QByteArray("?0"));
    request.setHeader(QByteArray("Sec-Ch-Ua-Platform"), QByteArray("\"Windows\""));
    request.setHeader(QByteArray("Sec-Fetch-Dest"), QByteArray("document"));
    request.setHeader(QByteArray("Sec-Fetch-Mode"), QByteArray("navigate"));
    request.setHeader(QByteArray("Sec-Fetch-Site"), QByteArray("same-origin"));
    // 移除 Sec-Fetch-User:?1（冗余，脚本特征）
    request.setHeader(QByteArray("Upgrade-Insecure-Requests"), QByteArray("1"));

    if (!cookieStr.isEmpty()) {
        request.setHeader(QByteArray("Cookie"), cookieStr.toUtf8());
    } else {
        emit appendLogSignal("⚠️ 无有效Cookie，可能触发风控！");
    }

    webPage->load(request);
}

//页面加载完成槽函数
void Crawl::onPageLoadFinished(bool ok) {
    if (this == nullptr || webPage == nullptr) return;

    QString currentUrl = webPage->url().toString();
    bool isSearchTask = isProcessingSearchTask;

    // 处理首页加载完成后的搜索任务
    if (isHomeLoadedForSearch && currentUrl.contains("ke.com") && !currentUrl.contains("ershoufang")) {
        emit appendLogSignal("✅ 贝壳首页加载完成，延迟4-6秒后开始爬取二手房...");
        int homeDelay = 4000 + QRandomGenerator::global()->bounded(2000);

        QTimer::singleShot(
            homeDelay,
            this,
            [this]() { // Lambda 发送信号，不直接操作 UI
                emit appendLogSignal("🔍 开始执行二手房爬取任务...");
                processSearchUrl();
            }
            );

        isHomeLoadedForSearch = false;
        pendingSearchKeyword.clear();
        return;
    }

    // 风控检测
    bool isRiskPage = currentUrl.contains("verify", Qt::CaseInsensitive) ||
                      currentUrl.contains("captcha", Qt::CaseInsensitive) ||
                      currentUrl.contains("security", Qt::CaseInsensitive) ||
                      currentUrl.contains("antispam", Qt::CaseInsensitive);
    if (isRiskPage) {
        emit appendLogSignal("❌ 触发贝壳风控！跳转至验证页：" + currentUrl);
        emit appendLogSignal("💡 解决方案：");
        emit appendLogSignal("  1. 关闭VPN/代理，确保IP与登录Cookie一致；");
        emit appendLogSignal("  2. 降低爬取频率，单次仅爬1页；");
        emit appendLogSignal("  3. 重新获取最新Cookie并更新ke_cookies.txt。");
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
    simulateHumanBehavior(); // 无需传参，用成员变量

    // 渲染延迟
    int renderDelay = currentUrl.contains("ershoufang")
                          ? 15000 + QRandomGenerator::global()->bounded(5000)
                          : 4000 + QRandomGenerator::global()->bounded(3000);
    if (currentUrl.contains("ershoufang")) {
        emit appendLogSignal("⏳ 房源页等待完全渲染（" + QString::number(renderDelay/1000) + "秒）...");
    }

    // 渲染延迟后提取数据
    QTimer::singleShot(renderDelay, this, [this, currentUrl, isSearchTask]() {
        if (this == nullptr || webPage == nullptr) return;

        webPage->toHtml([this, currentUrl, isSearchTask](const QString& html) {
            // 调试日志：发送信号（避免子线程操作 UI）
            bool hasHouseNode = html.contains("li class=\"clear\"");
            emit appendLogSignal(QString("📋 获取到HTML：%1房源节点（li.clear）").arg(hasHouseNode ? "包含" : "不包含"));

            // 提取房源数据（搜索任务 + 二手房页面）
            if (isSearchTask && currentUrl.contains("ershoufang")) {
                extractHouseData(html);
                currentPageCount++;

                // 处理下一页或结束
                QString nextLog;
                //爬完当前页就结束，不加载下一页
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

//解析普通页面
void Crawl::extractKeData(const QString& html, const QString& baseUrl)
{
   emit appendLogSignal("🔍 开始解析贝壳页面...");

    QRegularExpression cityRegex(R"(<a\s+href=["'](https?://[^.]+.ke.com/)["']\s+class=["']city-item["'].*?>([\s\S]*?)</a>)",
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


void Crawl::extractHouseData(const QString& html)
{
    emit appendLogSignal("🔍 开始提取二手房房源数据...");

    // 1. 匹配单个房源容器：<li class="clear">
    QRegularExpression houseRegex(
        R"(<li\s+class=["']clear["'].*?>([\s\S]*?)</li>)",
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
        );

    QRegularExpressionMatchIterator houseIt = houseRegex.globalMatch(html);
    int extractCount = 0;

    while (houseIt.hasNext()) {
        QRegularExpressionMatch houseMatch = houseIt.next();
        QString houseHtml = houseMatch.captured(1).trimmed();


        // ========== 初始化字段 ==========
        QString title = "未知";
        QString communityName = "未知";
        QString totalPrice = "未知";
        QString unitPrice = "未知";
        QString houseType = "未知";
        QString area = "未知";
        QString orientation = "未知";
        QString floor = "未知";
        QString buildingYear = "未知";
        QString houseUrl = "未知";


        //  提取房源标题
        QRegularExpression titleRegex(R"(<a\s+.*?title=["']([^"']+)["'].*?>)", QRegularExpression::DotMatchesEverythingOption);
        if (titleRegex.match(houseHtml).hasMatch()) {
            title = titleRegex.match(houseHtml).captured(1).trimmed();
        }
       emit appendLogSignal(QString("\n📌 房源标题：%1").arg(title));


       //  2. 小区名提取（放弃多余判断，直接抓 a 标签文本）
       // 匹配 positionInfo 容器 + 预处理（移除span干扰）
       QRegularExpression posInfoRegex(R"(<div\s+class=["']positionInfo["']([\s\S]*?)</div>)", QRegularExpression::DotMatchesEverythingOption);
       QRegularExpressionMatch posInfoMatch = posInfoRegex.match(houseHtml);
       if (!posInfoMatch.hasMatch()) {
           emit appendLogSignal("❌ 未找到 div.positionInfo 容器");
           continue;
       }
       QString posInfoHtml = posInfoMatch.captured(0).trimmed();
       posInfoHtml.remove(QRegularExpression(R"(<span[^>]*>.*?</span>)")); // 移除span标签

       //匹配<a...>后到</a>前的所有内容
       QRegularExpression aTagRegex(
           R"(<a\s+[^>]*>([\s\S]*?)</a>)",
           QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
           );
       QRegularExpressionMatch aTagMatch = aTagRegex.match(posInfoHtml);

       if (aTagMatch.hasMatch()) {
           QString rawName = aTagMatch.captured(1).trimmed();

           communityName = rawName
                               .remove(QRegularExpression(R"(\s+)"))       // 移除多余空格
                               .replace("“", "").replace("”", "")          // 移除中文引号
                               .replace("\"", "").replace("'", "");         // 移除英文引号
           // 保留小区名中的合法特殊字符
       } else {

           int aTagStart = posInfoHtml.indexOf("<a");
           if (aTagStart == -1) {
               emit appendLogSignal("❌ positionInfo 内无 a 标签");
               continue;
           }
           int aTagClose = posInfoHtml.indexOf(">", aTagStart);
           int aTagEnd = posInfoHtml.indexOf("</a>", aTagClose); // 找</a>位置
           if (aTagClose == -1 || aTagEnd == -1) {
               emit appendLogSignal("❌ a 标签格式异常");
               continue;
           }

           QString temp = posInfoHtml.mid(aTagClose + 1, aTagEnd - aTagClose - 1).trimmed();

           temp = temp.remove(QRegularExpression(R"(\s+)")).replace("“", "").replace("”", "").replace("\"", "");
           if (!temp.isEmpty()) {
               communityName = temp;
           } else {
               emit appendLogSignal("❌ 未找到 a 标签内的有效文本");
           }
       }

       if (!communityName.isEmpty()) {
           emit appendLogSignal(QString("✅ 小区名提取成功：%1").arg(communityName));
       } else {
           emit appendLogSignal("❌ 小区名提取失败");
       }

       QRegularExpression totalPriceRegex(
           R"(<div\s+class=["']totalPrice totalPrice2["']>.*?<span\s+class=["']*["']>(\s*[\d.]+)\s*</span>.*?<i>万</i>.*?</div>)",
           QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
           );
       QRegularExpressionMatch priceMatch = totalPriceRegex.match(houseHtml); // 只匹配一次，提升效率
       if (priceMatch.hasMatch()) {
           QString priceNum = priceMatch.captured(1).trimmed();
           totalPrice = priceNum + " 万";

       } else {
           emit appendLogSignal("❌ 总价提取失败");
       }


       // 提取单价
       QRegularExpression unitPriceRegex(
           R"(<span\s*[^>]*>\s*([\d,.]+)\s*(元/平|元/㎡)</span>)",
           QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
           );
       QRegularExpressionMatch unitPriceMatch = unitPriceRegex.match(houseHtml);
       if (unitPriceMatch.hasMatch()) {
           QString priceNum = unitPriceMatch.captured(1).trimmed();
           unitPrice = priceNum + " 元/㎡";

       } else {
           emit appendLogSignal("❌ 单价提取失败");
       }


        // 提取楼层/建筑年代/户型/面积/朝向
        QRegularExpression houseInfoRegex(R"(<div\s+class=["']houseInfo["'].*?>([\s\S]*?)</div>)", QRegularExpression::DotMatchesEverythingOption);
        QString houseInfoHtml = houseInfoRegex.match(houseHtml).captured(1).trimmed();

        floor = "未知";
        buildingYear = "未知";
        houseType = "未知";
        area = "未知";
        orientation = "未知";

        if (!houseInfoHtml.isEmpty()) {
            //清理文本
            QString cleanHouseInfo = houseInfoHtml;
            cleanHouseInfo.replace(QRegularExpression(R"(<[^>]+>)"), ""); // 删所有HTML标签
            cleanHouseInfo.replace(QRegularExpression(R"(\n|\r)"), " "); // 换行符转为空格
            cleanHouseInfo = cleanHouseInfo.trimmed();                   // 去首尾空格

            emit appendLogSignal(QString("📋 清理后的houseInfo：%1").arg(cleanHouseInfo));


            QRegularExpression floorRegex(
                R"((底层|顶层|低楼层|中楼层|高楼层)\s*\(\s*共\d+层\s*\))", // 完整格式：底层 (共7层)
                QRegularExpression::CaseInsensitiveOption
                );
            QRegularExpressionMatch floorMatch = floorRegex.match(cleanHouseInfo);
            if (floorMatch.hasMatch()) {
                floor = floorMatch.captured(0).trimmed();
                //清理楼层的空格间距
                floor = floor.replace(QRegularExpression(R"(\s+)"), "");
                emit appendLogSignal(QString("✅楼层：%1").arg(floor));
            } else {

                QRegularExpression floorSimpleRegex(R"(底层|顶层|低楼层|中楼层|高楼层)");
                if (floorSimpleRegex.match(cleanHouseInfo).hasMatch()) {
                    floor = floorSimpleRegex.match(cleanHouseInfo).captured(0);
                    emit appendLogSignal(QString("✅ 兜底匹配楼层：%1").arg(floor));
                }
            }

            // 匹配户型
            QRegularExpression houseTypeRegex(R"(\d+室\d+厅)", QRegularExpression::CaseInsensitiveOption);
            if (houseTypeRegex.match(cleanHouseInfo).hasMatch()) {
                houseType = houseTypeRegex.match(cleanHouseInfo).captured(0);
                emit appendLogSignal(QString("✅户型：%1").arg(houseType));
            }

            // 匹配面积
            QRegularExpression areaRegex(R"((\d+(\.\d+)?)平米)", QRegularExpression::CaseInsensitiveOption);
            if (areaRegex.match(cleanHouseInfo).hasMatch()) {
                QString areaNum = areaRegex.match(cleanHouseInfo).captured(1);
                area = areaNum + " ㎡";
                emit appendLogSignal(QString("✅面积：%1").arg(area));
            }

            //匹配建筑年代
            QRegularExpression yearRegex(R"(\d{4}年)", QRegularExpression::CaseInsensitiveOption);
            if (yearRegex.match(cleanHouseInfo).hasMatch()) {
                buildingYear = yearRegex.match(cleanHouseInfo).captured(0);
                emit appendLogSignal(QString("✅建筑年代：%1").arg(buildingYear));
            }

            //匹配“朝向”
            QStringList dirWords = {"东南", "西南", "东北", "西北", "南", "北", "东", "西"}; // 长方向词优先（避免“东南”被拆为“东”+“南”）
            QString dirResult = "";
            foreach (QString dir, dirWords) {
                if (cleanHouseInfo.contains(dir) && !dirResult.contains(dir)) {
                    dirResult += dir + " ";
                }
            }
            orientation = dirResult.trimmed().isEmpty() ? "未知" : dirResult.trimmed();
            if (orientation != "未知") {
                emit appendLogSignal(QString("✅朝向：%1").arg(orientation));
            }


        } else {
            emit appendLogSignal("⚠️  未提取到 houseInfo 相关信息");
        }

        //提取房源链接
        QRegularExpression urlRegex(R"(<a\s+.*?href=["']([^"']+)["'].*?>)", QRegularExpression::DotMatchesEverythingOption);
        if (urlRegex.match(houseHtml).hasMatch()) {
            houseUrl = urlRegex.match(houseHtml).captured(1).trimmed();
            if (!houseUrl.startsWith("http")) houseUrl = "https://bj.ke.com" + houseUrl;
        }


        //存储数据
        if (!title.isEmpty() && !houseUrl.isEmpty() && !houseIdSet.contains(houseUrl)) {
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

            // 最终结果输出
            QString floorAndOri = (floor != "未知" ? floor : "") + (orientation != "未知" ? " " + orientation : "");
           emit appendLogSignal(QString("🎉 最终提取成功：小区=%1 | 总价=%2 | 户型=%3 | 面积=%4 | 楼层=%5")
                                     .arg(communityName, totalPrice, houseType, area, floorAndOri));
        }
    }

    emit appendLogSignal(QString("\n📊 提取完成：共%1条有效房源").arg(extractCount));

}


// 处理房源页URL
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
    request.setHeader(QByteArray("Referer"), QByteArray("https://www.ke.com/"));
    request.setHeader(QByteArray("Accept"), QByteArray("text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"));
    request.setHeader(QByteArray("Accept-Encoding"), QByteArray("gzip, deflate, br")); // 移除zstd
    request.setHeader(QByteArray("Accept-Language"), QByteArray("zh-CN,zh;q=0.9,en;q=0.8"));
    request.setHeader(QByteArray("Cache-Control"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Connection"), QByteArray("keep-alive"));
    request.setHeader(QByteArray("Pragma"), QByteArray("no-cache"));
    request.setHeader(QByteArray("Sec-Ch-Ua"), QByteArray("\"Chromium\";v=\"138\", \"Not=A?Brand\";v=\"8\", \"Google Chrome\";v=\"138\"")); // 更新版本
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

// 展示房源对比结果
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
            //插入数据到数据库
            mysql->insertInfo(house);
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

void Crawl::startHouseCrawl(const QString& city, int targetPages)
{
    currentCity = city.trimmed(); // 用 Crawl 类内成员 currentCity 替代全局变量
    if (currentCity.isEmpty()) {
        emit appendLogSignal("❌ 错误：请输入城市名！（如：北京、上海、广州）");
        return;
    }

    // 页数限制最多2页
    targetPageCount = targetPages;
    if (targetPageCount < 1) {
        targetPageCount = 1;
        emit appendLogSignal("⚠️  页码不能小于1，已自动调整为第1页");
    }
    if (targetPageCount > 5) {
        targetPageCount = 5;
        emit appendLogSignal("⚠️  风控限制：目标页码最大为5，已自动调整为第5页");
    }


    //清空旧数据
    searchUrlQueue.clear();    // 搜索URL队列
    houseDataList.clear();     // 房源数据列表
    houseIdSet.clear();        // 房源ID去重集合
    currentPageCount = 0;      // 当前已爬页数
    isProcessingSearchTask = true; // 搜索任务标志位

    emit appendLogSignal("=== 低风控模式：爬取「" + currentCity + "」二手房房源（第" + QString::number(targetPageCount) + "页）===");
    emit appendLogSignal("⚠️  风控提醒：单次只爬1页，目标页码范围1-5！");
    emit appendLogSignal("⚠️  请确保ke_cookies.txt中的Cookie是登录后最新抓取的！");
    emit appendLogSignal("————————————————");

    // 城市名转拼音（
    QString cityPinyin = cityToPinyin(currentCity);
    emit appendLogSignal("🏙️  城市拼音转换：" + currentCity + " → " + cityPinyin);

    //生成待爬取房源页 URL
    QString pvid = generateRandomPvid();
    QString logId = generateLogId();
    // 页码直接用 targetPageCount（目标页码）
    QString houseUrl = QString("https://%1.ke.com/ershoufang/pg%2/?pvid=%3&log_id=%4")
                           .arg(cityPinyin)
                           .arg(targetPageCount) // 这里用目标页码，不是循环变量
                           .arg(pvid)
                           .arg(logId);
    searchUrlQueue.enqueue(houseUrl);
    emit appendLogSignal("📌 待爬取房源页：" + houseUrl);


    emit appendLogSignal("🏠 第一步：先访问贝壳首页建立会话...");
    QString homeUrl = "https://www.ke.com/";
    QWebEngineHttpRequest homeRequest(homeUrl);
    QString homeUA = getRandomUA();

    // 设置请求头
    homeRequest.setHeader(QByteArray("User-Agent"), homeUA.toUtf8());
    homeRequest.setHeader(QByteArray("Referer"), QByteArray(""));
    homeRequest.setHeader(QByteArray("Accept"), QByteArray("text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"));
    homeRequest.setHeader(QByteArray("Accept-Encoding"), QByteArray("gzip, deflate, br")); // 移除zstd
    homeRequest.setHeader(QByteArray("Accept-Language"), QByteArray("zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6"));

    // 携带 Cooki
    if (!cookieStr.isEmpty()) {
        homeRequest.setHeader(QByteArray("Cookie"), cookieStr.toUtf8());
    }

    // 启动首页加载（
    webPage->load(homeRequest); // QWebEnginePage 实例
    isHomeLoadedForSearch = true; //首页加载标志
    pendingSearchKeyword = currentCity; //待搜索关键词
}

/*void Crawl::IntoDB()
{
    for(const auto& data: houseDataList){
        mysql->insertInfo(data);
    }
}*/

