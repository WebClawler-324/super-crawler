#ifndef EMAILSERVICE_H
#define EMAILSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class EmailService : public QObject
{
    Q_OBJECT

public:
    static EmailService* instance();
    
    bool initialize(const QJsonObject& config);
    
    // 发送验证码邮件
    bool sendVerificationEmail(const QString& toEmail, const QString& code, const QString& type);
    
    // 发送密码重置通知
    bool sendPasswordResetNotification(const QString& toEmail, const QString& username);
    
    // 发送密码修改成功通知
    bool sendPasswordChangeSuccessNotification(const QString& toEmail, const QString& username);
    
    // 发送密码修改失败警告
    bool sendPasswordChangeFailureWarning(const QString& toEmail, const QString& username);
    
    // 生成随机验证码
    static QString generateVerificationCode(int length = 6);

private:
    explicit EmailService(QObject *parent = nullptr);
    ~EmailService();
    
    bool sendEmail(const QString& to, const QString& subject, const QString& body);
    
    QString smtpServer;
    int smtpPort;
    bool useSsl;
    QString username;
    QString password;
    QString fromName;
    
    static EmailService* m_instance;
};

#endif // EMAILSERVICE_H
