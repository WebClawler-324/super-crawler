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

// 类内静态常量初始化
const int AliCrawl::REQUEST_INTERVAL = 3500;
const int AliCrawl::MAX_DEPTH = 1;
const int AliCrawl::MIN_REQUEST_INTERVAL = 9000;
const int AliCrawl::MAX_REQUEST_INTERVAL = 16000;
const QStringList AliCrawl::USER_AGENT_POOL = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36"
};

// ===================== 城市名转阿里城市编码（核心修改）=====================
QString AliCrawl::cityToPinyin(const QString& cityName) {
    // 阿里房产城市编码映射表（覆盖主流城市，可按需扩展）
    QMap<QString, QString> cityCodeMap = {
        {"北京", "110000"}, {"上海", "310000"}, {"广州", "440100"},
        {"深圳", "440300"}, {"杭州", "330100"}, {"南京", "320100"},
        {"成都", "510100"}, {"重庆", "500000"}, {"武汉", "420100"},
        {"西安", "610100"}, {"天津", "120000"}, {"苏州", "320500"},
        {"郑州", "410100"}, {"长沙", "430100"}, {"青岛", "370200"},
        {"宁波", "330200"}, {"无锡", "320200"}, {"佛山", "440600"},
        {"东莞", "441900"}, {"厦门", "350200"}, {"济南", "370100"},
        {"沈阳", "210100"}, {"合肥", "340100"}, {"福州", "350100"},
        {"昆明", "530100"}, {"大连", "210200"}, {"哈尔滨", "230100"},
        {"长春", "220100"}, {"石家庄", "130100"}, {"南宁", "450100"}
    };

    if (cityCodeMap.contains(cityName)) {
        return cityCodeMap[cityName]; // 返回城市编码（如北京→110000）
    }

    // 未匹配到编码时的处理
    emit appendLogSignal(QString("⚠️ 未支持「%1」的城市编码，请手动添加到cityCodeMap！").arg(cityName));
    return "";
}

// ===================== 辅助函数：根据区位码获取首字母（复用，仅拼音转换时用）=====================
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

// ===================== 模拟真人行为（适配阿里房产交互特点）=====================
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
        delay += 2500; // 阿里页面可能需要更长间隔
    }

    int totalStayTime = 8000 + QRandomGenerator::global()->bounded(4000);
    emit appendLogSignal("🤖 模拟浏览停留：" + QString::number(totalStayTime/1000) + "秒");
}

// Cookie管理函数（适配阿里房产Cookie）=====================
void AliCrawl::loadCookiesFromFile(const QString& filePath) {
    QFile file(filePath.isEmpty() ? "ali_cookies.txt" : filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit appendLogSignal(QString("⚠️ 阿里Cookie加载失败：%1（使用默认）").arg(file.fileName()));
        // 默认Cookie（建议替换为浏览器获取的真实Cookie）
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

// 工具函数（适配阿里参数规则）=====================
QString AliCrawl::generateRandomPvid() {
    const QString chars = "0123456789abcdefghijklmnopqrstuvwxyz";
    QString pvid;
    QRandomGenerator* gen = QRandomGenerator::global();
    pvid += "ali_";
    for (int i = 0; i < 28; i++) { // 阿里PVID长度保持不变
        pvid += chars.at(gen->bounded(chars.length()));
    }
    return pvid;
}

QString AliCrawl::generateLogId() {
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    int random = QRandomGenerator::global()->bounded(10000, 99999);
    return QString("log_%1_%2").arg(timestamp).arg(random); // 保留原有日志ID格式
}

QString AliCrawl::getRandomUA() {
    int index = QRandomGenerator::global()->bounded(USER_AGENT_POOL.size());
    return USER_AGENT_POOL.at(index);
}

int AliCrawl::getRandomInterval() {
    return QRandomGenerator::global()->bounded(MIN_REQUEST_INTERVAL, MAX_REQUEST_INTERVAL);
}

// 构造函数（初始化阿里特定配置）=====================
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

    // 关键：连接信号到startHouseCrawl（子线程内触发，线程安全）
    connect(this, &AliCrawl::startCrawlSignal, this, &AliCrawl::startHouseCrawl, Qt::QueuedConnection);

    // 阿里页面特殊配置（保留原有）
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
    emit appendLogSignal("💡 使用说明：输入城市名，点击爬取按钮（支持：北京、上海、广州等30+城市）");
}

// 处理普通URL（保留，暂不使用）=====================
void AliCrawl::processNextUrl() {
    if (urlQueue.isEmpty()) {
        emit appendLogSignal("\n=== 阿里房产首页爬取完成 ===");
        return;
    }

    QString currentUrl = urlQueue.dequeue();
    emit appendLogSignal("\n📌 加载页面：" + currentUrl);

    QUrl reqUrl(currentUrl);
    // 修复：用大括号初始化，避免编译器误解为函数声明
    QWebEngineHttpRequest request{reqUrl};

    QString randomUA = getRandomUA();
    request.setHeader(QByteArray("User-Agent"), randomUA.toUtf8());
    request.setHeader(QByteArray("Referer"), QByteArray("https://huodong.taobao.com/")); // 修正Referer
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

// 页面加载完成处理（适配新URL逻辑）=====================
void AliCrawl::onPageLoadFinished(bool ok) {
    if (this == nullptr || webPage == nullptr) return;

    QString currentUrl = webPage->url().toString();
    bool isSearchTask = isProcessingSearchTask;

    // 阿里风控检测（增强关键词）
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

    // 页面渲染延迟（淘宝页面渲染时间更长）
    int renderDelay = currentUrl.contains("ershoufang") || currentUrl.contains("pm/default/pc/4b05fb")
                          ? 22000 + QRandomGenerator::global()->bounded(8000) // 22-30秒
                          : 5000 + QRandomGenerator::global()->bounded(3000);
    if (currentUrl.contains("pm/default/pc/4b05fb")) {
        emit appendLogSignal("⏳ 等待房源渲染：" + QString::number(renderDelay/1000) + "秒");
    }

    QTimer::singleShot(renderDelay, this, [this, currentUrl, isSearchTask]() {
        if (this == nullptr || webPage == nullptr) return;

        webPage->toHtml([this, currentUrl, isSearchTask](const QString& html) {
            // 淘宝房源容器特征（修正为真实页面结构，需根据实际HTML调整）
            bool hasHouseNode = html.contains("div class=\"house-item\"") ||
                                html.contains("div class=\"item-wrap\"") ||
                                html.contains("div class=\"property-item\"");
            emit appendLogSignal(QString("📋 HTML包含房源节点：%1").arg(hasHouseNode ? "是" : "否"));

            // 仅处理二手房搜索结果页
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

// 解析阿里普通页面（保留，暂不使用）=====================
void AliCrawl::extractAliData(const QString& html, const QString& currentUrl) {
    emit appendLogSignal("🔍 解析阿里页面...");

    // 修正城市列表链接规则（适配淘宝域名）
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

// 提取阿里二手房数据（核心修正：适配淘宝页面结构）=====================
void AliCrawl::extractHouseData(const QString& html)
{
    emit appendLogSignal("🔍 开始提取阿里二手房房源数据...");

    // ========== 关键修复：宽松且精准的房源容器正则 ==========
    // 核心逻辑：匹配包含“标题span（numberoflines=2）+ 当前价文本 + 价格数字span（24px）”的最小div容器
    QRegularExpression houseRegex(
        R"(<div\s+[^>]*?>[\s\S]*?)"  // 外层div起始
        R"(<span\s+class=["']text["']\s+numberoflines=["']2["'])"  // 标题span（必含）
        R"([\s\S]{0,2000}?)"  // 标题到价格区的范围（足够覆盖单个房源）
        R"(当前价)"  // 价格区文本锚点（必含）
        R"([\s\S]{0,500}?)"  // 价格区内部范围
        R"(<span\s+class=["']text["'].*?font-size:\s*24px)"  // 24px价格数字span（必含）
        R"([\s\S]*?</div>)",  // 外层div结束
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption
        );

    QRegularExpressionMatchIterator houseIt = houseRegex.globalMatch(html);
    int extractCount = 0;
    int totalBlockCount = 0;

    // 统计总房源区块数
    QRegularExpressionMatchIterator countIt = houseRegex.globalMatch(html);
    while (countIt.hasNext()) {
        countIt.next();
        totalBlockCount++;
    }
    emit appendLogSignal(QString("📋 共识别到%1个房源容器").arg(totalBlockCount));

    // ========== 调试日志：确认HTML中是否存在核心锚点 ==========
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
        extractCount++;
        emit appendLogSignal(QString("\n=================================================="));
        emit appendLogSignal(QString("🏠 正在处理第%1个房源").arg(extractCount));
        emit appendLogSignal(QString("=================================================="));

        // ========== 初始化字段 ==========
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

        // 1. 提取房源标题（优先title属性，兼容空格和引号）
        QRegularExpression titleRegex(
            R"(<span\s+class=["']text["']\s+numberoflines=["']2["']\s+title=["']([^"']+)["'])",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch titleMatch = titleRegex.match(houseHtml);
        if (titleMatch.hasMatch()) {
            title = titleMatch.captured(1).trimmed();
            emit appendLogSignal(QString("✅ 房源标题：%1").arg(title));
        } else {
            // 兜底：提取span文本（去除HTML标签）
            QRegularExpression titleTextRegex(
                R"(<span\s+class=["']text["']\s+numberoflines=["']2["'].*?>([\s\S]*?)</span>)",
                QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
                );
            QRegularExpressionMatch titleTextMatch = titleTextRegex.match(houseHtml);
            if (titleTextMatch.hasMatch()) {
                title = titleTextMatch.captured(1).trimmed();
                title.remove(QRegularExpression("<[^>]*>"));  // 移除HTML标签
                title.replace(QRegularExpression("\\s+"), " ");  // 合并空格
                emit appendLogSignal(QString("✅ 兜底提取标题：%1").arg(title));
            } else {
                emit appendLogSignal("❌ 标题提取失败");
            }
        }
        // ========== 新增：非住宅房源过滤 ==========
        // 定义非住宅关键词列表（可根据实际需求扩展）
        QStringList nonHouseKeywords = {
            "车位", "车库", "商铺", "店面", "门市",
            "写字楼", "办公", "厂房", "仓库", "工业",
            "公寓式办公", "商办", "商住两用", "摊位", "柜台","储藏间",
            "B"
        };

        // 检查标题是否包含非住宅关键词（不区分大小写，中文不敏感）
        bool isNonHouse = false;
        foreach (const QString& keyword, nonHouseKeywords) {
            if (title.contains(keyword, Qt::CaseInsensitive)) {
                isNonHouse = true;
                break;
            }
        }
        if (isNonHouse) {
            emit appendLogSignal(QString("🚫 过滤非住宅房源：标题包含关键词（%1），跳过处理").arg(title));
            continue;  // 跳过当前房源，进入下一个循环
        }

        // 2. 提取基础信息（小区名、面积、户型、区域、城市）
        QRegularExpression baseInfoRegex(
            R"(<span\s+class=["']text["']\s+numberoflines=["']1["'].*?>([\s\S]*?)</span>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch baseInfoMatch = baseInfoRegex.match(houseHtml);
        if (baseInfoMatch.hasMatch()) {
            QString baseText = baseInfoMatch.captured(1).trimmed();
            baseText.remove(QRegularExpression("<[^>]*>"));  // 移除HTML标签
            baseText.replace(QRegularExpression("\\s+"), " ");  // 合并空格
            emit appendLogSignal(QString("📋 基础信息原始文本：%1").arg(baseText));

            QStringList baseList = baseText.split("|", Qt::SkipEmptyParts);
            for (int i = 0; i < baseList.size(); ++i) {
                baseList[i] = baseList[i].trimmed();
            }

            // 按内容特征识别字段（不依赖顺序）
            foreach (QString item, baseList) {
                // 🔥 修复1：bool类型不能调用.operator&&()，直接用&&运算符
                bool isArea = item.contains(QRegularExpression("\\d+(\\.\\d+)?")) && (item.contains("㎡") || item.contains("m²"));
                bool isHouseType = item.contains(QRegularExpression("\\d+室\\d+厅"));
                bool isCityRegion = item.contains(QRegularExpression("^[\\u4e00-\\u9fa5]+$"));

                if (isArea) {
                    // 🔥 修复2：正则转义错误（多了一个\），导致面积数字匹配失败
                    QRegularExpression areaNumRegex(R"(\d+(\.\d+)?)");
                    QRegularExpressionMatch areaNumMatch = areaNumRegex.match(item);
                    area = areaNumMatch.hasMatch() ? areaNumMatch.captured(0) + " ㎡" : item.replace("m²", "㎡");
                    emit appendLogSignal(QString("✅ 面积：%1").arg(area));
                } else if (isHouseType) {
                    houseType = item;
                    emit appendLogSignal(QString("✅ 户型：%1").arg(houseType));
                } else if (isCityRegion) {
                    // 城市：长度较短（2-4字），区域：长度较长（2-6字）
                    if (city == "未知" && item.length() <= 4) {
                        city = item;
                    } else if (region == "未知") {
                        region = item;
                    }
                } else if (communityName == "未知" && !item.isEmpty()) {
                    communityName = item;
                    emit appendLogSignal(QString("✅ 小区名：%1").arg(communityName));
                }
            }

            // 拼接位置
            location = city != "未知" && region != "未知" ? QString("%1市%2区").arg(city, region) :
                           city != "未知" ? QString("%1市").arg(city) : "未知";
            if (location != "未知") {
                emit appendLogSignal(QString("✅ 位置：%1").arg(location));
            }
        } else {
            emit appendLogSignal("❌ 未找到基础信息容器（numberoflines=1的text span）");
        }

        // 3. 提取总价（兼容样式属性中的空格和引号）
        QRegularExpression totalPriceRegex(
            R"(当前价[\s\S]{0,500}?)"
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

        // 4. 提取评估价（兼容“评估价”和“市场价”两种名称）
        QRegularExpression evalPriceRegex(
            R"((评估价|市场价))"  // 扩展：同时匹配两种价格名称
            R"([\s\S]{0,300}?)"
            R"(<span\s+class=["']text["'].*?>(\d+(?:\.\d+)?)万</span>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch evalMatch = evalPriceRegex.match(houseHtml);
        if (evalMatch.hasMatch()) {
            QString priceType = evalMatch.captured(1).trimmed();  // 捕获是“评估价”还是“市场价”
            QString evalNum = evalMatch.captured(2).trimmed();
            evalPrice = QString("%1 万").arg(evalNum);
            // 日志显示具体价格类型，保持原风格
            emit appendLogSignal(QString("✅ %1：%2").arg(priceType, evalPrice));
        } else {
            emit appendLogSignal("❌ 评估价/市场价提取失败");
        }
        // 5. 提取楼层（从标题中匹配“X层”）
        QRegularExpression floorRegex(R"((\d+层))", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch floorMatch = floorRegex.match(title);
        if (floorMatch.hasMatch()) {
            floor = floorMatch.captured(1).trimmed();
            emit appendLogSignal(QString("✅ 楼层：%1").arg(floor));
        } else {
            emit appendLogSignal("❌ 楼层提取失败（标题中无明确楼层）");
        }

        // 6. 提取朝向（从标题或基础信息中匹配方向词）
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

        // 7. 提取房源链接（兼容相对路径和绝对路径）
        QRegularExpression urlRegex(
            R"(<a\s+[^>]*?href=["']([^"']+)["'].*?>)",
            QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption
            );
        QRegularExpressionMatch urlMatch = urlRegex.match(houseHtml);
        if (urlMatch.hasMatch()) {
            houseUrl = urlMatch.captured(1).trimmed();
            // 处理相对路径
            if (houseUrl.startsWith("//")) {
                houseUrl = "https:" + houseUrl;
            } else if (!houseUrl.startsWith("http") && !houseUrl.isEmpty()) {
                houseUrl = "https://huodong.taobao.com" + houseUrl;
            }
            emit appendLogSignal(QString("✅ 房源链接：%1").arg(houseUrl));
        } else {
            emit appendLogSignal("❌ 房源链接提取失败");
        }

        // 8. 计算单价（总价/面积）
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

        // 9. 存储数据（去重+核心字段校验）
        if (!title.isEmpty() && title != "未知" && !houseUrl.isEmpty() && !houseIdSet.contains(houseUrl)) {
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

    // 最终统计
    emit appendLogSignal(QString("\n=================================================="));
    emit appendLogSignal(QString("📊 提取完成：共识别%1个房源容器，成功存储%2条有效房源").arg(totalBlockCount).arg(houseDataList.size()));
    emit appendLogSignal("==================================================\n");
}
// 处理阿里房源搜索URL（修正请求头+对象初始化）=====================
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
    // 修复：用大括号初始化，避免编译器误解为函数声明
    QWebEngineHttpRequest request{reqUrl};
    QString randomUA = getRandomUA();
    request.setHeader(QByteArray("User-Agent"), randomUA.toUtf8());

    // 修正请求头（适配淘宝域名）
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

// 展示阿里房源结果（保留原有逻辑）=====================
void AliCrawl::showHouseCompareResult() {
    emit appendLogSignal("\n" + QString("=").repeated(80));
    emit appendLogSignal("=== " + currentCity + "阿里二手房对比结果（共" + QString::number(houseDataList.size()) + "条）===");
    emit appendLogSignal(QString("=").repeated(80));

    // 排序（按总价升序，保持原有逻辑，优化价格解析容错）
    std::sort(houseDataList.begin(), houseDataList.end(), [](const HouseInfo& a, const HouseInfo& b) {
        // 解析总价（兼容“未知”“56 万”“110.59 万”格式）
        auto parsePrice = [](const QString& priceStr) -> double {
            QString temp = priceStr;
            if (temp == "未知" || !temp.contains(QRegularExpression(R"(\d+)"))) {
                return 1e18; // 未知价格排最后
            }
            temp = temp.remove("万").remove(",").remove(" ").trimmed();
            bool ok;
            double price = temp.toDouble(&ok);
            return ok ? price : 1e18;
        };
        return parsePrice(a.price) < parsePrice(b.price);
    });

    // 输出详情（补充区域、单价、年租，优化格式排版）
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

    // 统计信息（优化容错，补充更多维度）
    if (!houseDataList.isEmpty()) {
        emit appendLogSignal("\n🔥 对比总结：");

        // 1. 最低价房源（过滤未知价格）
        HouseInfo cheapest = houseDataList.first();
        if (cheapest.price != "未知") {
            emit appendLogSignal("✅ 最低价：" + cheapest.communityName + " - " + cheapest.price +
                                 "（区域：" + cheapest.region + " | 单价：" + cheapest.unitPrice + "）");
        } else {
            emit appendLogSignal("✅ 最低价：暂无有效价格房源");
        }

        // 2. 总价统计（均价）
        double totalPrice = 0;
        int validPriceCount = 0;
        // 3. 单价统计（均价，新增）
        double totalUnitPrice = 0;
        int validUnitPriceCount = 0;

        for (auto& house : houseDataList) {
            // 统计总价
            QString priceStr = house.price;
            if (priceStr != "未知" && priceStr.contains(QRegularExpression(R"(\d+)"))) {
                priceStr = priceStr.remove("万").remove(",").remove(" ").trimmed();
                bool ok;
                double price = priceStr.toDouble(&ok);
                if (ok) {
                    totalPrice += price;
                    validPriceCount++;
                }
            }

            // 统计单价（新增）
            QString unitPriceStr = house.unitPrice;
            if (unitPriceStr != "未知" && unitPriceStr != "计算失败" && unitPriceStr.contains(QRegularExpression(R"(\d+)"))) {
                unitPriceStr = unitPriceStr.remove("元/㎡").remove(",").remove(" ").trimmed();
                bool ok;
                double unitPrice = unitPriceStr.toDouble(&ok);
                if (ok) {
                    totalUnitPrice += unitPrice;
                    validUnitPriceCount++;
                }
            }

            // mysql->insertInfo(house); // 需启用时取消注释
        }

        // 输出均价（总价+单价）
        if (validPriceCount > 0) {
            emit appendLogSignal("✅ 总价均价：" + QString::number(totalPrice / validPriceCount, 'f', 1) + " 万");
        } else {
            emit appendLogSignal("✅ 总价均价：暂无有效价格数据");
        }

        if (validUnitPriceCount > 0) { // 新增单价均价统计
            emit appendLogSignal("✅ 单价均价：" + QString::number(totalUnitPrice / validUnitPriceCount, 'f', 0) + " 元/㎡");
        } else {
            emit appendLogSignal("✅ 单价均价：暂无有效单价数据");
        }

        // 新增：统计有年租的房源数量（如果有）
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

// 启动阿里房产爬取（核心修改：生成淘宝格式URL+对象初始化修复）=====================
void AliCrawl::startHouseCrawl(const QString& city, int targetPages) {
    currentCity = city.trimmed();
    if (currentCity.isEmpty()) {
        emit appendLogSignal("❌ 请输入城市名！");
        return;
    }

    targetPageCount = qBound(1, targetPages, 5); // 限制1-5页
    emit appendLogSignal("=== 爬取「" + currentCity + "」阿里二手房（第" + QString::number(targetPageCount) + "页）===");

    // 清空旧数据
    searchUrlQueue.clear();
    houseDataList.clear();
    houseIdSet.clear();
    currentPageCount = 0;
    isProcessingSearchTask = true;

    // 获取城市编码（替代原拼音转换）
    QString locationCode = cityToPinyin(currentCity);
    if (locationCode.isEmpty()) {
        emit appendLogSignal("❌ 城市编码获取失败，无法生成URL！");
        isProcessingSearchTask = false;
        return;
    }
    emit appendLogSignal("🏙️ 城市编码：" + currentCity + " → " + locationCode);

    // 生成随机参数
    QString pvid = generateRandomPvid();
    QString logId = generateLogId();

    // 核心：生成淘宝格式的二手房URL（严格匹配真实参数）
    QString baseUrl = "https://huodong.taobao.com/wow/pm/default/pc/4b05fb";
    QString keyword = QString("二手房").toUtf8().toPercentEncoding(); // 关键词URL编码
    QString fcatV4Ids = "[%22206058503%22]"; // 二手房固定分类ID（已URL编码）
    QString locationCodes = "[%22" + locationCode + "%22]"; // 城市编码URL编码
    QString page = QString::number(targetPageCount);

    // 拼接完整URL
    QString houseUrl = QString("%1?keyword=%2&fcatV4Ids=%3&locationCodes=%4&page=%5&pvid=%6&logid=%7")
                           .arg(baseUrl)
                           .arg(keyword)
                           .arg(fcatV4Ids)
                           .arg(locationCodes)
                           .arg(page)
                           .arg(pvid)
                           .arg(logId);

    searchUrlQueue.enqueue(houseUrl);
    emit appendLogSignal("📌 待爬URL：" + houseUrl);

    // 直接加载目标URL（无需访问fang.ali.com首页）
    emit appendLogSignal("🏠 开始加载阿里二手房页面...");
    // 修复1：先创建QUrl对象，再用大括号初始化request，避免编译器误解
    QUrl reqUrl(houseUrl);
    QWebEngineHttpRequest request{reqUrl};
    // 修复2：setHeader参数正确传递
    request.setHeader(QByteArray("User-Agent"), getRandomUA().toUtf8());
    if (!cookieStr.isEmpty()) {
        request.setHeader(QByteArray("Cookie"), cookieStr.toUtf8());
    }

    // 修复3：load参数为正确的request对象
    webPage->load(request);
    isHomeLoadedForSearch = false; // 禁用首页加载标志
    pendingSearchKeyword.clear();
}
