#include "TaxCalculator.h"

/**
 * 计算契税的实现
 *
 * 函数逻辑：
 * 1. 根据家庭房产套数判断是否为二套房
 * 2. 根据房屋面积判断是否超过140平方米
 * 3. 根据套数和面积组合确定税率
 * 4. 用房屋总价乘以税率得到契税金额
 */
double TaxCalculator::calculateDeedTax(double housePrice, double houseArea, int familyHouseCount)
{
    double taxRate = 0.0; // 税率（百分比，转为小数）

    // 根据国家税务总局契税规定确定税率
    if (familyHouseCount == 1) {
        // 首套房
        if (houseArea <= 140.0) {
            taxRate = 0.01; // 1%
        } else {
            taxRate = 0.015; // 1.5%
        }
    } else if (familyHouseCount >= 2) {
        // 二套房及以上
        if (houseArea <= 140.0) {
            taxRate = 0.01; // 1%（二套房140㎡以下仍按1%）
        } else {
            taxRate = 0.02; // 2%
        }
    }

    // 计算契税：总价 × 税率
    double deedTax = housePrice * taxRate;

    return deedTax;
}

/**
 * 获取契税税率说明的实现
 *
 * 返回具体的税率文字说明，方便用户理解
 */
QString TaxCalculator::getDeedTaxRateDescription(double houseArea, int familyHouseCount)
{
    QString description;

    if (familyHouseCount == 1) {
        // 首套房
        if (houseArea <= 140.0) {
            description = QString("首套房%1㎡以内：契税税率1%").arg(houseArea);
        } else {
            description = QString("首套房%1㎡：契税税率1.5%").arg(houseArea);
        }
    } else if (familyHouseCount >= 2) {
        // 二套房及以上
        if (houseArea <= 140.0) {
            description = QString("二套房%1㎡以内：契税税率1%").arg(houseArea);
        } else {
            description = QString("二套房%1㎡：契税税率2%").arg(houseArea);
        }
    }

    return description;
}
