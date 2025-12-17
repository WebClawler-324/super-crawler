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
private:
    QSqlDatabase db;



};
#endif // MYSQL_H
