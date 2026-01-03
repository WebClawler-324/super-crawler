#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWebEnginePage>
#include <QQueue>
#include <QSet>
#include <QMap>
#include <QList>
#include <QStringList>
#include<QLabel>
#include "Crawl.h"
#include "AliCrawl.h"
#include <QtCharts>
#include <QWebEngineProfile>    // 对应 customProfile
#include <QWebEnginePage>      // 对应 customPage
#include <QWebEngineView>      // 对应 webView
#include <QWebEngineSettings>  // 对应 settings

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_pushButton_clicked();                        // 启动
    void on_searchCompareBtn_clicked();                  // 搜索对比按钮（城市二手房）
    void updateLog(const QString& log);
    void AliLog(const QString &log);
    void on_analyzeBtn_clicked();                        // AI分析按钮
    void on_clearAnalysisBtn_clicked();                  // 清空分析按钮
    void on_askQuestionBtn_clicked();                    // 询问按钮
    void onAnalysisCompleted(const QString& result);     // AI分析完成回调
    void onQuestionAnswered(const QString& result);     // AI问答完成回调
    // 接收日志并显示到UI
  //  void showLog(const QString& log);
    void on_searchCompareBtn_2_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_searchCompareBtn_3_clicked();

    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

    void on_FininMap_clicked();

    void on_Back_clicked();

    void on_pushButton_6_clicked();

    void on_pushButton_7_clicked();

    void on_pushButton_8_clicked();

    void on_Select_clicked();

    void on_Back_2_clicked();

    void on_FininMap_2_clicked();

private:
    Ui::MainWindow *ui;
    QChart *pieChart;   // 饼图
    QChart *barChart;   // 柱状图
     Mysql *mysql;

    Crawl* m_crawl;
    AliCrawl* a_crawl;
    // 辅助函数：创建独立的 QWebEnginePage（每个爬虫单独用，避免冲突）
    QWebEnginePage* createWebEnginePage();

    QWebEngineView *webView;    // Web视图（显示地图）
    QWebEngineProfile *customProfile;
    QWebEnginePage *customPage;
    bool mapPageLoaded = false;

    int targetPageCount;
    QString currentCity;
    //存储表格数据
    QList<QStringList> tableOriginalData;
    //填充表格
    void fillTableWithData(const QList<QStringList> &data);

    void setImage(QString,QLabel *);
    //饼状图生成
    void generateBin();
    //柱状图生成
    void generateZhu();
    //数据表
    void generateTabel();
    //大模型推荐
    void ModelSuggest();
    //地图
    void displayMap();
    //找房
    void findHouse(QString);
    //筛选
    void filterTableData();
    //AI Jason数组
    QJsonArray houseDataArray;


};

#endif // MAINWINDOW_H
