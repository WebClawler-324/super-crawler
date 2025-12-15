#ifndef HOUSDATA_H
#define HOUSDATA_H

#include <QString>

// 房源数据结构（供MainWindow和Crawl共享）
struct HouseData {
    QString houseTitle;      // 房源标题
    QString communityName;   // 小区名称
    QString price;           // 总价
    QString unitPrice;       // 单价
    QString area;            // 面积
    QString houseType;       // 户型（几室几厅）
    QString orientation;     // 朝向
    QString floor;           // 楼层
    QString decoration;      // 装修情况
    QString buildingYear;    // 建造年代
    QString houseUrl;        // 房源链接
    QString city;            // 城市
};

#endif // HOUSDATA_H

