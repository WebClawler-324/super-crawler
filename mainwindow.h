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

private:
    Ui::MainWindow *ui;
    Crawl* m_crawl;
    int targetPageCount;
    QString currentCity;
    void setImage(QString,QLabel *);

};

#endif // MAINWINDOW_H
