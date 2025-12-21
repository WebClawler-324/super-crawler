#pragma once
#include <QString>
#include <QVector>

/**
 * 输入 6 个字段（按 0 索引顺序）：
 * 0: 总价格 "XXX万"
 * 1: 户型   "p室q厅"
 * 2: 每平方价格 "xxxx元/㎡"（或带任意单位）
 * 3: 楼层   "低楼层（共Y层）" / "中楼层（共Y层）" / "高楼层（共Y层）"
 * 4: 朝向   "东/南/西/北/东南/东北/西南/西北"
 * 5: 建造年代 "XXXX年"
 *
 * 输出特征向量顺序（你可以固定下来供模型使用）：
 * [0]          总价格归一化
 * [1..R]       户型 one-hot（R = max_room^2）
 * [1+R]        每平米价格归一化
 * [2+R..4+R]   楼层档位 one-hot（低/中/高）
 * [5+R]        总楼层数归一化（Y/max_floor）
 * [6+R..13+R]  朝向 one-hot（8维，顺序：东 南 西 北 东南 东北 西南 西北）
 * [14+R]       建造年代归一化
 */
class Preprocessor {
public:
    struct Config {
        float max_prices = 500.0f;        // 价格“万”为单位的最大值，例如 500 万
        int   max_room = 5;               // 户型最大室/厅数（one-hot 用 max_room^2）
        float max_floor = 60.0f;          // 总楼层 Y 的最大值
        float max_meter_price = 200000.0f;// 每平米价格的最大值（按你数据量级设）
        int   min_year = 1900;
        int   max_year = 2025;
    };

    explicit Preprocessor(Config cfg);

    int featureDim() const; // 输出维度

    // 输入必须是长度=6的 QVector<QString>（按你定义顺序）
    QVector<float> transform(const QVector<QString>& fields) const;

private:
    Config cfg_;

    static float clamp01(float v);

    // 解析工具
    float parseWanPriceNorm(const QString& s) const;          // "XXX万" -> 0..1
    void  encodeLayoutOneHot(const QString& s, QVector<float>& out, int offset) const; // "p室q厅"
    float parseMeterPriceNorm(const QString& s) const;        // 提取数字 -> /max_meter_price
    void  encodeFloor(const QString& s, QVector<float>& out, int offset) const;        // 低/中/高 + Y/max_floor
    void  encodeOrientationOneHot(const QString& s, QVector<float>& out, int offset) const; // 8 类
    float parseYearNorm(const QString& s) const;              // "XXXX年" -> 0..1
};
