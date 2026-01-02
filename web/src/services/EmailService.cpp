#include "EmailService.h"
#include <QDebug>
#include <QTcpSocket>
#include <QSslSocket>
#include <QRandomGenerator>
#include <QDateTime>

EmailService* EmailService::m_instance = nullptr;

EmailService* EmailService::instance()
{
    if (!m_instance) {
        m_instance = new EmailService();
    }
    return m_instance;
}

EmailService::EmailService(QObject *parent)
    : QObject(parent)
    , smtpPort(465)
    , useSsl(true)
{
}

EmailService::~EmailService()
{
}

bool EmailService::initialize(const QJsonObject& config)
{
    smtpServer = config["smtp_server"].toString();
    smtpPort = config["smtp_port"].toInt();
    useSsl = config["use_ssl"].toBool();
    username = config["username"].toString();
    password = config["password"].toString();
    fromName = config["from_name"].toString();
    
    qDebug() << "Email service initialized with server:" << smtpServer;
    return true;
}

QString EmailService::generateVerificationCode(int length)
{
    QString code;
    for (int i = 0; i < length; ++i) {
        code += QString::number(QRandomGenerator::global()->bounded(10));
    }
    return code;
}

bool EmailService::sendVerificationEmail(const QString& toEmail, const QString& code, const QString& type)
{
    QString subject;
    QString body;
    
    if (type == "register") {
        subject = "二手房平台 - 注册验证码";
        body = QString(R"(
            <html>
            <body style="font-family: Arial, sans-serif;">
                <h2>欢迎注册二手房平台</h2>
                <p>您的验证码是：<strong style="font-size: 24px; color: #007bff;">%1</strong></p>
                <p>验证码将在10分钟后过期，请尽快使用。</p>
                <p>如果这不是您的操作，请忽略此邮件。</p>
                <hr>
                <p style="color: #666; font-size: 12px;">此邮件由系统自动发送，请勿回复。</p>
            </body>
            </html>
        )").arg(code);
    } else if (type == "reset_password") {
        subject = "二手房平台 - 密码重置验证码";
        body = QString(R"(
            <html>
            <body style="font-family: Arial, sans-serif;">
                <h2>密码重置请求</h2>
                <p>您的验证码是：<strong style="font-size: 24px; color: #007bff;">%1</strong></p>
                <p>验证码将在10分钟后过期，请尽快使用。</p>
                <p>如果这不是您的操作，请立即修改密码以保护账户安全。</p>
                <hr>
                <p style="color: #666; font-size: 12px;">此邮件由系统自动发送，请勿回复。</p>
            </body>
            </html>
        )").arg(code);
    } else if (type == "change_password") {
        subject = "二手房平台 - 修改密码验证码";
        body = QString(R"(
            <html>
            <body style="font-family: Arial, sans-serif;">
                <h2>修改密码验证</h2>
                <p>您正在修改账户密码，验证码是：<strong style="font-size: 24px; color: #007bff;">%1</strong></p>
                <p>验证码将在10分钟后过期，请尽快使用。</p>
                <p>如果这不是您的操作，请立即登录账户检查安全设置。</p>
                <hr>
                <p style="color: #666; font-size: 12px;">此邮件由系统自动发送，请勿回复。</p>
            </body>
            </html>
        )").arg(code);
    }
    
    return sendEmail(toEmail, subject, body);
}

bool EmailService::sendPasswordResetNotification(const QString& toEmail, const QString& username)
{
    QString subject = "二手房平台 - 密码已重置";
    QString body = QString(R"(
        <html>
        <body style="font-family: Arial, sans-serif;">
            <h2>密码已成功重置</h2>
            <p>尊敬的 %1：</p>
            <p>您的账户密码已成功重置。重置时间：%2</p>
            <p>如果这不是您的操作，请立即联系我们。</p>
            <hr>
            <p style="color: #666; font-size: 12px;">此邮件由系统自动发送，请勿回复。</p>
        </body>
        </html>
    )").arg(username).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    
    return sendEmail(toEmail, subject, body);
}

bool EmailService::sendPasswordChangeSuccessNotification(const QString& toEmail, const QString& username)
{
    QString subject = "二手房平台 - 密码修改成功";
    QString body = QString(R"(
        <html>
        <body style="font-family: Arial, sans-serif;">
            <h2>✅ 密码修改成功</h2>
            <p>尊敬的 %1：</p>
            <p>您的账户密码已成功修改。</p>
            <p><strong>修改时间：</strong>%2</p>
            <p><strong>修改方式：</strong>使用旧密码验证</p>
            <p style="margin-top: 20px; padding: 15px; background-color: #e8f5e9; border-left: 4px solid #4caf50;">
                💡 <strong>安全提示：</strong>如果这不是您本人的操作，请立即登录系统修改密码，并联系我们的客服团队。
            </p>
            <hr>
            <p style="color: #666; font-size: 12px;">此邮件由系统自动发送，请勿回复。</p>
        </body>
        </html>
    )").arg(username).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    
    return sendEmail(toEmail, subject, body);
}

bool EmailService::sendPasswordChangeFailureWarning(const QString& toEmail, const QString& username)
{
    QString subject = "⚠️ 二手房平台 - 密码修改失败警告";
    QString body = QString(R"(
        <html>
        <body style="font-family: Arial, sans-serif;">
            <h2 style="color: #f44336;">⚠️ 密码修改尝试失败</h2>
            <p>尊敬的 %1：</p>
            <p>有人尝试使用<strong>错误的旧密码</strong>修改您的账户密码，但验证失败。</p>
            <p><strong>尝试时间：</strong>%2</p>
            <p><strong>验证方式：</strong>旧密码验证</p>
            <p><strong>失败原因：</strong>提供的旧密码不正确</p>
            <div style="margin-top: 20px; padding: 15px; background-color: #ffebee; border-left: 4px solid #f44336;">
                <p style="margin: 0; font-weight: bold; color: #d32f2f;">🔒 安全建议：</p>
                <ul style="margin: 10px 0 0 20px; color: #666;">
                    <li>如果这是您本人的操作，请确认您输入的旧密码是否正确</li>
                    <li>如果这不是您的操作，说明有人可能在尝试访问您的账户</li>
                    <li>建议您立即修改密码以保护账户安全</li>
                    <li>检查账户的登录记录，确认是否有异常登录</li>
                </ul>
            </div>
            <p style="margin-top: 20px;">
                <strong>需要帮助？</strong>请联系我们的客服团队。
            </p>
            <hr>
            <p style="color: #666; font-size: 12px;">此邮件由系统自动发送，请勿回复。</p>
        </body>
        </html>
    )").arg(username).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    
    return sendEmail(toEmail, subject, body);
}

bool EmailService::sendEmail(const QString& to, const QString& subject, const QString& body)
{
    // 这里使用QSslSocket实现SMTP发送
    // 注意：这是简化版实现，生产环境建议使用专门的SMTP库如VMime或SimpleMail
    
    QSslSocket *socket = new QSslSocket(this);
    
    qDebug() << "Connecting to SMTP server:" << smtpServer << ":" << smtpPort;
    
    if (useSsl) {
        socket->connectToHostEncrypted(smtpServer, smtpPort);
    } else {
        socket->connectToHost(smtpServer, smtpPort);
    }
    
    if (!socket->waitForConnected(5000)) {
        qWarning() << "Failed to connect to SMTP server:" << socket->errorString();
        socket->deleteLater();
        return false;
    }
    
    // 等待服务器响应
    if (!socket->waitForReadyRead(5000)) {
        qWarning() << "No response from SMTP server";
        socket->deleteLater();
        return false;
    }
    
    QString response = socket->readAll();
    qDebug() << "Server response:" << response;
    
    // EHLO命令
    socket->write(QString("EHLO localhost\r\n").toUtf8());
    socket->waitForBytesWritten();
    socket->waitForReadyRead();
    response = socket->readAll();
    qDebug() << "EHLO response:" << response;
    
    // AUTH LOGIN
    socket->write("AUTH LOGIN\r\n");
    socket->waitForBytesWritten();
    socket->waitForReadyRead();
    response = socket->readAll();
    qDebug() << "AUTH response:" << response;
    
    // 发送用户名（Base64编码）
    socket->write(username.toUtf8().toBase64() + "\r\n");
    socket->waitForBytesWritten();
    socket->waitForReadyRead();
    response = socket->readAll();
    qDebug() << "Username response:" << response;
    
    // 发送密码（Base64编码）
    socket->write(password.toUtf8().toBase64() + "\r\n");
    socket->waitForBytesWritten();
    socket->waitForReadyRead();
    response = socket->readAll();
    qDebug() << "Password response:" << response;
    
    // MAIL FROM
    socket->write(QString("MAIL FROM:<%1>\r\n").arg(username).toUtf8());
    socket->waitForBytesWritten();
    socket->waitForReadyRead();
    response = socket->readAll();
    qDebug() << "MAIL FROM response:" << response;
    
    // RCPT TO
    socket->write(QString("RCPT TO:<%1>\r\n").arg(to).toUtf8());
    socket->waitForBytesWritten();
    socket->waitForReadyRead();
    response = socket->readAll();
    qDebug() << "RCPT TO response:" << response;
    
    // DATA
    socket->write("DATA\r\n");
    socket->waitForBytesWritten();
    socket->waitForReadyRead();
    response = socket->readAll();
    qDebug() << "DATA response:" << response;
    
    // 邮件内容
    QString message = QString(
        "From: %1 <%2>\r\n"
        "To: %3\r\n"
        "Subject: %4\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "MIME-Version: 1.0\r\n"
        "\r\n"
        "%5\r\n"
        ".\r\n"
    ).arg(fromName).arg(username).arg(to).arg(subject).arg(body);
    
    socket->write(message.toUtf8());
    socket->waitForBytesWritten();
    socket->waitForReadyRead(5000);
    response = socket->readAll();
    qDebug() << "Message response:" << response;
    
    // QUIT
    socket->write("QUIT\r\n");
    socket->waitForBytesWritten();
    socket->waitForReadyRead();
    response = socket->readAll();
    qDebug() << "QUIT response:" << response;
    
    socket->disconnectFromHost();
    socket->deleteLater();
    
    qDebug() << "Email sent successfully to:" << to;
    return true;
}
