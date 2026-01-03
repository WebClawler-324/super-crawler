#include "CustomInfoDialog.h"

CustomInfoDialog::CustomInfoDialog(QWidget *parent)
    : QDialog(parent)
{
    // 1. 设置弹窗属性（标题、大小、固定最小尺寸）
    this->setWindowTitle("选中行详情"); // 弹窗标题
    this->resize(500, 300); // 默认大小
    this->setMinimumSize(400, 250); // 最小尺寸，防止用户拖得太小

    // 2. 创建核心控件：QTextEdit（支持复制）
    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true); // 设置为只读，避免用户修改数据
    m_textEdit->setStyleSheet("font-size: 14px; padding: 10px;"); // 可选：设置样式，更美观
    // QTextEdit默认支持右键菜单（复制、粘贴、选中全部等），无需额外代码

    // 3. 创建关闭按钮（QDialogButtonBox：QT标准按钮容器，自带布局）
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    // 绑定OK按钮点击信号，触发弹窗关闭
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    // 4. 布局弹窗控件（QVBoxLayout：垂直布局）
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_textEdit); // 先添加文本框
    mainLayout->addWidget(buttonBox); // 再添加按钮
    mainLayout->setSpacing(10); // 控件之间的间距
    mainLayout->setContentsMargins(15, 15, 15, 15); // 弹窗内边距
    this->setLayout(mainLayout); // 设置弹窗的主布局
}

// 实现数据设置接口：将主窗口传递的文本设置到QTextEdit中
void CustomInfoDialog::setInfoText(const QString &text)
{
    m_textEdit->setText(text); // 填充文本
    m_textEdit->moveCursor(QTextCursor::Start); // 光标移到文本开头，方便查看
}
