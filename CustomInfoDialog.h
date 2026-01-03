#ifndef CUSTOMINFODIALOG_H
#define CUSTOMINFODIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QString>

// 自定义信息弹窗类（支持文本复制）
class CustomInfoDialog : public QDialog
{
    Q_OBJECT
public:
    // 构造函数：parent为父窗口（用于弹窗居中显示）
    explicit CustomInfoDialog(QWidget *parent = nullptr);

    // 核心接口：设置弹窗要显示的文本数据（供主窗口调用）
    void setInfoText(const QString &text);

private:
    QTextEdit *m_textEdit; // 可复制的文本显示控件
};

#endif // CUSTOMINFODIALOG_H
