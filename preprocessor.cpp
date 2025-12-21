#include "preprocessor.h"
#include <QRegularExpression>
#include <QtGlobal>
#include <cmath>

Preprocessor::Preprocessor(Config cfg) : cfg_(cfg) {}

int Preprocessor::featureDim() const {
    const int R = cfg_.max_room * cfg_.max_room;
    // 1(总价) + R(户型) + 1(每平米) + 3(低中高) + 1(总层数) + 8(朝向) + 1(年份)
    return 1 + R + 1 + 3 + 1 + 8 + 1;
}

QVector<float> Preprocessor::transform(const QVector<QString>& fields) const {
    QVector<float> feat(featureDim(), 0.0f);

    if (fields.size() != 6) {
        // 输入不对就返回全零，避免崩
        return feat;
    }

    const int R = cfg_.max_room * cfg_.max_room;

    // 0 总价
    feat[0] = parseWanPriceNorm(fields[0]);

    // 1..R 户型 one-hot
    encodeLayoutOneHot(fields[1], feat, 1);

    // 1+R 每平米价格
    feat[1 + R] = parseMeterPriceNorm(fields[2]);

    // 2+R..4+R 楼层低中高 one-hot + 5+R 总层数归一化
    encodeFloor(fields[3], feat, 2 + R);

    // 6+R..13+R 朝向 one-hot
    encodeOrientationOneHot(fields[4], feat, 6 + R);

    // 14+R 年份归一化
    feat[14 + R] = parseYearNorm(fields[5]);

    return feat;
}

float Preprocessor::clamp01(float v) {
    if (std::isnan(v) || std::isinf(v)) return 0.0f;
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float Preprocessor::parseWanPriceNorm(const QString& s) const {
    // 支持 "123万" "123.5万" "约123万" 等：提取第一个数字
    QRegularExpression re(R"(([0-9]+(?:\.[0-9]+)?))");
    auto m = re.match(s);
    if (!m.hasMatch() || cfg_.max_prices <= 0.0f) return 0.0f;

    float value = m.captured(1).toFloat(); // 单位：万
    return clamp01(value / cfg_.max_prices);
}

void Preprocessor::encodeLayoutOneHot(const QString& s, QVector<float>& out, int offset) const {
    // 解析 "p室q厅"
    // one-hot 维度：max_room^2
    // 索引：(p-1)*max_room + (q-1)
    if (cfg_.max_room <= 0) return;

    QRegularExpression re(R"((\d+)\s*室\s*(\d+)\s*厅)");
    auto m = re.match(s);
    if (!m.hasMatch()) return;

    int p = m.captured(1).toInt();
    int q = m.captured(2).toInt();
    if (p < 1 || p > cfg_.max_room || q < 1 || q > cfg_.max_room) return;

    int idx = (p - 1) * cfg_.max_room + (q - 1);
    int R = cfg_.max_room * cfg_.max_room;
    if (offset + idx >= 0 && offset + idx < offset + R && offset + idx < out.size()) {
        out[offset + idx] = 1.0f;
    }
}

float Preprocessor::parseMeterPriceNorm(const QString& s) const {
    // 提取第一个数字即可，比如 "12000元/㎡" "12000 元/平" "1.2万/㎡"（如果你数据会出现“万”，需要另加规则）
    QRegularExpression re(R"(([0-9]+(?:\.[0-9]+)?))");
    auto m = re.match(s);
    if (!m.hasMatch() || cfg_.max_meter_price <= 0.0f) return 0.0f;

    float value = m.captured(1).toFloat();
    return clamp01(value / cfg_.max_meter_price);
}

void Preprocessor::encodeFloor(const QString& s, QVector<float>& out, int offset) const {
    // 输出 4 维：
    // offset+0..2: 低/中/高 one-hot
    // offset+3: Y/max_floor
    // 例如 "低楼层（共18层）"
    // 兼容中英文括号
    QString t = s;
    t.remove(' ');

    // 低/中/高
    if (t.contains("低")) out[offset + 0] = 1.0f;
    else if (t.contains("中")) out[offset + 1] = 1.0f;
    else if (t.contains("高")) out[offset + 2] = 1.0f;

    // 提取 Y
    QRegularExpression re(R"(共\s*(\d+)\s*层)");
    auto m = re.match(t);
    if (!m.hasMatch() || cfg_.max_floor <= 0.0f) {
        out[offset + 3] = 0.0f;
        return;
    }
    float Y = m.captured(1).toFloat();
    out[offset + 3] = clamp01(Y / cfg_.max_floor);
}

void Preprocessor::encodeOrientationOneHot(const QString& s, QVector<float>& out, int offset) const {
    // 8 类顺序：东 南 西 北 东南 东北 西南 西北
    // 输入就是其中一个（QString）
    QString t = s;
    t.remove(' ');

    static const QStringList dirs = {"东","南","西","北","东南","东北","西南","西北"};
    for (int i = 0; i < dirs.size(); ++i) {
        if (t == dirs[i]) {
            out[offset + i] = 1.0f;
            return;
        }
    }
    // 未匹配则全 0
}

float Preprocessor::parseYearNorm(const QString& s) const {
    QRegularExpression re(R"((\d{4}))");
    auto m = re.match(s);
    if (!m.hasMatch()) return 0.0f;

    int year = m.captured(1).toInt();
    if (cfg_.max_year <= cfg_.min_year) return 0.0f;

    float v = float(year - cfg_.min_year) / float(cfg_.max_year - cfg_.min_year);
    return clamp01(v);
}
