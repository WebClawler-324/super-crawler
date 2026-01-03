#ifndef TAXCALCULATOR_H
#define TAXCALCULATOR_H

#include <QString>

/**
 * @brief 税费计算器类
 *
 * 主要用于计算房产交易相关的税费，目前支持契税计算
 */
class TaxCalculator
{
public:
    /**
     * @brief 计算契税
     *
     * 根据国家税务总局的规定计算契税：
     * - 家庭首套/二套住房 ≤140㎡：税率1%
     * - 家庭首套住房 >140㎡：税率1.5%
     * - 家庭二套住房 >140㎡：税率2%
     *
     * @param housePrice 房屋总价（单位：万元）
     * @param houseArea 房屋面积（单位：平方米）
     * @param familyHouseCount 家庭名下房产套数（用于判断是否为二套房）
     * @return double 契税金额（单位：万元）
     *
     * @note
     * - housePrice: 房屋的实际成交总价
     * - houseArea: 房屋的建筑面积
     * - familyHouseCount: 购买前家庭名下已有的房产套数（1表示首套，2表示二套）
     * - 返回值为契税金额，直接用总价乘以对应税率得到
     *
     * @example
     * // 计算一套100㎡的首套房，总价200万
     * double tax = calculateDeedTax(200.0, 100.0, 1); // 返回2.0万
     *
     * // 计算一套150㎡的首套房，总价300万
     * double tax = calculateDeedTax(300.0, 150.0, 1); // 返回4.5万
     *
     * // 计算一套150㎡的二套房，总价300万
     * double tax = calculateDeedTax(300.0, 150.0, 2); // 返回6.0万
     */
    static double calculateDeedTax(double housePrice, double houseArea, int familyHouseCount);

    /**
     * @brief 获取契税税率说明
     *
     * 返回当前适用的契税税率说明文字
     *
     * @param houseArea 房屋面积（平方米）
     * @param familyHouseCount 家庭房产套数
     * @return QString 税率说明文字
     */
    static QString getDeedTaxRateDescription(double houseArea, int familyHouseCount);
};

#endif // TAXCALCULATOR_H
