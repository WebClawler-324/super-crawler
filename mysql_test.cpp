#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>
#include <QPluginLoader>
#include <QDir>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qDebug() << "=== Qt MySQL Driver Test ===";

    // 1. 检查可用驱动
    qDebug() << "Available drivers:" << QSqlDatabase::drivers();

    // 2. 检查MySQL驱动是否存在
    bool mysqlDriverAvailable = QSqlDatabase::drivers().contains("QMYSQL");
    qDebug() << "QMYSQL driver available:" << mysqlDriverAvailable;

    if (!mysqlDriverAvailable) {
        // 3. 手动加载MySQL驱动
        qDebug() << "Trying to load QMYSQL driver manually...";

        // 检查插件目录
        QString pluginPath = QCoreApplication::applicationDirPath() + "/../plugins/sqldrivers";
        qDebug() << "Plugin path:" << pluginPath;

        QDir pluginDir(pluginPath);
        QStringList dllFiles = pluginDir.entryList(QStringList() << "*.dll", QDir::Files);
        qDebug() << "DLL files in plugin directory:" << dllFiles;

        // 尝试加载qsqlmysql.dll
        QPluginLoader loader(pluginPath + "/qsqlmysql.dll");
        qDebug() << "Plugin loader error:" << loader.errorString();

        QObject *plugin = loader.instance();
        if (plugin) {
            qDebug() << "Plugin loaded successfully";
        } else {
            qDebug() << "Failed to load plugin";
        }
    }

    // 4. 尝试创建MySQL连接
    qDebug() << "Trying to create MySQL database connection...";
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");

    db.setHostName("localhost");
    db.setDatabaseName("HouseDB");
    db.setUserName("root");
    db.setPassword("qwqwasas25205817");
    db.setPort(3306);

    if (db.open()) {
        qDebug() << "MySQL connection successful!";
        db.close();
    } else {
        qDebug() << "MySQL connection failed:" << db.lastError().text();
        qDebug() << "Error type:" << db.lastError().type();
        qDebug() << "Database error:" << db.lastError().databaseText();
        qDebug() << "Driver error:" << db.lastError().driverText();
    }

    // 5. 检查系统环境
    qDebug() << "=== System Environment ===";
    qDebug() << "Qt version:" << QT_VERSION_STR;
    qDebug() << "Application dir:" << QCoreApplication::applicationDirPath();

    return 0;
}

