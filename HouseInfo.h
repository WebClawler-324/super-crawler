#ifndef HOUSEINFO_H
#define HOUSEINFO_H

#include <QString>

// 房源数据结构体
struct HouseInfo {
    QString city;           // 城市（如：北京、上海）
    QString houseTitle;     // 房源标题
    QString communityName;  // 小区名称
    QString price;          // 总价（带单位，如：500万）
    QString evalPrice;      // 评估价（带单位，如：520万）
    QString unitPrice;
    QString houseType;      // 户型（如：3室2厅）
    QString area;           // 面积（带单位，如：120㎡）
    QString orientation;    // 朝向（如：南北通透）
    QString floor;          // 楼层（如：中楼层(共18层)）
    QString buildingYear;   // 建筑年代（如：2010年）
    QString houseUrl;       // 房源详情页链接
    QString region;         // 所在区域（如：朝阳区）
    QString decoration;     // 装修情况（如：精装修）
    QString location;  // 具体位置（如：张郭庄地铁站附近）
    QString rent;        // 年租
};

#endif // HOUSEINFO_H
