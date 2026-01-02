#!/bin/bash
# 数据库修复脚本
# 用于修复 email_verified 和 is_admin 字段缺失的问题

echo "========================================"
echo "  二手房平台 - 数据库修复工具"
echo "========================================"
echo ""

# 读取数据库配置
echo "请输入MySQL配置信息："
read -p "数据库主机 [localhost]: " DB_HOST
DB_HOST=${DB_HOST:-localhost}

read -p "数据库端口 [3306]: " DB_PORT
DB_PORT=${DB_PORT:-3306}

read -p "数据库名称 [house_db]: " DB_NAME
DB_NAME=${DB_NAME:-house_db}

read -p "数据库用户名: " DB_USER
read -sp "数据库密码: " DB_PASS
echo ""

echo ""
echo "正在连接数据库..."

# 生成SQL修复脚本
cat > /tmp/fix_users_table.sql << EOF
USE $DB_NAME;

-- 检查并添加 email_verified 字段
SET @col_exists = 0;
SELECT COUNT(*) INTO @col_exists 
FROM information_schema.COLUMNS 
WHERE TABLE_SCHEMA = '$DB_NAME' 
  AND TABLE_NAME = 'users' 
  AND COLUMN_NAME = 'email_verified';

SET @sql = IF(@col_exists = 0,
  'ALTER TABLE users ADD COLUMN email_verified TINYINT(1) NOT NULL DEFAULT 0 AFTER email',
  'SELECT "email_verified字段已存在" AS status');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 检查并添加 is_admin 字段
SET @col_exists = 0;
SELECT COUNT(*) INTO @col_exists 
FROM information_schema.COLUMNS 
WHERE TABLE_SCHEMA = '$DB_NAME' 
  AND TABLE_NAME = 'users' 
  AND COLUMN_NAME = 'is_admin';

SET @sql = IF(@col_exists = 0,
  'ALTER TABLE users ADD COLUMN is_admin TINYINT(1) NOT NULL DEFAULT 0 AFTER email_verified',
  'SELECT "is_admin字段已存在" AS status');
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 清理可能的错误admin数据
DELETE FROM users WHERE username='admin' AND email='admin@house.com';

-- 显示最终表结构
DESCRIBE users;

SELECT "数据库修复完成！" AS status;
EOF

# 执行SQL脚本
mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASS" < /tmp/fix_users_table.sql

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ 数据库修复成功！"
    echo ""
    echo "接下来的步骤："
    echo "1. 重启服务：sudo systemctl restart house-server"
    echo "2. 检查日志：journalctl -u house-server -f"
    echo "3. 确认admin用户自动创建成功"
    echo ""
    echo "默认管理员账户："
    echo "  用户名: admin"
    echo "  密码: admin123"
    echo "  ⚠️ 首次登录后请立即修改密码！"
else
    echo ""
    echo "❌ 数据库修复失败！"
    echo "请检查："
    echo "1. 数据库连接信息是否正确"
    echo "2. MySQL服务是否运行"
    echo "3. 用户是否有足够权限"
fi

# 清理临时文件
rm -f /tmp/fix_users_table.sql

echo ""
echo "========================================"
