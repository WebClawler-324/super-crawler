#include "DatabaseManager.h"
#include <QDebug>
#include <QSqlRecord>
#include <QDateTime>
#include <QJsonDocument>

DatabaseManager* DatabaseManager::m_instance = nullptr;

DatabaseManager* DatabaseManager::instance()
{
    if (!m_instance) {
        m_instance = new DatabaseManager();
    }
    return m_instance;
}

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    if (db.isOpen()) {
        db.close();
    }
}

bool DatabaseManager::initialize(const QJsonObject& config)
{
    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName(config["host"].toString());
    db.setPort(config["port"].toInt());
    db.setDatabaseName(config["database"].toString());
    db.setUserName(config["username"].toString());
    db.setPassword(config["password"].toString());
    
    if (!db.open()) {
        qCritical() << "Database connection failed:" << db.lastError().text();
        return false;
    }
    
    qDebug() << "Database connected successfully";
    return createTables();
}

bool DatabaseManager::isConnected() const
{
    return db.isOpen();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(db);
    
    // 用户表
    QString createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INT AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(50) UNIQUE NOT NULL,
            password_hash VARCHAR(255) NOT NULL,
            email VARCHAR(100) UNIQUE NOT NULL,
            email_verified BOOLEAN DEFAULT FALSE,
            is_admin BOOLEAN DEFAULT FALSE,
            is_disabled BOOLEAN DEFAULT FALSE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_login TIMESTAMP NULL,
            INDEX idx_username (username),
            INDEX idx_email (email)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    
    if (!query.exec(createUsersTable)) {
        qCritical() << "Failed to create users table:" << query.lastError().text();
        return false;
    }
    
    // 验证码表
    QString createVerificationTable = R"(
        CREATE TABLE IF NOT EXISTS verification_codes (
            id INT AUTO_INCREMENT PRIMARY KEY,
            email VARCHAR(100) NOT NULL,
            code VARCHAR(10) NOT NULL,
            type VARCHAR(20) NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            expires_at TIMESTAMP NULL DEFAULT NULL,
            INDEX idx_email_type (email, type)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    
    if (!query.exec(createVerificationTable)) {
        qCritical() << "Failed to create verification_codes table:" << query.lastError().text();
        return false;
    }
    
    // 收藏表
    QString createFavoritesTable = R"(
        CREATE TABLE IF NOT EXISTS favorites (
            id INT AUTO_INCREMENT PRIMARY KEY,
            user_id INT NOT NULL,
            house_id INT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            UNIQUE KEY unique_favorite (user_id, house_id),
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            FOREIGN KEY (house_id) REFERENCES houseinfo(ID) ON DELETE CASCADE,
            INDEX idx_user_id (user_id),
            INDEX idx_house_id (house_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    
    if (!query.exec(createFavoritesTable)) {
        qCritical() << "Failed to create favorites table:" << query.lastError().text();
        return false;
    }
    
    // 用户偏好表
    QString createPreferencesTable = R"(
        CREATE TABLE IF NOT EXISTS user_preferences (
            id INT AUTO_INCREMENT PRIMARY KEY,
            user_id INT UNIQUE NOT NULL,
            preferences JSON,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    
    if (!query.exec(createPreferencesTable)) {
        qCritical() << "Failed to create user_preferences table:" << query.lastError().text();
        return false;
    }
    
    // 创建默认管理员账户（用户名：admin，密码：admin123，需要修改）
    QString checkAdmin = "SELECT COUNT(*) as count FROM users WHERE username = 'admin'";
    if (query.exec(checkAdmin) && query.next()) {
        if (query.value("count").toInt() == 0) {
            // 这里使用简单的密码哈希，实际应该使用更安全的方式
            query.prepare(R"(
                INSERT INTO users (username, password_hash, email, email_verified, is_admin)
                VALUES (?, ?, ?, ?, ?)
            )");
            query.addBindValue("admin");
            query.addBindValue("e10adc3949ba59abbe56e057f20f883e");
            query.addBindValue("admin@house.com");
            query.addBindValue(true);
            query.addBindValue(true);
            
            if (!query.exec()) {
                qWarning() << "Failed to create admin user:" << query.lastError().text();
            } else {
                qDebug() << "Default admin user created (username: admin, password: admin123)";
            }
        }
    }
    
    qDebug() << "All tables created successfully";
    return true;
}

// 用户相关实现
bool DatabaseManager::createUser(const QString& username, const QString& passwordHash, const QString& email)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO users (username, password_hash, email) VALUES (?, ?, ?)");
    query.addBindValue(username);
    query.addBindValue(passwordHash);
    query.addBindValue(email);
    
    if (!query.exec()) {
        qWarning() << "Failed to create user:" << query.lastError().text();
        return false;
    }
    return true;
}

QVariantMap DatabaseManager::getUserByUsername(const QString& username)
{
    QSqlQuery query(db);
    query.prepare("SELECT * FROM users WHERE username = ?");
    query.addBindValue(username);
    
    if (query.exec() && query.next()) {
        QVariantMap user;
        user["id"] = query.value("id");
        user["username"] = query.value("username");
        user["password_hash"] = query.value("password_hash");
        user["email"] = query.value("email");
        user["email_verified"] = query.value("email_verified");
        user["is_admin"] = query.value("is_admin");
        user["is_disabled"] = query.value("is_disabled");
        user["created_at"] = query.value("created_at");
        user["last_login"] = query.value("last_login");
        return user;
    }
    return QVariantMap();
}

QVariantMap DatabaseManager::getUserById(int userId)
{
    QSqlQuery query(db);
    query.prepare("SELECT * FROM users WHERE id = ?");
    query.addBindValue(userId);
    
    if (query.exec() && query.next()) {
        QVariantMap user;
        user["id"] = query.value("id");
        user["username"] = query.value("username");
        user["password_hash"] = query.value("password_hash");
        user["email"] = query.value("email");
        user["email_verified"] = query.value("email_verified");
        user["is_admin"] = query.value("is_admin");
        user["is_disabled"] = query.value("is_disabled");
        user["created_at"] = query.value("created_at");
        user["last_login"] = query.value("last_login");
        return user;
    }
    return QVariantMap();
}

bool DatabaseManager::updateUser(int userId, const QVariantMap& data)
{
    QSqlQuery query(db);
    
    if (data.contains("last_login")) {
        query.prepare("UPDATE users SET last_login = NOW() WHERE id = ?");
        query.addBindValue(userId);
        return query.exec();
    }
    
    return false;
}

bool DatabaseManager::verifyEmail(int userId)
{
    QSqlQuery query(db);
    query.prepare("UPDATE users SET email_verified = TRUE WHERE id = ?");
    query.addBindValue(userId);
    return query.exec();
}

bool DatabaseManager::updatePassword(int userId, const QString& newPasswordHash)
{
    QSqlQuery query(db);
    query.prepare("UPDATE users SET password_hash = ? WHERE id = ?");
    query.addBindValue(newPasswordHash);
    query.addBindValue(userId);
    return query.exec();
}

// 验证码相关实现
bool DatabaseManager::saveVerificationCode(const QString& email, const QString& code, const QString& type)
{
    QSqlQuery query(db);
    
    // 先删除旧的验证码
    deleteVerificationCode(email, type);
    
    // 计算过期时间（10分钟后）
    QDateTime expiresAt = QDateTime::currentDateTime().addSecs(600);
    
    // 插入新的验证码
    query.prepare(R"(
        INSERT INTO verification_codes (email, code, type, expires_at)
        VALUES (?, ?, ?, ?)
    )");
    query.addBindValue(email);
    query.addBindValue(code);
    query.addBindValue(type);
    query.addBindValue(expiresAt);
    
    if (!query.exec()) {
        qWarning() << "Failed to save verification code:" << query.lastError().text();
        return false;
    }
    return true;
}

QString DatabaseManager::getVerificationCode(const QString& email, const QString& type)
{
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT code FROM verification_codes
        WHERE email = ? AND type = ? AND expires_at > NOW()
        ORDER BY created_at DESC LIMIT 1
    )");
    query.addBindValue(email);
    query.addBindValue(type);
    
    if (query.exec() && query.next()) {
        return query.value("code").toString();
    }
    return QString();
}

bool DatabaseManager::deleteVerificationCode(const QString& email, const QString& type)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM verification_codes WHERE email = ? AND type = ?");
    query.addBindValue(email);
    query.addBindValue(type);
    return query.exec();
}

// 房产相关实现
QList<QVariantMap> DatabaseManager::getHouses(int limit, int offset)
{
    QList<QVariantMap> houses;
    QSqlQuery query(db);
    query.prepare("SELECT * FROM houseinfo ORDER BY ID DESC LIMIT ? OFFSET ?");
    query.addBindValue(limit);
    query.addBindValue(offset);
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap house;
            house["ID"] = query.value("ID");
            house["houseTitle"] = query.value("houseTitle");
            house["price"] = query.value("price");
            house["area"] = query.value("area");
            house["communityName"] = query.value("communityName");
            house["floor"] = query.value("floor");
            house["houseType"] = query.value("houseType");
            house["unitPrice"] = query.value("unitPrice");
            house["houseUrl"] = query.value("houseUrl");
            houses.append(house);
        }
    }
    return houses;
}

QList<QVariantMap> DatabaseManager::searchHouses(const QVariantMap& filters)
{
    QList<QVariantMap> houses;
    QString sql = "SELECT * FROM houseinfo WHERE 1=1";
    QStringList conditions;
    
    if (filters.contains("minPrice")) {
        conditions << QString("price >= %1").arg(filters["minPrice"].toDouble());
    }
    if (filters.contains("maxPrice")) {
        conditions << QString("price <= %1").arg(filters["maxPrice"].toDouble());
    }
    if (filters.contains("minArea")) {
        conditions << QString("area >= %1").arg(filters["minArea"].toDouble());
    }
    if (filters.contains("maxArea")) {
        conditions << QString("area <= %1").arg(filters["maxArea"].toDouble());
    }
    if (filters.contains("communityName")) {
        conditions << QString("communityName LIKE '%%1%'").arg(filters["communityName"].toString());
    }
    if (filters.contains("houseType")) {
        conditions << QString("houseType = '%1'").arg(filters["houseType"].toString());
    }
    
    if (!conditions.isEmpty()) {
        sql += " AND " + conditions.join(" AND ");
    }
    
    sql += " ORDER BY ID DESC LIMIT 100";
    
    QSqlQuery query(db);
    if (query.exec(sql)) {
        while (query.next()) {
            QVariantMap house;
            house["ID"] = query.value("ID");
            house["houseTitle"] = query.value("houseTitle");
            house["price"] = query.value("price");
            house["area"] = query.value("area");
            house["communityName"] = query.value("communityName");
            house["floor"] = query.value("floor");
            house["houseType"] = query.value("houseType");
            house["unitPrice"] = query.value("unitPrice");
            house["houseUrl"] = query.value("houseUrl");
            houses.append(house);
        }
    } else {
        qWarning() << "Search houses failed:" << query.lastError().text();
    }
    return houses;
}

QVariantMap DatabaseManager::getHouseById(int houseId)
{
    QSqlQuery query(db);
    query.prepare("SELECT * FROM houseinfo WHERE ID = ?");
    query.addBindValue(houseId);
    
    if (query.exec() && query.next()) {
        QVariantMap house;
        house["ID"] = query.value("ID");
        house["houseTitle"] = query.value("houseTitle");
        house["price"] = query.value("price");
        house["area"] = query.value("area");
        house["communityName"] = query.value("communityName");
        house["floor"] = query.value("floor");
        house["houseType"] = query.value("houseType");
        house["unitPrice"] = query.value("unitPrice");
        house["houseUrl"] = query.value("houseUrl");
        return house;
    }
    return QVariantMap();
}

QList<QVariantMap> DatabaseManager::getHouseStatistics()
{
    QList<QVariantMap> stats;
    QSqlQuery query(db);
    
    // 按小区统计
    QString sql = R"(
        SELECT 
            communityName,
            COUNT(*) as count,
            AVG(price) as avg_price,
            AVG(unitPrice) as avg_unit_price,
            MIN(price) as min_price,
            MAX(price) as max_price
        FROM houseinfo
        GROUP BY communityName
        ORDER BY count DESC
        LIMIT 20
    )";
    
    if (query.exec(sql)) {
        while (query.next()) {
            QVariantMap stat;
            stat["communityName"] = query.value("communityName");
            stat["count"] = query.value("count");
            stat["avg_price"] = query.value("avg_price");
            stat["avg_unit_price"] = query.value("avg_unit_price");
            stat["min_price"] = query.value("min_price");
            stat["max_price"] = query.value("max_price");
            stats.append(stat);
        }
    }
    return stats;
}

// 收藏相关实现
bool DatabaseManager::addFavorite(int userId, int houseId)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO favorites (user_id, house_id) VALUES (?, ?)");
    query.addBindValue(userId);
    query.addBindValue(houseId);
    
    if (!query.exec()) {
        qWarning() << "Failed to add favorite:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::removeFavorite(int userId, int houseId)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM favorites WHERE user_id = ? AND house_id = ?");
    query.addBindValue(userId);
    query.addBindValue(houseId);
    return query.exec();
}

QList<QVariantMap> DatabaseManager::getUserFavorites(int userId)
{
    QList<QVariantMap> favorites;
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT h.* FROM houseinfo h
        INNER JOIN favorites f ON h.ID = f.house_id
        WHERE f.user_id = ?
        ORDER BY f.created_at DESC
    )");
    query.addBindValue(userId);
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap house;
            house["ID"] = query.value("ID");
            house["houseTitle"] = query.value("houseTitle");
            house["price"] = query.value("price");
            house["area"] = query.value("area");
            house["communityName"] = query.value("communityName");
            house["floor"] = query.value("floor");
            house["houseType"] = query.value("houseType");
            house["unitPrice"] = query.value("unitPrice");
            house["houseUrl"] = query.value("houseUrl");
            favorites.append(house);
        }
    }
    return favorites;
}

bool DatabaseManager::isFavorite(int userId, int houseId)
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) as count FROM favorites WHERE user_id = ? AND house_id = ?");
    query.addBindValue(userId);
    query.addBindValue(houseId);
    
    if (query.exec() && query.next()) {
        return query.value("count").toInt() > 0;
    }
    return false;
}

// 用户偏好相关实现
bool DatabaseManager::saveUserPreferences(int userId, const QJsonObject& preferences)
{
    QSqlQuery query(db);
    QJsonDocument doc(preferences);
    QString jsonStr = doc.toJson(QJsonDocument::Compact);
    
    query.prepare(R"(
        INSERT INTO user_preferences (user_id, preferences)
        VALUES (?, ?)
        ON DUPLICATE KEY UPDATE preferences = ?
    )");
    query.addBindValue(userId);
    query.addBindValue(jsonStr);
    query.addBindValue(jsonStr);
    
    if (!query.exec()) {
        qWarning() << "Failed to save user preferences:" << query.lastError().text();
        return false;
    }
    return true;
}

QJsonObject DatabaseManager::getUserPreferences(int userId)
{
    QSqlQuery query(db);
    query.prepare("SELECT preferences FROM user_preferences WHERE user_id = ?");
    query.addBindValue(userId);
    
    if (query.exec() && query.next()) {
        QString jsonStr = query.value("preferences").toString();
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        return doc.object();
    }
    return QJsonObject();
}

// 用户画像统计实现
QList<QVariantMap> DatabaseManager::getUserStatistics()
{
    QList<QVariantMap> stats;
    QSqlQuery query(db);
    
    QString sql = R"(
        SELECT 
            u.id,
            u.username,
            u.email,
            u.created_at,
            u.last_login,
            COUNT(DISTINCT f.house_id) as favorite_count,
            u.email_verified
        FROM users u
        LEFT JOIN favorites f ON u.id = f.user_id
        WHERE u.is_admin = FALSE
        GROUP BY u.id
        ORDER BY u.created_at DESC
    )";
    
    if (query.exec(sql)) {
        while (query.next()) {
            QVariantMap stat;
            stat["id"] = query.value("id");
            stat["username"] = query.value("username");
            stat["email"] = query.value("email");
            stat["created_at"] = query.value("created_at");
            stat["last_login"] = query.value("last_login");
            stat["favorite_count"] = query.value("favorite_count");
            stat["email_verified"] = query.value("email_verified");
            stats.append(stat);
        }
    }
    return stats;
}

bool DatabaseManager::toggleUserDisabled(int userId, bool disabled)
{
    QSqlQuery query(db);
    query.prepare("UPDATE users SET is_disabled = ? WHERE id = ?");
    query.addBindValue(disabled);
    query.addBindValue(userId);
    
    if (!query.exec()) {
        qWarning() << "Failed to toggle user disabled status:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<QVariantMap> DatabaseManager::getPopularHouses(int limit)
{
    QList<QVariantMap> houses;
    QSqlQuery query(db);
    
    // 先简单查询房源表，按ID排序返回前N条（不依赖收藏表）
    QString sql = R"(
        SELECT h.ID, h.houseTitle, h.communityName, h.houseType, h.area, h.price,
               COALESCE((SELECT COUNT(*) FROM favorites f WHERE f.house_id = h.ID), 0) AS favorite_count
        FROM houseinfo h
        ORDER BY favorite_count DESC, h.ID ASC
        LIMIT ?
    )";
    
    query.prepare(sql);
    query.addBindValue(limit);
    
    qDebug() << "Executing popular houses query with limit:" << limit;
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap house;
            house["ID"] = query.value("ID");
            house["houseTitle"] = query.value("houseTitle");
            house["communityName"] = query.value("communityName");
            house["houseType"] = query.value("houseType");
            house["area"] = query.value("area");
            house["price"] = query.value("price");
            house["favorite_count"] = query.value("favorite_count");
            houses.append(house);
        }
        qDebug() << "Popular houses query returned" << houses.size() << "results";
    } else {
        qWarning() << "Failed to get popular houses:" << query.lastError().text();
        qWarning() << "SQL:" << sql;
    }
    
    return houses;
}
