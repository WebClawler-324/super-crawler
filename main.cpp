#include <QApplication>
#include <QDebug>
#include <QSqlDatabase>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置Qt调试插件环境变量（可选，用于调试数据库插件）
    qputenv("QT_DEBUG_PLUGINS", "1");

    // 触发数据库驱动扫描
    QSqlDatabase::drivers();

    // 输出可用的数据库驱动（调试信息）
    qDebug() << "Available database drivers:" << QSqlDatabase::drivers();

    // 创建主窗口
    MainWindow w;
    w.show();

    return a.exec();
}
