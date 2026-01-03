#ifndef MYSQL_H
#define MYSQL_H

#include<QSqlDatabase>
#include <QSqlError>
#include"HouseData.h"

class Mysql{

public:
    Mysql();
    ~Mysql();
     void connectDatabase();
     void close();
     void insertInfo(const HouseData &data);
     QList<HouseData> getAllHouseData();  // 新增：获取所有房源数据

     // AI对话查询方法
     QList<HouseData> findHousesByPrice(double minPrice, double maxPrice);  // 按价格范围查询
     QList<HouseData> findHousesByType(const QString& houseType);          // 按户型查询
     QList<HouseData> findHousesByArea(double minArea, double maxArea);    // 按面积范围查询
     QList<HouseData> findHousesByPriceAndType(double minPrice, double maxPrice, const QString& houseType);  // 按价格和户型查询

private:
     // 辅助函数
     HouseData createHouseDataFromQuery(QSqlQuery& query);
    QSqlDatabase db;



};
#endif // MYSQL_H
