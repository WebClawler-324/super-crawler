#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    static DatabaseManager* instance();
    
    bool initialize(const QJsonObject& config);
    bool isConnected() const;
    
    // 用户相关
    bool createUser(const QString& username, const QString& passwordHash, const QString& email);
    QVariantMap getUserByUsername(const QString& username);
    QVariantMap getUserById(int userId);
    bool updateUser(int userId, const QVariantMap& data);
    bool verifyEmail(int userId);
    bool updatePassword(int userId, const QString& newPasswordHash);
    bool toggleUserDisabled(int userId, bool disabled);
    
    // 验证码相关
    bool saveVerificationCode(const QString& email, const QString& code, const QString& type);
    QString getVerificationCode(const QString& email, const QString& type);
    bool deleteVerificationCode(const QString& email, const QString& type);
    
    // 房产相关
    QList<QVariantMap> getHouses(int limit = 50, int offset = 0);
    QList<QVariantMap> searchHouses(const QVariantMap& filters);
    QVariantMap getHouseById(int houseId);
    QList<QVariantMap> getHouseStatistics();
    QList<QVariantMap> getPopularHouses(int limit = 10);
    
    // 收藏相关
    bool addFavorite(int userId, int houseId);
    bool removeFavorite(int userId, int houseId);
    QList<QVariantMap> getUserFavorites(int userId);
    bool isFavorite(int userId, int houseId);
    
    // 用户偏好相关
    bool saveUserPreferences(int userId, const QJsonObject& preferences);
    QJsonObject getUserPreferences(int userId);
    
    // 用户画像统计（管理员功能）
    QList<QVariantMap> getUserStatistics();
    
private:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();
    
    bool createTables();
    QSqlDatabase db;
    
    static DatabaseManager* m_instance;
};

#endif // DATABASEMANAGER_H
