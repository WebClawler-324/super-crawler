#include"MYSQL.h"
#include<QSqlQuery>

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
    db.setUserName("crawler_user_1");
    db.setPassword("123");
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





