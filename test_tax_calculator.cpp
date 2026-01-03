#include <QCoreApplication>
#include <QDebug>
#include "TaxCalculator.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qDebug() << "=== 税费计算器测试 ===";

    // 测试用例1：首套房，100㎡，200万
    double tax1 = TaxCalculator::calculateDeedTax(200.0, 100.0, 1);
    QString desc1 = TaxCalculator::getDeedTaxRateDescription(100.0, 1);
    qDebug() << "测试1 - 首套房100㎡ 200万：" << desc1 << "，契税：" << tax1 << "万";

    // 测试用例2：首套房，150㎡，300万
    double tax2 = TaxCalculator::calculateDeedTax(300.0, 150.0, 1);
    QString desc2 = TaxCalculator::getDeedTaxRateDescription(150.0, 1);
    qDebug() << "测试2 - 首套房150㎡ 300万：" << desc2 << "，契税：" << tax2 << "万";

    // 测试用例3：二套房，150㎡，300万
    double tax3 = TaxCalculator::calculateDeedTax(300.0, 150.0, 2);
    QString desc3 = TaxCalculator::getDeedTaxRateDescription(150.0, 2);
    qDebug() << "测试3 - 二套房150㎡ 300万：" << desc3 << "，契税：" << tax3 << "万";

    // 测试用例4：二套房，120㎡，250万
    double tax4 = TaxCalculator::calculateDeedTax(250.0, 120.0, 2);
    QString desc4 = TaxCalculator::getDeedTaxRateDescription(120.0, 2);
    qDebug() << "测试4 - 二套房120㎡ 250万：" << desc4 << "，契税：" << tax4 << "万";

    qDebug() << "=== 测试完成 ===";

    return 0;
}
