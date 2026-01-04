# 二手房信息服务平台

一个基于Qt6的完整二手房信息Web服务平台，支持用户注册、房源浏览、智能推荐、AI助手等功能。

## 📋 项目特性

### 核心功能
- ✅ **用户系统**: 注册、登录、邮箱验证、密码重置、密码修改
- 🏠 **房源管理**: 浏览、搜索、筛选、详情查看
- ⭐ **收藏功能**: 添加/移除收藏，独立收藏页面
- 🎯 **智能筛选**: 按价格、面积、地区、户型筛选
- 🤖 **AI助手**: DeepSeek驱动，打字机效果，用户反馈
- 📍 **地图集成**: 百度地图显示房源位置
- 📊 **数据统计**: 可视化统计页面，价格分布，热门小区
- ⚙️ **账户设置**: 双重验证密码修改（邮箱/旧密码）
- 👨‍💼 **管理后台**: 完整的管理员控制面板

### 技术特点
- **后端**: Qt6 Core + Network + SQL
- **数据库**: MySQL 8.0+
- **前端**: 原生HTML5 + CSS3 + JavaScript
- **架构**: RESTful API + MVC模式
- **邮件服务**: SMTP (126邮箱)
- **AI集成**: DeepSeek API
- **地图服务**: 百度地图API

## 📁 项目结构

```
house3/
├── src/                          # 源代码
│   ├── main.cpp                  # 主程序入口
│   ├── database/                 # 数据库层
│   │   ├── DatabaseManager.h
│   │   └── DatabaseManager.cpp
│   ├── services/                 # 服务层
│   │   ├── EmailService.h        # 邮件服务
│   │   ├── EmailService.cpp
│   │   ├── AIService.h           # AI服务
│   │   └── AIService.cpp
│   ├── server/                   # 服务器层
│   │   ├── HttpServer.h          # HTTP服务器
│   │   └── HttpServer.cpp
│   └── models/                   # 数据模型
├── config/                       # 配置文件
│   └── config.json               # 主配置文件
├── resources/                    # 资源文件
│   └── web/                      # Web前端
│       ├── index.html            # 首页
│       ├── house-detail.html     # 房源详情
│       ├── ai-assistant.html     # AI助手
│       ├── statistics.html       # 数据统计 (新)
│       ├── favorites.html        # 我的收藏 (新)
│       ├── settings.html         # 账户设置 (新)
│       ├── admin.html            # 管理后台 (新)
│       ├── css/
│       │   └── style.css         # 全局样式
│       └── js/
│           ├── common.js         # 公共函数
│           ├── index.js          # 首页脚本
│           └── config.js         # 配置加载 (新)
│       ├── css/
│       │   └── style.css         # 样式文件
│       └── js/
│           ├── common.js         # 公共JS
│           └── index.js          # 首页JS
├── CMakeLists.txt                # CMake配置
├── HouseInfoServer.pro           # QMake配置
├── DEPLOYMENT.md                 # 部署指南
└── README.md                     # 项目说明
```

## 🚀 快速开始

### 前置要求
- Ubuntu 20.04+ 或其他Linux发行版
- Qt 6.2+
- MySQL 8.0+
- CMake 3.16+
- GCC 9.0+ 或 Clang 10.0+

### 本地开发

1. **克隆项目**
```bash
git clone <repository-url>
cd house3
```

2. **安装依赖**
```bash
sudo apt install qt6-base-dev mysql-server libmysqlclient-dev
```

3. **配置数据库**
```bash
mysql -u root -p
CREATE DATABASE house_db;
```

4. **修改配置**
```bash
nano config/config.json
# 填写数据库、邮箱、API密钥等配置
```

5. **初始化数据库**
```bash
# 如果遇到字段缺失错误，执行修复脚本
chmod +x scripts/fix_database.sh
./scripts/fix_database.sh

# 或手动执行SQL
mysql -u root -p < scripts/fix_users_table.sql
```

6. **编译运行**
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./HouseInfoServer
```

7. **访问应用**
```
http://localhost:8080
```

**默认管理员账户**：
- 用户名: `admin`
- 密码: `admin123`
- ⚠️ 首次登录后请立即在"账户设置"中修改密码！

## 📦 生产部署

详细部署步骤请参考 [DEPLOYMENT.md](DEPLOYMENT.md)

### 快速部署命令

```bash
# 安装依赖
sudo apt update
sudo apt install -y qt6-base-dev mysql-server cmake build-essential

# 配置MySQL
sudo mysql_secure_installation

# 编译安装
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install

# 配置服务
sudo systemctl enable house-server
sudo systemctl start house-server
```

## ⚙️ 配置说明

### config.json 配置项

```json
{
  "database": {
    "host": "localhost",        // MySQL主机
    "port": 3306,               // MySQL端口
    "database": "house_db",     // 数据库名
    "username": "your_user",    // 数据库用户名
    "password": "your_pass"     // 数据库密码
  },
  "server": {
    "host": "0.0.0.0",         // 监听地址
    "port": 8080               // 监听端口
  },
  "email": {
    "smtp_server": "smtp.126.com",  // SMTP服务器
    "smtp_port": 465,                // SMTP端口
    "use_ssl": true,                 // 使用SSL
    "username": "your@126.com",      // 邮箱账号
    "password": "auth_code"          // 邮箱授权码
  },
  "deepseek": {
    "api_key": "your_api_key",      // DeepSeek API密钥
    "model": "deepseek-chat"        // 使用的模型
  },
  "baidu_map": {
    "api_key": "your_map_key"       // 百度地图API密钥
  }
}
```

## 🗄️ 数据库表结构

### 已存在表
- **houseinfo**: 房产信息（您已创建）

### 自动创建表
- **users**: 用户账户信息
- **verification_codes**: 邮箱验证码
- **favorites**: 用户收藏
- **user_preferences**: 用户偏好设置

## 🔌 API接口

### 用户相关
- `POST /api/register` - 用户注册
- `POST /api/login` - 用户登录
- `POST /api/send-code` - 发送验证码
- `POST /api/verify-email` - 验证邮箱
- `POST /api/reset-password` - 重置密码
- `GET /api/user/info` - 获取用户信息

### 房产相关
- `GET /api/houses` - 获取房源列表
- `POST /api/houses/search` - 搜索房源
- `GET /api/houses/:id` - 获取房源详情
- `GET /api/houses/statistics` - 获取统计数据

### 收藏相关
- `POST /api/favorites` - 添加收藏
- `DELETE /api/favorites` - 取消收藏
- `GET /api/favorites` - 获取收藏列表

### AI相关
- `POST /api/ai/recommend` - AI智能推荐
- `POST /api/ai/chat` - AI问答

### 管理相关
- `GET /api/admin/users` - 获取用户统计

## 🔐 安全说明

1. **密码加密**: 使用MD5哈希（建议升级为bcrypt）
2. **邮箱验证**: 注册时需要邮箱验证
3. **授权机制**: 简单Token认证（建议升级为JWT）
4. **SQL注入防护**: 使用参数化查询
5. **CORS配置**: 支持跨域请求

## 📝 待改进项

- [ ] 使用JWT替代简单Token
- [ ] 密码加密升级为bcrypt或argon2
- [ ] 添加图片上传功能
- [ ] 实现WebSocket实时通信
- [ ] 添加Redis缓存
- [ ] 完善错误处理和日志系统
- [ ] 添加单元测试
- [ ] API速率限制

## 🛠️ 开发工具

### 推荐IDE
- Qt Creator
- Visual Studio Code (C++ 扩展)
- CLion

### 调试工具
- GDB (命令行调试)
- Valgrind (内存检测)
- Qt Creator Debugger

### 测试工具
- Postman (API测试)
- curl (命令行测试)
- MySQL Workbench (数据库管理)

## 📞 联系方式

如有问题或建议，请通过以下方式联系：
- 提交Issue
- 发送邮件

## 📄 许可证

本项目仅供学习使用。

---

**开发时间**: 2026年1月
**作者**: 您的名字
**版本**: 1.0.0
