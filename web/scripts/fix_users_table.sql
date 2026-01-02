-- ===================================================
-- 二手房平台 - 数据库修复脚本
-- ===================================================
-- 用途：修复 users 表缺少 email_verified 和 is_admin 字段的问题
-- 使用方法：
--   1. 修改第一行的数据库名称
--   2. 在MySQL客户端执行此脚本
--   3. 重启服务
-- ===================================================

USE house_db;  -- ⚠️ 请修改为您的实际数据库名称

-- 步骤1: 检查表结构
SELECT '当前users表结构：' AS info;
DESCRIBE users;

-- 步骤2: 添加 email_verified 字段
-- 如果字段已存在会报错，可以忽略
SELECT '正在添加 email_verified 字段...' AS info;

-- MySQL 8.0+ 使用此语句
-- ALTER TABLE users ADD COLUMN IF NOT EXISTS email_verified TINYINT(1) NOT NULL DEFAULT 0 AFTER email;

-- MySQL 5.7 或更早版本，请逐条执行以下语句
-- 如果字段已存在会报错，继续执行下一条即可
ALTER TABLE users ADD COLUMN email_verified TINYINT(1) NOT NULL DEFAULT 0 AFTER email;

-- 步骤3: 添加 is_admin 字段
SELECT '正在添加 is_admin 字段...' AS info;

-- MySQL 8.0+ 使用此语句
-- ALTER TABLE users ADD COLUMN IF NOT EXISTS is_admin TINYINT(1) NOT NULL DEFAULT 0 AFTER email_verified;

-- MySQL 5.7 或更早版本
ALTER TABLE users ADD COLUMN is_admin TINYINT(1) NOT NULL DEFAULT 0 AFTER email_verified;

-- 步骤4: 清理可能的错误admin数据
SELECT '正在清理旧的admin账户...' AS info;
DELETE FROM users WHERE username='admin' AND email='admin@house.com';

-- 步骤5: 验证修复结果
SELECT '修复后的users表结构：' AS info;
DESCRIBE users;

SELECT '检查users表数据：' AS info;
SELECT id, username, email, email_verified, is_admin, created_at 
FROM users 
ORDER BY id;

-- 步骤6: 显示完成信息
SELECT '✅ 数据库修复完成！' AS status;
SELECT '请重启服务，admin账户将自动创建' AS next_step;
SELECT '默认账户 - 用户名: admin, 密码: admin123' AS admin_info;
SELECT '⚠️ 首次登录后请立即修改密码！' AS warning;

-- ===================================================
-- 常见问题解决
-- ===================================================

-- 问题1: 如果提示 "Duplicate column name"
-- 说明字段已存在，可以忽略此错误

-- 问题2: 如果提示 "You have an error in your SQL syntax"
-- 可能是MySQL版本不支持 IF NOT EXISTS
-- 请使用不带 IF NOT EXISTS 的ALTER语句

-- 问题3: 如何手动创建admin用户？
-- 取消下面的注释并执行：
/*
INSERT INTO users (username, password_hash, email, email_verified, is_admin, created_at)
VALUES (
    'admin',
    'e10adc3949ba59abbe56e057f20f883e',  -- MD5('admin123')
    'admin@house.com',
    1,
    1,
    NOW()
);
*/

-- 问题4: 如何检查字段是否存在？
-- 执行以下查询：
/*
SELECT COLUMN_NAME, DATA_TYPE, IS_NULLABLE, COLUMN_DEFAULT
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = 'house_db'  -- 修改为您的数据库名
  AND TABLE_NAME = 'users'
  AND COLUMN_NAME IN ('email_verified', 'is_admin');
*/

-- ===================================================
-- 脚本结束
-- ===================================================
