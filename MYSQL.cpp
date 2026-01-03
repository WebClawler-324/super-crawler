#include"MYSQL.h"
#include<QSqlQuery>
#include<QList>
#include<QVariant>
#include<QDir>
#include<QCoreApplication>

Mysql::Mysql(){
    QString connName = QString("WebCrawl_%1").arg((quintptr)this); // 唯一连接名
    db = QSqlDatabase::addDatabase("QMYSQL", connName);
}


Mysql::~Mysql(){
    db.close();
}

void Mysql::connectDatabase(){
    db.setHostName("localhost");
    db.setDatabaseName("HouseDB");
    db.setUserName("root");
    db.setPassword("qwqwasas25205817");
    db.setPort(3306);

    qDebug() << "=== MySQL Connection Debug Info ===";
    qDebug() << "Qt可用数据库驱动：" << QSqlDatabase::drivers();
    qDebug() << "QMYSQL driver available:" << QSqlDatabase::drivers().contains("QMYSQL");

    // 检查插件路径
    QString pluginPath = QCoreApplication::applicationDirPath() + "/../plugins/sqldrivers";
    qDebug() << "Plugin path:" << pluginPath;
    QDir pluginDir(pluginPath);
    if (pluginDir.exists()) {
        QStringList dllFiles = pluginDir.entryList(QStringList() << "*mysql*.dll", QDir::Files);
        qDebug() << "MySQL DLL files found:" << dllFiles;
    } else {
        qDebug() << "Plugin directory does not exist!";
    }

    // 检查系统PATH中的MySQL库
    QStringList pathDirs = QString(qgetenv("PATH")).split(";");
    qDebug() << "Checking PATH for MySQL libraries...";
    for (const QString& dir : pathDirs) {
        if (dir.contains("mysql", Qt::CaseInsensitive)) {
            qDebug() << "Found MySQL in PATH:" << dir;
        }
    }

    // 3. 打开连接并检查结果
    if (db.open()) {
        qDebug() << "MySQL连接成功！";
    } else {
        qDebug() << "MySQL连接失败：" << db.lastError().text();
        qDebug() << "Error type:" << db.lastError().type();
        qDebug() << "Database error:" << db.lastError().databaseText();
        qDebug() << "Driver error:" << db.lastError().driverText();
    }
}

void Mysql::close(){
    db.close();
}

void Mysql::insertInfo(const HouseData &data){
    QString sql = R"(
        INSERT INTO houseinfo (houseTitle,communityName, price, unitPrice,
                            houseType, area, floor, orientation, buildingYear, houseUrl)
        VALUES (:houseTitle,:communityName, :price, :unitPrice,
                :houseType, :area, :floor, :orientation, :buildingYear, :houseUrl)
    )";

    //准备SQL查询
    QSqlQuery query(db);  //显示绑定数据库
    if (!query.prepare(sql)) {
        qWarning() << "SQL准备失败：" << query.lastError().text();
    }

    // 数据清理和转换
    double priceValue = -1.0;
    if(data.price != "未知" && !data.price.isEmpty()){
        QString cleanPrice = data.price;
        cleanPrice = cleanPrice.remove("万").remove(",").trimmed();
        priceValue = cleanPrice.toDouble();
    }

    double unitPriceValue = -1.0;
    if(data.unitPrice != "未知" && !data.unitPrice.isEmpty()){
        QString cleanUnitPrice = data.unitPrice;
        cleanUnitPrice = cleanUnitPrice.remove("元/㎡").remove(",").trimmed();
        unitPriceValue = cleanUnitPrice.toDouble();
    }

    int yearValue = -1;
    if(data.buildingYear != "未知" && !data.buildingYear.isEmpty()){
        QString cleanYear = data.buildingYear;
        cleanYear = cleanYear.remove("年").remove(",").trimmed();
        yearValue = cleanYear.toInt();
    }

    double areaValue = -1.0;
    if(data.area != "未知" && !data.area.isEmpty()){
        QString cleanArea = data.area;
        cleanArea = cleanArea.remove("㎡").remove(",").trimmed();
        areaValue = cleanArea.toDouble();
    }
    //绑定数据（参数名对应SQL中的:xxx）
    query.bindValue(":houseTitle", data.houseTitle);
    query.bindValue(":communityName", data.communityName);
    query.bindValue(":price", priceValue);
    query.bindValue(":unitPrice", unitPriceValue);
    query.bindValue(":houseType", data.houseType);
    query.bindValue(":area", areaValue);
    query.bindValue(":floor", data.floor);
    query.bindValue(":orientation", data.orientation);
    query.bindValue(":buildingYear", yearValue);
    query.bindValue(":houseUrl", data.houseUrl);

    qDebug() << "插入数据：" << data.communityName << "价格:" << priceValue << "元";

    //执行插入
    if (!query.exec()) {
        qWarning() << "数据插入失败：" << query.lastError().text();
    }else{
       qDebug() << "数据插入成功！";
    }
}

QList<HouseData> Mysql::getAllHouseData()
{
    QList<HouseData> houseDataList;

    QString sql = "SELECT houseTitle, communityName, price, unitPrice, houseType, "
                  "area, floor, orientation, buildingYear, houseUrl FROM houseinfo";

    QSqlQuery query(db);
    if (!query.exec(sql)) {
        qWarning() << "查询房源数据失败：" << query.lastError().text();
        return houseDataList;
    }

    while (query.next()) {
        HouseData data;

        // 从数据库读取数据并转换回字符串格式
        data.houseTitle = query.value(0).toString();
        data.communityName = query.value(1).toString();

        // 价格转换回字符串格式
        double price = query.value(2).toDouble();
        data.price = (price > 0) ? QString::number(price) + "万" : "未知";

        double unitPrice = query.value(3).toDouble();
        data.unitPrice = (unitPrice > 0) ? QString::number(unitPrice) + "元/㎡" : "未知";

        data.houseType = query.value(4).toString();

        double area = query.value(5).toDouble();
        data.area = (area > 0) ? QString::number(area) + "㎡" : "未知";

        data.floor = query.value(6).toString();
        data.orientation = query.value(7).toString();

        int year = query.value(8).toInt();
        data.buildingYear = (year > 0) ? QString::number(year) + "年" : "未知";

        data.houseUrl = query.value(9).toString();

        houseDataList.append(data);
    }

    qDebug() << "从数据库获取到" << houseDataList.size() << "条房源数据";
    return houseDataList;
}

// 按价格范围查询房源
QList<HouseData> Mysql::findHousesByPrice(double minPrice, double maxPrice)
{
    QList<HouseData> houseDataList;

    QString sql = "SELECT houseTitle, communityName, price, unitPrice, houseType, "
                  "area, floor, orientation, buildingYear, houseUrl FROM houseinfo "
                  "WHERE price >= ? AND price <= ?";

    QSqlQuery query(db);
    query.prepare(sql);
    query.addBindValue(minPrice);
    query.addBindValue(maxPrice);

    if (!query.exec()) {
        qWarning() << "按价格查询房源失败：" << query.lastError().text();
        return houseDataList;
    }

    while (query.next()) {
        HouseData data = createHouseDataFromQuery(query);
        houseDataList.append(data);
    }

    qDebug() << "按价格查询到" << houseDataList.size() << "条房源数据";
    return houseDataList;
}

// 按户型查询房源
QList<HouseData> Mysql::findHousesByType(const QString& houseType)
{
    QList<HouseData> houseDataList;

    QString sql = "SELECT houseTitle, communityName, price, unitPrice, houseType, "
                  "area, floor, orientation, buildingYear, houseUrl FROM houseinfo "
                  "WHERE houseType LIKE ?";

    QSqlQuery query(db);
    query.prepare(sql);
    query.addBindValue("%" + houseType + "%");

    if (!query.exec()) {
        qWarning() << "按户型查询房源失败：" << query.lastError().text();
        return houseDataList;
    }

    while (query.next()) {
        HouseData data = createHouseDataFromQuery(query);
        houseDataList.append(data);
    }

    qDebug() << "按户型查询到" << houseDataList.size() << "条房源数据";
    return houseDataList;
}

// 按面积范围查询房源
QList<HouseData> Mysql::findHousesByArea(double minArea, double maxArea)
{
    QList<HouseData> houseDataList;

    QString sql = "SELECT houseTitle, communityName, price, unitPrice, houseType, "
                  "area, floor, orientation, buildingYear, houseUrl FROM houseinfo "
                  "WHERE area >= ? AND area <= ?";

    QSqlQuery query(db);
    query.prepare(sql);
    query.addBindValue(minArea);
    query.addBindValue(maxArea);

    if (!query.exec()) {
        qWarning() << "按面积查询房源失败：" << query.lastError().text();
        return houseDataList;
    }

    while (query.next()) {
        HouseData data = createHouseDataFromQuery(query);
        houseDataList.append(data);
    }

    qDebug() << "按面积查询到" << houseDataList.size() << "条房源数据";
    return houseDataList;
}

// 按价格和户型查询房源
QList<HouseData> Mysql::findHousesByPriceAndType(double minPrice, double maxPrice, const QString& houseType)
{
    QList<HouseData> houseDataList;

    QString sql = "SELECT houseTitle, communityName, price, unitPrice, houseType, "
                  "area, floor, orientation, buildingYear, houseUrl FROM houseinfo "
                  "WHERE price >= ? AND price <= ? AND houseType LIKE ?";

    QSqlQuery query(db);
    query.prepare(sql);
    query.addBindValue(minPrice);
    query.addBindValue(maxPrice);
    query.addBindValue("%" + houseType + "%");

    if (!query.exec()) {
        qWarning() << "按价格和户型查询房源失败：" << query.lastError().text();
        return houseDataList;
    }

    while (query.next()) {
        HouseData data = createHouseDataFromQuery(query);
        houseDataList.append(data);
    }

    qDebug() << "按价格和户型查询到" << houseDataList.size() << "条房源数据";
    return houseDataList;
}

// 辅助函数：从查询结果创建HouseData对象
HouseData Mysql::createHouseDataFromQuery(QSqlQuery& query)
{
    HouseData data;

    data.houseTitle = query.value(0).toString();
    data.communityName = query.value(1).toString();

    double price = query.value(2).toDouble();
    data.price = (price > 0) ? QString::number(price) + "万" : "未知";

    double unitPrice = query.value(3).toDouble();
    data.unitPrice = (unitPrice > 0) ? QString::number(unitPrice) + "元/㎡" : "未知";

    data.houseType = query.value(4).toString();

    double area = query.value(5).toDouble();
    data.area = (area > 0) ? QString::number(area) + "㎡" : "未知";

    data.floor = query.value(6).toString();
    data.orientation = query.value(7).toString();

    int year = query.value(8).toInt();
    data.buildingYear = (year > 0) ? QString::number(year) + "年" : "未知";

    data.houseUrl = query.value(9).toString();

    return data;
}




