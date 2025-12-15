#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWebEnginePage>
#include <QQueue>
#include <QSet>
#include <QMap>
#include <QList>
#include <QStringList>
#include "Crawl.h"

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
    // ===================== 界面按钮槽函数 =====================
    void on_pushButton_clicked();                        // 首页爬取按钮
    void on_searchCompareBtn_clicked();                  // 搜索对比按钮（城市二手房）
    void updateLog(const QString& log);

private:
    // ===================== 界面相关 =====================
    Ui::MainWindow *ui;
    QWebEnginePage *webPage;                             // Web引擎页面
    Crawl* m_crawl;
    int targetPageCount;
    QString currentCity;

};

#endif // MAINWINDOW_H
