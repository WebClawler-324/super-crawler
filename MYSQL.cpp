#include"MYSQL.h"
#include<QSqlQuery>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>  // 若用到JSON文档解析，可一并包含
#include <QJsonObject>
Mysql::Mysql(){
    QString connName = QString("WebCrawl_%1").arg((quintptr)this); // 唯一连接名
    db = QSqlDatabase::addDatabase("QMYSQL", connName);
}


Mysql::~Mysql(){
    db.close();
}

void Mysql::connectDatabase(){
    /*db.setHostName("localhost");
    db.setDatabaseName("HouseDB");
    db.setUserName("root");
    db.setPassword("qwqwasas25205817");
    db.setPort(3306);*/
    db.setHostName("rm-2zepql94a2hcect0vvo.mysql.rds.aliyuncs.com");
    db.setDatabaseName("House_DB");
    db.setUserName("action_2");
    db.setPassword("123456");
    db.setPort(3306);
qDebug() << "Qt可用数据库驱动：" << QSqlDatabase::drivers();
    // 3. 打开连接并检查结果
    if (db.open()) {
        qDebug() << "MySQL连接成功！";
    } else {
        qDebug() << "MySQL连接失败：" << db.lastError().text();
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

    QString price = data.price;
    if(price=="未知"){
        price="-1";
        price.toDouble();
    }else{
      price.remove("万").remove(",").trimmed().toDouble();
    }


    QString unitPrice=data.unitPrice;
    if(unitPrice=="未知"){
        unitPrice="-1";
        unitPrice.toDouble();
    }else{
       unitPrice.remove("元/㎡").remove(",").trimmed().toDouble();
    }


    QString year=data.buildingYear;
    if(year=="未知"){
        year="-1";
        year.toDouble();
    }else{
        year.remove("年建造").remove(",").trimmed().toInt();
    }


    QString area=data.area;
    if(area=="未知"){
        area="-1";
        area.toDouble();
    }else{
        area.remove("㎡").remove(",").trimmed().toDouble();
    }
    qDebug()<<"小区标题"<<data.houseTitle;
    qDebug()<<"小区名"<<data.communityName;
    //绑定数据（参数名对应SQL中的:xxx）
    query.bindValue(":houseTitle", data.houseTitle);
    query.bindValue(":communityName", data.communityName);
    query.bindValue(":price",price);
    query.bindValue(":unitPrice",unitPrice);
    query.bindValue(":houseType", data.houseType);
    query.bindValue(":area",area);
    query.bindValue(":floor", data.floor);
    query.bindValue(":orientation", data.orientation);
    query.bindValue(":buildingYear", year);
    query.bindValue(":houseUrl", data.houseUrl);

    //执行插入
    if (!query.exec()) {
        qWarning() << "数据插入失败：" << query.lastError().text();
    }else{
       qDebug() << "数据插入成功！";
    }
}


void Mysql::insertAlInfo(const HouseInfo &data){
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

    QString price = data.price;
    if(price=="未知"){
        price="-1";
        price.toDouble();
    }else{
        price.remove("万").remove(",").trimmed().toDouble();
    }


    QString unitPrice=data.unitPrice;
    if(unitPrice=="计算失败"){
        unitPrice="-1";
        unitPrice.toDouble();
    }else{
        unitPrice.remove("元/㎡").remove(",").trimmed().toDouble();
    }


    QString year=data.buildingYear;
    if(year=="未知"){
        year="-1";
        year.toDouble();
    }else{
        year.remove("年").remove(",").trimmed().toInt();
    }


    QString area=data.area;
    if(area=="未知"){
        area="-1";
        area.toDouble();
    }else{
        area.remove("㎡").remove(",").trimmed().toDouble();
    }
    qDebug()<<"小区标题"<<data.houseTitle;
    qDebug()<<"小区名"<<data.communityName;
    //绑定数据（参数名对应SQL中的:xxx）
    query.bindValue(":houseTitle", data.houseTitle);
    query.bindValue(":communityName", data.communityName);
    query.bindValue(":price",price);
    query.bindValue(":unitPrice",unitPrice);
    query.bindValue(":houseType", data.houseType);
    query.bindValue(":area",area);
    query.bindValue(":floor", data.floor);
    query.bindValue(":orientation", data.orientation);
    query.bindValue(":buildingYear", year);
    query.bindValue(":houseUrl", data.houseUrl);

    //执行插入
    if (!query.exec()) {
        qWarning() << "数据插入失败：" << query.lastError().text();
    }else{
        qDebug() << "数据插入成功！";
    }
}

QVector<QVector<QString>> Mysql::getInfo(){
    QVector<QVector<QString>> testSamples;

    QSqlQuery query(db);  //显示绑定数据库
    // SELECT 后面写全你需要的列名（和数据库中一致）
    QString sql = R"(
        SELECT communityName, price, unitPrice,
               houseType, area, floor, orientation, buildingYear, houseUrl
        FROM houseinfo;
    )";
    if (!query.exec(sql)) {
        qDebug() << "查询失败：" << query.lastError().text();
        db.close();
        return testSamples;
    }
    // 3. 遍历结果集：逐行读取每列数据，转成QString并存入二维向量
    while (query.next()) {  // 遍历每一行记录
        QVector<QString> singleHouse;  // 存储单条房源的所有字段

        // 逐个读取列数据
        singleHouse.append(query.value("communityName").toString());

        singleHouse.append(QString::number(query.value("price").toDouble()));

        singleHouse.append(QString::number(query.value("unitPrice").toDouble()));

        singleHouse.append(query.value("houseType").toString());

        singleHouse.append(QString::number(query.value("area").toDouble()));

        singleHouse.append(query.value("floor").toString());

        singleHouse.append(query.value("orientation").toString());

        if(query.value("buildingYear")==-1){
            singleHouse.append("未知");
        }else{
            singleHouse.append(QString::number(query.value("buildingYear").toInt()));
        }

        singleHouse.append(query.value("houseUrl").toString());

        testSamples.append(singleHouse);
    }

    return testSamples;
}

void  Mysql::getPriceCout(double& two, double & four, double & ufour)
{
    two = 0.0;
    four = 0.0;
    ufour = 0.0;


    if (!db.isOpen()) {
        qDebug() << "数据库未打开，查询失败！";
        return;
    }

    QSqlQuery query(db);

    QString sql = R"(
        SELECT price
        FROM houseinfo
    )";

    // 执行SQL查询
    if (!query.exec(sql)) {
        qDebug() << "查询失败：" << query.lastError().text();
        return;
    }

    double count2=0, count4=0, count5=0, total=0;

    while (query.next()) {
        // 优免重复调用toDouble()，提高效率
        double price = query.value("price").toDouble();
        total++;

        if (price <= 200) {
            count2++;
        } else if (price > 200 && price <= 400) {
            count4++;
        } else if (price > 400) {
            count5++;
        }
    }

    if (total == 0) {
        qDebug() << "数据库表houseinfo中无数据，无需计算百分比！";
        return;
    }
    two = (count2 / total) * 100.0;
    four = (count4 / total) * 100.0;
    ufour = (count5 / total) * 100.0;
}


void Mysql::getAreaCout(double& One,double & Two,double & Three, double & total)
{
    One=0.0;
    Two=0.0;
    Three=0.0;
    total=0.0;

    if (!db.isOpen()) {
        qDebug() << "数据库未打开，查询失败！";
        return;
    }

    QSqlQuery query(db);

    QString sql = R"(
        SELECT area
        FROM houseinfo
    )";

    // 执行SQL查询
    if (!query.exec(sql)) {
        qDebug() << "查询失败：" << query.lastError().text();
        return;
    }

    double count1=0, count2=0, count3=0;

    while (query.next()) {
        // 优免重复调用toDouble()，提高效率
        double price = query.value("area").toDouble();
        total++;

        if (price <= 100) {
            count1++;
        } else if (price >100 && price <=200) {
            count2++;
        } else if (price >200) {
            count3++;
        }
    }

    if (total == 0) {
        qDebug() << "数据库表houseinfo中无数据";
        return;
    }

    One=count1;
    Two=count2;
    Three=count3;
    qDebug()<<One;
    qDebug()<<Two;
    qDebug()<<Three;

}

void Mysql::generateTable(QTableWidget* tableWidget,QList<QStringList> &tableOriginalData){
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，查询失败！";
        return;
    }

    QSqlQuery query(db);

    QString sql = R"(
        SELECT houseTitle,communityName,price,unitPrice,area,houseType,floor,houseUrl
        FROM houseinfo
    )";
    // 执行SQL查询
    if (!query.exec(sql)) {
        qDebug() << "查询失败：" << query.lastError().text();
        return;
    }

    int dataRowCount = 0;
    // 先遍历一次统计行数
    while (query.next())
    {
        dataRowCount++;
    }
    //将查询指针重置到结果集开头，以便重新遍历填充数据
    query.first();
    query.previous();

    qDebug() << "查询到的数据总行数：" << dataRowCount; // 验证是否有数据（核心排查）

    //初始化表格
    tableWidget->setColumnCount(8); // 你要填充8列（0~7），必须先设置列数
    tableWidget->clearContents();   // 清空原有单元格数据（保留表格结构）
    tableOriginalData.clear();
    tableWidget->setRowCount(dataRowCount); // 设置表格行数与数据行数一致

     int currentRow = 0;
    while(query.next()){
         QStringList rowData; // 每次循环新建一个，存储当前行的独立数据

         // 步骤3：按列顺序，填充QStringList（与表格填充顺序完全一致）
         QString col0 = query.value(0).toString().trimmed(); // houseTitle
         QString col1 = query.value(1).toString().trimmed(); // communityName
         QString col2 = query.value(2).toString().trimmed()+"万"; // price
         QString col3 = query.value(3).toString().trimmed()+"元"; // unitPrice
         QString col4 = query.value(4).toString().trimmed()+"平米"; // area
         QString col5 = query.value(5).toString().trimmed(); // houseType
         QString col6 = query.value(6).toString().trimmed(); // floor
         QString col7 = query.value(7).toString().trimmed(); // houseUrl

         // 向rowData中添加8列数据（顺序与表格一致，后续可通过索引精准获取）
         rowData << col0 << col1 << col2 << col3 << col4 << col5 << col6 << col7;

        // 填充其他列（索引1~6）
        tableWidget->setItem(currentRow, 0, new QTableWidgetItem(query.value(0).toString()));
        tableWidget->setItem(currentRow, 1, new QTableWidgetItem(query.value(1).toString()));
        tableWidget->setItem(currentRow, 2, new QTableWidgetItem(query.value(2).toString()+"万"));
        tableWidget->setItem(currentRow, 3, new QTableWidgetItem(query.value(3).toString()+"元"));
        tableWidget->setItem(currentRow, 4, new QTableWidgetItem(query.value(4).toString()+"平米"));
        tableWidget->setItem(currentRow, 5, new QTableWidgetItem(query.value(5).toString()));
        tableWidget->setItem(currentRow, 6, new QTableWidgetItem(query.value(6).toString()));
        tableWidget->setItem(currentRow,7, new QTableWidgetItem(query.value(7).toString()));

        currentRow++;

         // 保存到原始数据容器
         tableOriginalData.append(rowData);
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


//批量转为Jason
void Mysql::getToJas(QJsonArray& houseDataArray){

    QString sql = "SELECT houseTitle, communityName, price, unitPrice, houseType, "
                  "area, floor, orientation, buildingYear, houseUrl FROM houseinfo";

    QSqlQuery query(db);
    if (!query.exec(sql)) {
        qWarning() << "查询房源数据失败：" << query.lastError().text();
    }
    houseDataArray = QJsonArray();
    while (query.next()) {

        QString price= QString::number(query.value(2).toDouble());
        QJsonObject q1;
        q1["房子标题"]=query.value(0).toString();
        q1["小区名"]=query.value(1).toString();
        q1["总价"] = price+"万";
        q1["面积"]=query.value(5).toString()+"平米";
        q1["户型"] = query.value(4).toString();
        q1["单价"] = query.value(3).toString()+"元/㎡";
        q1["楼层"] = query.value(6).toString();
        q1["朝向"] = query.value(7).toString();
        q1["年代"] = query.value(8).toString();
        houseDataArray.append(q1);
    }
}


