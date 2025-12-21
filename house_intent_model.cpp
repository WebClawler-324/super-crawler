#include "house_intent_model.h"

#include <QFile>
#include <QDataStream>
#include <QSaveFile>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTextStream>
#include <QtGlobal>
#include <QDir>
#include <QFileInfo>
#include <cmath>
#include <utility>

// ----------------------------- logging -----------------------------
void HouseIntentModel::logLine(QStringList* logs, const QString& s) {
    qWarning().noquote() << s;
    if (logs) logs->push_back(s);
}

// ----------------------------- config helpers -----------------------------
HouseIntentModel::Config HouseIntentModel::defaultConfig() {
    Config cfg;
    // 这些值你可以按需调整默认
    cfg.pp.max_prices = 1000.0f;
    cfg.pp.max_room = 5;
    cfg.pp.max_floor = 60.0f;
    cfg.pp.max_meter_price = 100000.0f;
    cfg.pp.min_year = 1950;
    cfg.pp.max_year = 2025;

    cfg.trainOpt.epochs = 150;
    cfg.trainOpt.lr = 0.05f;
    cfg.trainOpt.l2 = 1e-4f;
    cfg.trainOpt.shuffle = true;
    return cfg;
}

static QString stripInlineComment(QString line) {
    // 支持 # 和 //
    int p1 = line.indexOf('#');
    int p2 = line.indexOf("//");
    int cut = -1;
    if (p1 >= 0) cut = p1;
    if (p2 >= 0) cut = (cut < 0) ? p2 : std::min(cut, p2);
    if (cut >= 0) line = line.left(cut);
    return line.trimmed();
}

static bool parseBoolLoose(const QString& s, bool* out) {
    QString t = s.trimmed().toLower();
    if (t == "1" || t == "true" || t == "yes" || t == "y" || t == "on") { *out = true; return true; }
    if (t == "0" || t == "false" || t == "no"  || t == "n" || t == "off") { *out = false; return true; }
    return false;
}

bool HouseIntentModel::loadConfigFromTxt(const QString& path,
                                         Config* outCfg,
                                         QStringList* logs,
                                         QString* errMsg) {
    if (!outCfg) {
        if (errMsg) *errMsg = "outCfg 为空。";
        return false;
    }

    Config cfg = defaultConfig();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errMsg) *errMsg = "无法打开配置文件：" + path;
        logLine(logs, "[ML][Config] 无法打开配置文件，将使用默认配置: " + path);
        *outCfg = cfg;
        return false;
    }

    QTextStream in(&f);
    int lineNo = 0;
    while (!in.atEnd()) {
        ++lineNo;
        QString line = in.readLine();
        line = stripInlineComment(line);
        if (line.isEmpty()) continue;

        int eq = line.indexOf('=');
        if (eq < 0) {
            logLine(logs, QString("[ML][Config][L%1] 无 '=' : '%2' -> 忽略").arg(lineNo).arg(line));
            continue;
        }

        QString key = line.left(eq).trimmed();
        QString val = line.mid(eq + 1).trimmed();
        if (key.isEmpty()) {
            logLine(logs, QString("[ML][Config][L%1] key 为空 -> 忽略").arg(lineNo));
            continue;
        }

        auto warnBad = [&](const QString& msg){
            logLine(logs, QString("[ML][Config][L%1] %2（key=%3, val=%4）-> 回退默认")
                              .arg(lineNo).arg(msg).arg(key).arg(val));
        };

        // ---- Preprocessor keys ----
        if (key == "max_prices") {
            bool ok=false; double v=val.toDouble(&ok);
            if (!ok || !std::isfinite(v) || v <= 0) { warnBad("max_prices 非法"); }
            else cfg.pp.max_prices = static_cast<float>(v);
        } else if (key == "max_room") {
            bool ok=false; int v=val.toInt(&ok);
            if (!ok || v <= 0 || v > 50) { warnBad("max_room 非法"); }
            else cfg.pp.max_room = v;
        } else if (key == "max_floor") {
            bool ok=false; double v=val.toDouble(&ok);
            if (!ok || !std::isfinite(v) || v <= 0) { warnBad("max_floor 非法"); }
            else cfg.pp.max_floor = static_cast<float>(v);
        } else if (key == "max_meter_price") {
            bool ok=false; double v=val.toDouble(&ok);
            if (!ok || !std::isfinite(v) || v <= 0) { warnBad("max_meter_price 非法"); }
            else cfg.pp.max_meter_price = static_cast<float>(v);
        } else if (key == "min_year") {
            bool ok=false; int v=val.toInt(&ok);
            if (!ok || v < 0 || v > 3000) { warnBad("min_year 非法"); }
            else cfg.pp.min_year = v;
        } else if (key == "max_year") {
            bool ok=false; int v=val.toInt(&ok);
            if (!ok || v < 0 || v > 3000) { warnBad("max_year 非法"); }
            else cfg.pp.max_year = v;
        }

        // ---- Train keys ----
        else if (key == "epochs") {
            bool ok=false; int v=val.toInt(&ok);
            if (!ok || v <= 0 || v > 1000000) { warnBad("epochs 非法"); }
            else cfg.trainOpt.epochs = v;
        } else if (key == "lr") {
            bool ok=false; double v=val.toDouble(&ok);
            if (!ok || !std::isfinite(v) || v <= 0) { warnBad("lr 非法"); }
            else cfg.trainOpt.lr = static_cast<float>(v);
        } else if (key == "l2") {
            bool ok=false; double v=val.toDouble(&ok);
            if (!ok || !std::isfinite(v) || v < 0) { warnBad("l2 非法"); }
            else cfg.trainOpt.l2 = static_cast<float>(v);
        } else if (key == "shuffle") {
            bool b=false;
            if (!parseBoolLoose(val, &b)) { warnBad("shuffle 非法(应为0/1/true/false等)"); }
            else cfg.trainOpt.shuffle = b;
        } else {
            logLine(logs, QString("[ML][Config][L%1] 未知 key: '%2' -> 忽略").arg(lineNo).arg(key));
        }
    }

    // 额外一致性检查
    if (cfg.pp.min_year > cfg.pp.max_year) {
        logLine(logs, "[ML][Config] min_year > max_year，已交换两者");
        std::swap(cfg.pp.min_year, cfg.pp.max_year);
    }

    *outCfg = cfg;
    logLine(logs, "[ML][Config] 配置加载完成: " + path);
    return true;
}

bool HouseIntentModel::saveConfigTemplate(const QString& path, QString* errMsg) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errMsg) *errMsg = "无法写入配置模板：" + path;
        return false;
    }

    QTextStream out(&f);
    out << "# -------- House Intent Model Config --------\n"
        << "# key=value\n"
        << "# 注释支持：# 或 //\n\n"
        << "max_prices = 500\n"
        << "max_room = 5\n"
        << "max_floor = 60\n"
        << "max_meter_price = 20000\n"
        << "min_year = 1900\n"
        << "max_year = 2025\n\n"
        << "epochs = 150\n"
        << "lr = 0.05\n"
        << "l2 = 0.0001\n"
        << "shuffle = 1\n";

    if (!f.commit()) {
        if (errMsg) *errMsg = "保存配置模板失败：" + path;
        return false;
    }
    return true;
}

// ----------------------------- utils -----------------------------
static float sigmoid_stable(float z) {
    if (z >= 0.0f) {
        float ez = std::exp(-z);
        return 1.0f / (1.0f + ez);
    } else {
        float ez = std::exp(z);
        return ez / (1.0f + ez);
    }
}

static void shuffle_indices(QVector<int>& idx) {
    for (int i = idx.size() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        std::swap(idx[i], idx[j]);
    }
}

static QString cleaned(QString s) {
    s = s.trimmed();
    s.replace(",", "");
    s.replace("，", "");
    s.replace(" ", "");
    return s;
}

static bool extractFirstNumber(const QString& s, double* out) {
    QRegularExpression re(R"(([+-]?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?))");
    auto m = re.match(s);
    if (!m.hasMatch()) return false;
    bool ok = false;
    double v = m.captured(1).toDouble(&ok);
    if (!ok) return false;
    *out = v;
    return true;
}

static int chineseNumToInt_0_99(const QString& s, bool* ok) {
    if (ok) *ok = true;
    QString t = s;
    if (t.isEmpty()) { if (ok) *ok = false; return 0; }

    auto val = [](QChar c)->int {
        switch (c.unicode()) {
        case u'零': return 0;
        case u'一': return 1;
        case u'二': return 2;
        case u'三': return 3;
        case u'四': return 4;
        case u'五': return 5;
        case u'六': return 6;
        case u'七': return 7;
        case u'八': return 8;
        case u'九': return 9;
        default: return -1;
        }
    };

    bool ok2 = false;
    int d = t.toInt(&ok2);
    if (ok2) return d;

    int tenPos = t.indexOf(u'十');
    if (tenPos >= 0) {
        int tens = 1;
        if (tenPos > 0) {
            int v = val(t[0]);
            if (v < 0) { if (ok) *ok = false; return 0; }
            tens = v;
        }
        int ones = 0;
        if (tenPos + 1 < t.size()) {
            int v = val(t[tenPos + 1]);
            if (v < 0) { if (ok) *ok = false; return 0; }
            ones = v;
        }
        return tens * 10 + ones;
    }

    if (t.size() == 1) {
        int v = val(t[0]);
        if (v < 0) { if (ok) *ok = false; return 0; }
        return v;
    }

    if (ok) *ok = false;
    return 0;
}

static bool parseMaybeChineseInt(const QString& token, int* out) {
    bool ok = false;
    int v = chineseNumToInt_0_99(token, &ok);
    if (!ok) return false;
    *out = v;
    return true;
}

// ---------- robust normalization ----------
static QString normalizeTotalPriceWan(const QString& raw, QStringList* logs, int idx) {
    QString s = cleaned(raw);
    if (s.isEmpty() || s == "未知") {
        HouseIntentModel::logLine(logs, QString("[ML][%1][总价] 空/未知 -> 置0").arg(idx));
        return "";
    }

    double num = 0.0;
    if (!extractFirstNumber(s, &num)) {
        HouseIntentModel::logLine(logs, QString("[ML][%1][总价] 无法提取数字: '%2' -> 置0").arg(idx).arg(raw));
        return "";
    }

    double wan = 0.0;
    if (s.contains(u'亿')) wan = num * 10000.0;
    else if (s.contains(u'万') || s.contains("w") || s.contains("W")) wan = num;
    else if (s.contains(u'元')) wan = num / 10000.0;
    else {
        if (std::fabs(num) > 10000.0) wan = num / 10000.0;
        else wan = num;
        HouseIntentModel::logLine(logs, QString("[ML][%1][总价] 无单位，猜测为%2：'%3'")
                                            .arg(idx).arg(std::fabs(num) > 10000.0 ? "元" : "万").arg(raw));
    }

    if (!std::isfinite(wan) || wan < 0) {
        HouseIntentModel::logLine(logs, QString("[ML][%1][总价] 数值非法: '%2' -> 置0").arg(idx).arg(raw));
        return "";
    }

    return QString::number(wan, 'f', 3) + "万";
}

static QString normalizeMeterPriceYuan(const QString& raw, QStringList* logs, int idx) {
    QString s = cleaned(raw);
    if (s.isEmpty() || s == "未知") {
        HouseIntentModel::logLine(logs, QString("[ML][%1][单价] 空/未知 -> 置0").arg(idx));
        return "";
    }

    double num = 0.0;
    if (!extractFirstNumber(s, &num)) {
        HouseIntentModel::logLine(logs, QString("[ML][%1][单价] 无法提取数字: '%2' -> 置0").arg(idx).arg(raw));
        return "";
    }

    double yuan = 0.0;
    if (s.contains(u'万') || s.contains("w") || s.contains("W")) yuan = num * 10000.0;
    else if (s.contains("k") || s.contains("K")) yuan = num * 1000.0;
    else yuan = num;

    if (!std::isfinite(yuan) || yuan < 0) {
        HouseIntentModel::logLine(logs, QString("[ML][%1][单价] 数值非法: '%2' -> 置0").arg(idx).arg(raw));
        return "";
    }

    return QString::number(yuan, 'f', 2) + "元/㎡";
}

static QString normalizeLayout(const QString& raw, const Preprocessor::Config& cfg, QStringList* logs, int idx) {
    QString s = cleaned(raw);
    if (s.isEmpty() || s == "未知") {
        HouseIntentModel::logLine(logs, QString("[ML][%1][户型] 空/未知 -> one-hot 全0").arg(idx));
        return "";
    }

    s.replace("房间", "室");
    s.replace("房", "室");
    s.replace("居", "室");

    {
        QRegularExpression re(R"(([0-9一二三四五六七八九十]{1,3})室([0-9一二三四五六七八九十]{1,3})厅)");
        auto m = re.match(s);
        if (m.hasMatch()) {
            int p=0,q=0;
            if (!parseMaybeChineseInt(m.captured(1), &p) || !parseMaybeChineseInt(m.captured(2), &q)) {
                HouseIntentModel::logLine(logs, QString("[ML][%1][户型] 数字解析失败: '%2' -> 全0").arg(idx).arg(raw));
                return "";
            }
            if (p < 1 || q < 1 || p > cfg.max_room || q > cfg.max_room) {
                HouseIntentModel::logLine(logs, QString("[ML][%1][户型] 超出max_room=%2: '%3' -> 全0")
                                                    .arg(idx).arg(cfg.max_room).arg(raw));
                return "";
            }
            return QString("%1室%2厅").arg(p).arg(q);
        }
    }

    {
        QRegularExpression re(R"(([0-9一二三四五六七八九十]{1,3})室.*?([0-9一二三四五六七八九十]{1,3})厅)");
        auto m = re.match(s);
        if (m.hasMatch()) {
            int p=0,q=0;
            if (!parseMaybeChineseInt(m.captured(1), &p) || !parseMaybeChineseInt(m.captured(2), &q)) {
                HouseIntentModel::logLine(logs, QString("[ML][%1][户型] 数字解析失败: '%2' -> 全0").arg(idx).arg(raw));
                return "";
            }
            if (p < 1 || q < 1 || p > cfg.max_room || q > cfg.max_room) {
                HouseIntentModel::logLine(logs, QString("[ML][%1][户型] 超出max_room=%2: '%3' -> 全0")
                                                    .arg(idx).arg(cfg.max_room).arg(raw));
                return "";
            }
            return QString("%1室%2厅").arg(p).arg(q);
        }
    }

    {
        QRegularExpression re(R"(([0-9一二三四五六七八九十]{1,3})室)");
        auto m = re.match(s);
        if (m.hasMatch()) {
            int p=0;
            if (!parseMaybeChineseInt(m.captured(1), &p)) {
                HouseIntentModel::logLine(logs, QString("[ML][%1][户型] 解析室数失败: '%2' -> 全0").arg(idx).arg(raw));
                return "";
            }
            int q = 1;
            HouseIntentModel::logLine(logs, QString("[ML][%1][户型] 缺少厅信息: '%2' -> 默认%3室%4厅")
                                                .arg(idx).arg(raw).arg(p).arg(q));
            if (p < 1 || q < 1 || p > cfg.max_room || q > cfg.max_room) {
                HouseIntentModel::logLine(logs, QString("[ML][%1][户型] 默认后仍超出max_room=%2: '%3' -> 全0")
                                                    .arg(idx).arg(cfg.max_room).arg(raw));
                return "";
            }
            return QString("%1室%2厅").arg(p).arg(q);
        }
    }

    HouseIntentModel::logLine(logs, QString("[ML][%1][户型] 无法解析: '%2' -> 全0").arg(idx).arg(raw));
    return "";
}

static QString normalizeFloor(const QString& raw, QStringList* logs, int idx) {
    QString s = cleaned(raw);
    if (s.isEmpty() || s == "未知") {
        HouseIntentModel::logLine(logs, QString("[ML][%1][楼层] 空/未知 -> 全0").arg(idx));
        return "";
    }

    int total = 0;
    int current = 0;
    bool hasTotal = false;
    bool hasCurrent = false;

    {
        QRegularExpression re(R"(共(\d+)层)");
        auto m = re.match(s);
        if (m.hasMatch()) {
            total = m.captured(1).toInt();
            hasTotal = true;
        }
    }

    {
        QRegularExpression re(R"((\d+)\s*/\s*(\d+)\s*层)");
        auto m = re.match(s);
        if (m.hasMatch()) {
            current = m.captured(1).toInt();
            total = m.captured(2).toInt();
            hasCurrent = true;
            hasTotal = true;
        }
    }

    if (!hasCurrent) {
        QRegularExpression re(R"((\d+)层)");
        auto m = re.match(s);
        if (m.hasMatch()) {
            current = m.captured(1).toInt();
            hasCurrent = true;
        }
    }

    QString level;
    if (s.contains("低") || s.contains("底")) level = "低";
    else if (s.contains("高") || s.contains("顶")) level = "高";
    else if (s.contains("中")) level = "中";

    if (level.isEmpty() && hasCurrent && hasTotal && total > 0) {
        double r = double(current) / double(total);
        if (r <= 1.0/3.0) level = "低";
        else if (r >= 2.0/3.0) level = "高";
        else level = "中";
        HouseIntentModel::logLine(logs, QString("[ML][%1][楼层] 未给低中高，按%2/%3推断为%4")
                                            .arg(idx).arg(current).arg(total).arg(level));
    }

    if (!hasTotal || total <= 0) {
        HouseIntentModel::logLine(logs, QString("[ML][%1][楼层] 缺少总层数: '%2' -> 全0").arg(idx).arg(raw));
        return "";
    }

    if (level.isEmpty()) {
        HouseIntentModel::logLine(logs, QString("[ML][%1][楼层] 缺少楼层档位(低/中/高): '%2' -> 全0").arg(idx).arg(raw));
        return "";
    }

    return QString("%1楼层（共%2层）").arg(level).arg(total);
}

static QString normalizeOrientation(const QString& raw, QStringList* logs, int idx) {
    QString s = raw.trimmed();
    if (s.isEmpty() || s == "未知") {
        HouseIntentModel::logLine(logs, QString("[ML][%1][朝向] 空/未知 -> 全0").arg(idx));
        return "";
    }

    s.replace("/", " ").replace("、", " ").replace(",", " ").replace("，", " ");
    s.replace("朝", "");
    s.replace("向", "");
    s = cleaned(s);

    bool hasE = s.contains("东");
    bool hasW = s.contains("西");
    bool hasS = s.contains("南");
    bool hasN = s.contains("北");

    if (hasE && hasS) return "东南";
    if (hasE && hasN) return "东北";
    if (hasW && hasS) return "西南";
    if (hasW && hasN) return "西北";

    if (hasS) return "南";
    if (hasN) return "北";
    if (hasE) return "东";
    if (hasW) return "西";

    HouseIntentModel::logLine(logs, QString("[ML][%1][朝向] 无法识别: '%2' -> 全0").arg(idx).arg(raw));
    return "";
}

static QString normalizeYear(const QString& raw,
                             const Preprocessor::Config& cfg,
                             QStringList* logs,
                             int idx) {
    QString s = cleaned(raw);
    if (s.isEmpty() || s == "未知") {
        HouseIntentModel::logLine(logs, QString("[ML][%1][年代] 空/未知 -> 全0").arg(idx));
        return "";
    }

    {
        QRegularExpression re(R"((\d{4}))");
        auto m = re.match(s);
        if (m.hasMatch()) {
            int year = m.captured(1).toInt();
            if (year < cfg.min_year || year > cfg.max_year) {
                HouseIntentModel::logLine(logs, QString("[ML][%1][年代] 超出范围[%2,%3]: %4（将clamp）")
                                                    .arg(idx).arg(cfg.min_year).arg(cfg.max_year).arg(year));
            }
            return QString::number(year) + "年";
        }
    }

    {
        QRegularExpression re(R"((\d{2})年)");
        auto m = re.match(s);
        if (m.hasMatch()) {
            int yy = m.captured(1).toInt();
            int year = (yy >= 30) ? (1900 + yy) : (2000 + yy);
            HouseIntentModel::logLine(logs, QString("[ML][%1][年代] 2位年份推断: '%2' -> %3年").arg(idx).arg(raw).arg(year));
            return QString::number(year) + "年";
        }
    }

    HouseIntentModel::logLine(logs, QString("[ML][%1][年代] 无法解析: '%2' -> 全0").arg(idx).arg(raw));
    return "";
}

// ----------------------------- core model -----------------------------
SigmoidRegressor::SigmoidRegressor(int input_dim)
    : w(Eigen::VectorXf::Zero(input_dim)), b(0.0f) {}

float SigmoidRegressor::forward(const Eigen::VectorXf& x) const {
    float z = w.dot(x) + b;
    return sigmoid_stable(z);
}

// --------- Trainer ----------
bool Trainer::trainMSE(SigmoidRegressor& model,
                       const QVector<Eigen::VectorXf>& X,
                       const QVector<float>& y,
                       const Options& opt,
                       QString* errMsg) {
    if (X.isEmpty()) { if (errMsg) *errMsg = "训练数据为空。"; return false; }
    if (X.size() != y.size()) { if (errMsg) *errMsg = "X 和 y 数量不一致。"; return false; }
    const int n = model.inputDim();
    for (int i = 0; i < X.size(); ++i) {
        if (X[i].size() != n) { if (errMsg) *errMsg = "输入特征维度不一致（可能是预处理配置不同）。"; return false; }
    }

    QVector<int> order(X.size());
    for (int i = 0; i < order.size(); ++i) order[i] = i;

    for (int e = 0; e < opt.epochs; ++e) {
        if (opt.shuffle) shuffle_indices(order);

        for (int k = 0; k < order.size(); ++k) {
            const int i = order[k];
            const auto& x = X[i];
            const float yt = y[i];

            float yhat = model.forward(x);
            float dL_dy = (yhat - yt);
            float dy_dz = yhat * (1.0f - yhat);
            float dL_dz = dL_dy * dy_dz;

            Eigen::VectorXf grad_w = dL_dz * x;
            if (opt.l2 > 0.0f) grad_w += opt.l2 * model.w;
            float grad_b = dL_dz;

            model.w -= opt.lr * grad_w;
            model.b -= opt.lr * grad_b;
        }
    }
    return true;
}

// --------- ModelIO ----------
bool ModelIO::save(const SigmoidRegressor& model, const QString& path, QString* errMsg) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { if (errMsg) *errMsg = "无法写入权重文件：" + path; return false; }

    QDataStream out(&f);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);

    const qint32 dim = model.inputDim();
    out << dim;
    for (int i = 0; i < dim; ++i) out << model.w[i];
    out << model.b;

    if (out.status() != QDataStream::Ok) { if (errMsg) *errMsg = "写入权重文件失败（stream error）。"; return false; }
    if (!f.commit()) { if (errMsg) *errMsg = "保存权重文件失败（commit failed）。"; return false; }
    return true;
}

bool ModelIO::load(SigmoidRegressor& model, const QString& path, QString* errMsg) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { if (errMsg) *errMsg = "无法打开权重文件：" + path; return false; }

    QDataStream in(&f);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    qint32 dim = 0;
    in >> dim;
    if (dim <= 0) { if (errMsg) *errMsg = "权重文件维度非法。"; return false; }

    model = SigmoidRegressor(dim);
    for (int i = 0; i < dim; ++i) { float v = 0.0f; in >> v; model.w[i] = v; }
    float b = 0.0f; in >> b; model.b = b;

    if (in.status() != QDataStream::Ok) { if (errMsg) *errMsg = "读取权重文件失败（stream error）。"; return false; }
    return true;
}

// --------- HouseIntentModel ----------
HouseIntentModel::HouseIntentModel(Config cfg)
    : cfg_(cfg), pp_(cfg.pp) {}

HouseIntentModel::HouseIntentModel(const QString& cfgPath, QStringList* logs, QString* errMsg)
    : cfg_(defaultConfig()), pp_(cfg_.pp) {
    Config loaded;
    QString e;
    loadConfigFromTxt(cfgPath, &loaded, logs, &e);
    if (!e.isEmpty()) logLine(logs, "[ML][Config] " + e);
    cfg_ = loaded;
    pp_ = Preprocessor(cfg_.pp);
}

Eigen::VectorXf HouseIntentModel::toEigen(const QVector<float>& v) {
    Eigen::VectorXf x(v.size());
    for (int i = 0; i < v.size(); ++i) x[i] = v[i];
    return x;
}

bool HouseIntentModel::trainMode(const QVector<QVector<QString>>& rawSamples,
                                 const QVector<float>& labels,
                                 const QString& weightPath,
                                 QString* errMsg) {
    if (rawSamples.isEmpty()) { if (errMsg) *errMsg = "训练样本为空。"; return false; }
    if (rawSamples.size() != labels.size()) { if (errMsg) *errMsg = "训练样本数与标签数不一致。"; return false; }

    QVector<Eigen::VectorXf> X;
    X.reserve(rawSamples.size());

    for (int i = 0; i < rawSamples.size(); ++i) {
        QVector<float> feat = pp_.transform(rawSamples[i]);
        if (feat.size() != pp_.featureDim()) { if (errMsg) *errMsg = "预处理输出维度异常。"; return false; }
        X.push_back(toEigen(feat));
    }

    SigmoidRegressor model(pp_.featureDim());
    if (!Trainer::trainMSE(model, X, labels, cfg_.trainOpt, errMsg)) return false;
    if (!ModelIO::save(model, weightPath, errMsg)) return false;
    return true;
}

QVector<float> HouseIntentModel::testMode(const QVector<QVector<QString>>& rawSamples,
                                          const QString& weightPath,
                                          QString* errMsg) const {
    QVector<float> preds;
    preds.reserve(rawSamples.size());

    SigmoidRegressor model;
    if (!ModelIO::load(model, weightPath, errMsg)) return preds;

    if (model.inputDim() != pp_.featureDim()) {
        if (errMsg) *errMsg = "权重维度与当前预处理维度不匹配（可能 max_room 等配置变了）。";
        return QVector<float>();
    }

    for (const auto& fields : rawSamples) {
        QVector<float> feat = pp_.transform(fields);
        preds.push_back(model.predict(toEigen(feat)));
    }
    return preds;
}

QVector<QPair<int, float>> HouseIntentModel::testModeIndexed(const QVector<QVector<QString>>& rawSamples,
                                                             const QString& weightPath,
                                                             QString* errMsg) const {
    QVector<QPair<int, float>> out;
    out.reserve(rawSamples.size());

    SigmoidRegressor model;
    if (!ModelIO::load(model, weightPath, errMsg)) return out;

    if (model.inputDim() != pp_.featureDim()) {
        if (errMsg) *errMsg = "权重维度与当前预处理维度不匹配（可能 max_room 等配置变了）。";
        return QVector<QPair<int, float>>();
    }

    for (int i = 0; i < rawSamples.size(); ++i) {
        QVector<float> feat = pp_.transform(rawSamples[i]);
        out.push_back(qMakePair(i, model.predict(toEigen(feat))));
    }
    return out;
}

QVector<QVector<QString>> HouseIntentModel::normalizeSamplesForPreprocessor(const QVector<QVector<QString>>& rawSamples,
                                                                            const Preprocessor::Config& ppCfg,
                                                                            QStringList* logs) {
    QVector<QVector<QString>> out;
    out.reserve(rawSamples.size());

    for (int i = 0; i < rawSamples.size(); ++i) {
        const auto& s = rawSamples[i];
        if (s.size() != 6) {
            logLine(logs, QString("[ML][%1] 样本字段数=%2（应为6） -> 全0").arg(i).arg(s.size()));
            out.push_back({"", "", "", "", "", ""});
            continue;
        }

        QString price = normalizeTotalPriceWan(s[0], logs, i);
        QString layout = normalizeLayout(s[1], ppCfg, logs, i);
        QString meterPrice = normalizeMeterPriceYuan(s[2], logs, i);
        QString floor = normalizeFloor(s[3], logs, i);
        QString ori = normalizeOrientation(s[4], logs, i);
        QString year = normalizeYear(s[5], ppCfg, logs, i);

        out.push_back({price, layout, meterPrice, floor, ori, year});
    }
    return out;
}

QVector<QPair<int, float>> HouseIntentModel::testModeIndexedAutoNormalize(const QVector<QVector<QString>>& rawSamples,
                                                                          const QString& weightPath,
                                                                          QStringList* logs,
                                                                          QString* errMsg) const {
    auto normalized = normalizeSamplesForPreprocessor(rawSamples, cfg_.pp, logs);
    return testModeIndexed(normalized, weightPath, errMsg);
}

bool HouseIntentModel::trainModeAutoNormalize(const QVector<QVector<QString>>& rawSamples,
                                              const QVector<float>& labels,
                                              const QString& weightPath,
                                              QStringList* logs,
                                              QString* errMsg) {
    auto normalized = normalizeSamplesForPreprocessor(rawSamples, cfg_.pp, logs);
    return trainMode(normalized, labels, weightPath, errMsg);
}
