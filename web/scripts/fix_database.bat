@echo off
chcp 65001 >nul
REM 数据库修复脚本 (Windows版本)
REM 用于修复 email_verified 和 is_admin 字段缺失的问题

echo ========================================
echo   二手房平台 - 数据库修复工具
echo ========================================
echo.

REM 读取数据库配置
set /p DB_HOST="数据库主机 [localhost]: " || set DB_HOST=localhost
set /p DB_PORT="数据库端口 [3306]: " || set DB_PORT=3306
set /p DB_NAME="数据库名称 [house_db]: " || set DB_NAME=house_db
set /p DB_USER="数据库用户名: "
set /p DB_PASS="数据库密码: "

echo.
echo 正在生成修复脚本...

REM 生成SQL修复脚本
echo USE %DB_NAME%; > fix_users_table.sql
echo. >> fix_users_table.sql
echo -- 添加 email_verified 字段 >> fix_users_table.sql
echo ALTER TABLE users ADD COLUMN IF NOT EXISTS email_verified TINYINT(1) NOT NULL DEFAULT 0 AFTER email; >> fix_users_table.sql
echo. >> fix_users_table.sql
echo -- 添加 is_admin 字段 >> fix_users_table.sql
echo ALTER TABLE users ADD COLUMN IF NOT EXISTS is_admin TINYINT(1) NOT NULL DEFAULT 0 AFTER email_verified; >> fix_users_table.sql
echo. >> fix_users_table.sql
echo -- 清理可能的错误admin数据 >> fix_users_table.sql
echo DELETE FROM users WHERE username='admin' AND email='admin@house.com'; >> fix_users_table.sql
echo. >> fix_users_table.sql
echo -- 显示最终表结构 >> fix_users_table.sql
echo DESCRIBE users; >> fix_users_table.sql
echo. >> fix_users_table.sql
echo SELECT '数据库修复完成!' AS status; >> fix_users_table.sql

echo.
echo 正在执行修复...
echo.

REM 执行SQL脚本
mysql -h %DB_HOST% -P %DB_PORT% -u %DB_USER% -p%DB_PASS% < fix_users_table.sql

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✅ 数据库修复成功！
    echo.
    echo 接下来的步骤：
    echo 1. 重启服务
    echo 2. 检查日志
    echo 3. 确认admin用户自动创建成功
    echo.
    echo 默认管理员账户：
    echo   用户名: admin
    echo   密码: admin123
    echo   ⚠️ 首次登录后请立即修改密码！
) else (
    echo.
    echo ❌ 数据库修复失败！
    echo 请检查：
    echo 1. 数据库连接信息是否正确
    echo 2. MySQL服务是否运行
    echo 3. 用户是否有足够权限
    echo 4. MySQL版本是否支持 IF NOT EXISTS 语法
    echo.
    echo 如果是MySQL 5.7，请手动执行以下SQL：
    echo.
    type fix_users_table.sql
)

echo.
echo 修复脚本已保存到: fix_users_table.sql
echo 您也可以手动执行此脚本
echo.
echo ========================================
pause
