# 二手房爬虫与Web服务平台

本项目包含 Qt 桌面端房源爬虫工具（含 Gumbo 解析、MySQL 入库、AI 数据分析接口）与基于 Qt Network 的轻量 Web 服务器，提供 REST 风格 API、静态页面与 AI 助手能力。

## 组件概览

- **Qt 桌面端爬虫**：使用 `QWebEnginePage` 抓取房源，写入 MySQL；内置日志展示与地图预览。
- **AI 数据接口**：`AIDataInterface` + `DeepSeekClient`，支持房源批量分析、报告生成。
- **Web 服务**（见 `web/`）：`HttpServer` 提供注册登录、房源查询、收藏、统计、AI 推荐/对话等接口，并托管前端静态资源。

## 目录速览

- `main.cpp` / `mainwindow.*`：桌面端入口与 UI。
- `Crawl.cpp` / `AliCrawl.cpp` / `BaseCrawler.h`：爬虫核心逻辑。
- `MYSQL.*`：数据库访问封装。
- `AIc/`、`AIh/`：AI 接口实现与头文件。
- `web/`：Web 服务端与前端（详见 `web/README.md`）。
  - `web/src/main.cpp`：服务启动入口。
  - `web/src/server/HttpServer.*`：HTTP 路由与静态资源服务。
  - `web/src/services/`：`AIService`、`EmailService`。
  - `web/config/config.json`：运行配置。

## 环境要求

- Qt 6 (Network, WebEngineWidgets, Sql, Charts)
- CMake ≥ 3.16，C++17
- MySQL 8.0+
- 可选：DeepSeek API Key（环境变量或写入配置）

## 桌面端构建与运行

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
./WebCrawler   # Windows 下运行对应可执行文件
```

若使用 Qt Creator，可直接打开根目录 `CMakeLists.txt` 构建。

## Web 服务（`web/`）

### 配置

编辑 `web/config/config.json`：

```json
{
  "database": { "host": "localhost", "port": 3306, "database": "house_db", "username": "user", "password": "pass" },
  "server": { "host": "0.0.0.0", "port": 8080 },
  "email": { "smtp_server": "smtp.126.com", "smtp_port": 465, "use_ssl": true, "username": "your@126.com", "password": "auth_code" },
  "deepseek": { "api_key": "your_api_key", "model": "deepseek-chat" },
  "baidu_map": { "api_key": "your_map_key" }
}
```

### 启动

```bash
cd web
mkdir build && cd build
cmake ..
cmake --build . --config Release
./HouseInfoServer   # Windows 下运行对应可执行文件
```

启动后访问：http://localhost:8080

### API 快览

主要路由（详见 `web/README.md`）：

- 用户：`POST /api/register`、`POST /api/login`、`POST /api/send-code`、`POST /api/verify-email`、`POST /api/reset-password`、`GET /api/user/info`
- 房源：`GET /api/houses`、`POST /api/houses/search`、`GET /api/houses/:id`、`GET /api/houses/statistics`
- 收藏：`POST /api/favorites`、`DELETE /api/favorites`、`GET /api/favorites`
- AI：`POST /api/ai/recommend`、`POST /api/ai/chat`

## 数据库初始化

```sql
CREATE DATABASE house_db;
```

若字段缺失，可执行 `web/scripts/fix_database.sql` 或 `fix_users_table.sql` 等修复脚本。

## AI 接入要点

- 配置 DeepSeek API Key（`deepseek.api_key` 或环境变量 `Deepseek_test1`）。
- 使用 `AIDataInterface::importDataAndAnalyze()` 直接从数据库取数分析，或 `analyzeHouseData()` 传入列表。

## 常见问题

- **无法加载配置**：确认 `web/config/config.json` 路径与 JSON 格式正确。
- **MySQL 连接失败**：检查主机/端口/账号权限，确保表已创建。
- **AI 服务未启用**：确认 API Key 已配置，网络可访问 DeepSeek。

## 许可

仅供学习交流使用。
