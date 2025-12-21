#pragma once
#include <QVector>
#include <QString>
#include <QPair>
#include <QStringList>

#include <Eigen/Dense>
#include "preprocessor.h"

// ---------- Sigmoid Regressor ----------
class SigmoidRegressor {
public:
    SigmoidRegressor() = default;
    explicit SigmoidRegressor(int input_dim);

    int inputDim() const { return static_cast<int>(w.size()); }

    float forward(const Eigen::VectorXf& x) const;  // 0..1
    float predict(const Eigen::VectorXf& x) const { return forward(x); }

    Eigen::VectorXf w;
    float b = 0.0f;
};

// ---------- Trainer ----------
class Trainer {
public:
    struct Options {
        int   epochs = 50;
        float lr = 0.05f;
        float l2 = 0.0f;
        bool  shuffle = true;
    };

    static bool trainMSE(SigmoidRegressor& model,
                         const QVector<Eigen::VectorXf>& X,
                         const QVector<float>& y,
                         const Options& opt,
                         QString* errMsg = nullptr);
};

// ---------- IO ----------
class ModelIO {
public:
    static bool save(const SigmoidRegressor& model, const QString& path, QString* errMsg = nullptr);
    static bool load(SigmoidRegressor& model, const QString& path, QString* errMsg = nullptr);
};

// ---------- Facade: Preprocess + Train/Test ----------
class HouseIntentModel {
public:
    struct Config {
        Preprocessor::Config pp;
        Trainer::Options trainOpt;
    };

    // 默认配置
    static Config defaultConfig();

    // 从 txt 读取配置（在 defaultConfig 基础上覆盖；未知key忽略；非法值回退默认并记录日志）
    static bool loadConfigFromTxt(const QString& path,
                                  Config* outCfg,
                                  QStringList* logs = nullptr,
                                  QString* errMsg = nullptr);

    // 写出一份模板配置文件（便于首次运行生成）
    static bool saveConfigTemplate(const QString& path, QString* errMsg = nullptr);

    // 构造方式 1：直接传 Config
    explicit HouseIntentModel(Config cfg);

    // 构造方式 2：传配置文件路径（失败则回退默认，并在 logs/errMsg 里报告）
    explicit HouseIntentModel(const QString& cfgPath,
                              QStringList* logs = nullptr,
                              QString* errMsg = nullptr);

    int featureDim() const { return pp_.featureDim(); }

    bool trainMode(const QVector<QVector<QString>>& rawSamples,
                   const QVector<float>& labels,
                   const QString& weightPath,
                   QString* errMsg = nullptr);

    QVector<float> testMode(const QVector<QVector<QString>>& rawSamples,
                            const QString& weightPath,
                            QString* errMsg = nullptr) const;

    QVector<QPair<int, float>> testModeIndexed(const QVector<QVector<QString>>& rawSamples,
                                               const QString& weightPath,
                                               QString* errMsg = nullptr) const;

    // 强鲁棒版本：自动规范化 + 错误日志
    QVector<QPair<int, float>> testModeIndexedAutoNormalize(const QVector<QVector<QString>>& rawSamples,
                                                            const QString& weightPath,
                                                            QStringList* logs,
                                                            QString* errMsg = nullptr) const;

    bool trainModeAutoNormalize(const QVector<QVector<QString>>& rawSamples,
                                const QVector<float>& labels,
                                const QString& weightPath,
                                QStringList* logs,
                                QString* errMsg = nullptr);

    // 日志工具（供 cpp 内工具函数调用）
    static void logLine(QStringList* logs, const QString& s);

private:
    Config cfg_;
    Preprocessor pp_;

    static Eigen::VectorXf toEigen(const QVector<float>& v);

    static QVector<QVector<QString>> normalizeSamplesForPreprocessor(const QVector<QVector<QString>>& rawSamples,
                                                                     const Preprocessor::Config& ppCfg,
                                                                     QStringList* logs);
};
