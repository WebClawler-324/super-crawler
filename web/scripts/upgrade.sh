#!/bin/bash
# 一键安全升级脚本
# 用于已安装系统的安全更新

set -e  # 遇到错误立即退出

echo "=========================================="
echo "  二手房平台 - 安全升级脚本 v2.0"
echo "=========================================="
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 检查是否为root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}请使用 sudo 运行此脚本${NC}"
    exit 1
fi

# 配置
PROJECT_DIR="/opt/house3"
BACKUP_ROOT="/opt/backups"
SERVICE_NAME="house-server"

# 1. 创建备份
echo -e "${BLUE}[1/8] 创建完整备份...${NC}"
BACKUP_DIR=$BACKUP_ROOT/house3_$(date +%Y%m%d_%H%M%S)
mkdir -p $BACKUP_DIR

# 备份可执行文件
if [ -f "/usr/local/bin/HouseInfoServer" ]; then
    cp /usr/local/bin/HouseInfoServer $BACKUP_DIR/
    echo -e "${GREEN}  ✓ 已备份可执行文件${NC}"
fi

# 备份配置
if [ -d "$PROJECT_DIR/config" ]; then
    cp -r $PROJECT_DIR/config $BACKUP_DIR/
    echo -e "${GREEN}  ✓ 已备份配置文件${NC}"
fi

# 备份资源
if [ -d "$PROJECT_DIR/resources" ]; then
    cp -r $PROJECT_DIR/resources $BACKUP_DIR/
    echo -e "${GREEN}  ✓ 已备份Web资源${NC}"
fi

echo -e "${GREEN}✓ 备份完成: $BACKUP_DIR${NC}"
echo ""

# 2. 停止服务
echo -e "${BLUE}[2/8] 停止服务...${NC}"
if systemctl is-active --quiet $SERVICE_NAME; then
    systemctl stop $SERVICE_NAME
    sleep 2
    echo -e "${GREEN}✓ 服务已停止${NC}"
else
    echo -e "${YELLOW}! 服务未运行${NC}"
fi
echo ""

# 3. 升级数据库
echo -e "${BLUE}[3/8] 检查数据库升级...${NC}"
cd $PROJECT_DIR
if [ -f "scripts/fix_users_table.sql" ]; then
    echo -e "${YELLOW}  需要数据库密码来执行升级${NC}"
    read -sp "  MySQL root 密码: " DB_PASS
    echo ""
    
    mysql -u root -p"$DB_PASS" < scripts/fix_users_table.sql 2>&1 | grep -v "Warning" || true
    echo -e "${GREEN}✓ 数据库检查完成${NC}"
else
    echo -e "${YELLOW}! 未找到数据库升级脚本，跳过${NC}"
fi
echo ""

# 4. 清理并重新编译
echo -e "${BLUE}[4/8] 重新编译项目...${NC}"
rm -rf build/
mkdir build
cd build

echo -e "${YELLOW}  正在配置...${NC}"
cmake .. > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}  ✓ CMake配置成功${NC}"
else
    echo -e "${RED}  ✗ CMake配置失败${NC}"
    exit 1
fi

echo -e "${YELLOW}  正在编译（可能需要几分钟）...${NC}"
make -j$(nproc) > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ 编译完成${NC}"
else
    echo -e "${RED}✗ 编译失败${NC}"
    exit 1
fi
echo ""

# 5. 安装
echo -e "${BLUE}[5/8] 安装更新...${NC}"
make install > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ 安装完成${NC}"
else
    echo -e "${RED}✗ 安装失败${NC}"
    exit 1
fi
echo ""

# 6. 验证文件
echo -e "${BLUE}[6/8] 验证安装...${NC}"

# 检查可执行文件
if [ -f "/usr/local/bin/HouseInfoServer" ]; then
    echo -e "${GREEN}  ✓ 可执行文件已更新${NC}"
else
    echo -e "${RED}  ✗ 可执行文件未找到${NC}"
    exit 1
fi

# 检查新增的HTML文件
NEW_FILES=(
    "$PROJECT_DIR/resources/web/statistics.html"
    "$PROJECT_DIR/resources/web/favorites.html"
    "$PROJECT_DIR/resources/web/settings.html"
    "$PROJECT_DIR/resources/web/admin.html"
)

for file in "${NEW_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${GREEN}  ✓ $(basename $file) 已更新${NC}"
    else
        echo -e "${YELLOW}  ! $(basename $file) 未找到${NC}"
    fi
done
echo ""

# 7. 启动服务
echo -e "${BLUE}[7/8] 启动服务...${NC}"
systemctl start $SERVICE_NAME
sleep 3

# 8. 验证服务
echo -e "${BLUE}[8/8] 验证服务状态...${NC}"
if systemctl is-active --quiet $SERVICE_NAME; then
    echo -e "${GREEN}  ✓ 服务启动成功${NC}"
    
    # 测试HTTP
    sleep 2
    if curl -s http://localhost:8080/ > /dev/null 2>&1; then
        echo -e "${GREEN}  ✓ HTTP服务响应正常${NC}"
    else
        echo -e "${YELLOW}  ! HTTP服务可能需要几秒钟启动${NC}"
    fi
    
    echo ""
    echo -e "${GREEN}=========================================="
    echo "  ✓ 升级成功完成！"
    echo "==========================================${NC}"
    echo ""
    echo -e "${BLUE}备份位置:${NC} $BACKUP_DIR"
    echo -e "${BLUE}查看日志:${NC} journalctl -u $SERVICE_NAME -f"
    echo -e "${BLUE}服务状态:${NC} systemctl status $SERVICE_NAME"
    echo ""
    echo -e "${YELLOW}新增功能页面：${NC}"
    echo "  - 数据统计: http://your-server:8080/statistics.html"
    echo "  - 我的收藏: http://your-server:8080/favorites.html"
    echo "  - 账户设置: http://your-server:8080/settings.html"
    echo "  - 管理后台: http://your-server:8080/admin.html"
    echo ""
    echo -e "${YELLOW}请在浏览器中测试所有功能！${NC}"
    echo ""
    
else
    echo -e "${RED}  ✗ 服务启动失败！${NC}"
    echo ""
    echo -e "${YELLOW}正在尝试回滚...${NC}"
    
    # 回滚
    if [ -f "$BACKUP_DIR/HouseInfoServer" ]; then
        cp $BACKUP_DIR/HouseInfoServer /usr/local/bin/
        systemctl start $SERVICE_NAME
        
        if systemctl is-active --quiet $SERVICE_NAME; then
            echo -e "${GREEN}✓ 已回滚到备份版本，服务正常运行${NC}"
        else
            echo -e "${RED}✗ 回滚失败，请手动检查${NC}"
        fi
    fi
    
    echo ""
    echo -e "${RED}升级失败！请查看日志：${NC}"
    echo "  journalctl -u $SERVICE_NAME -n 50"
    echo ""
    exit 1
fi

# 清理提示
echo -e "${BLUE}提示：${NC}"
echo "  - 备份保留在: $BACKUP_DIR"
echo "  - 如需清理旧备份: find $BACKUP_ROOT -type d -mtime +30 -exec rm -rf {} \;"
echo ""
