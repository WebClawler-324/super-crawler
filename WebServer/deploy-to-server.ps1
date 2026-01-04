# 一键部署到云服务器脚本
# 使用方法：.\deploy-to-server.ps1

# ============ 配置区域 ============
$SERVER_USER = "your_username"           # 云服务器用户名
$SERVER_HOST = "your_server_ip"          # 云服务器 IP 或域名
$SERVER_PORT = "22"                      # SSH 端口，默认 22
$REMOTE_NAME = "cloud"                   # Git 远程仓库名称
$REMOTE_PATH = "~/git/house3.git"        # 云服务器上的裸仓库路径
$WORK_PATH = "~/projects/house3"         # 云服务器上的工作目录
# ==================================

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  一键部署到云服务器" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查是否在 git 仓库中
if (-not (Test-Path ".git")) {
    Write-Host "❌ 错误：当前目录不是 Git 仓库" -ForegroundColor Red
    exit 1
}

# 检查是否有未提交的更改
$status = git status --porcelain
if ($status) {
    Write-Host "⚠️  检测到未提交的更改：" -ForegroundColor Yellow
    git status --short
    Write-Host ""
    $commit = Read-Host "是否先提交这些更改？(y/n)"
    if ($commit -eq "y") {
        $message = Read-Host "请输入提交消息"
        git add .
        git commit -m $message
        Write-Host "✅ 已提交更改" -ForegroundColor Green
    }
}

# 检查远程仓库是否已添加
$remotes = git remote
if ($remotes -notcontains $REMOTE_NAME) {
    Write-Host "📝 添加远程仓库: $REMOTE_NAME" -ForegroundColor Yellow
    $remoteUrl = "${SERVER_USER}@${SERVER_HOST}:${REMOTE_PATH}"
    git remote add $REMOTE_NAME $remoteUrl
    Write-Host "✅ 已添加远程仓库: $remoteUrl" -ForegroundColor Green
}

# 推送到云服务器
Write-Host ""
Write-Host "🚀 开始推送到云服务器..." -ForegroundColor Cyan
$branch = git branch --show-current

try {
    git push $REMOTE_NAME ${branch}:master -f
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  ✅ 部署成功！" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "📍 代码已同步到: ${SERVER_USER}@${SERVER_HOST}:${WORK_PATH}" -ForegroundColor Cyan
    Write-Host "🔗 连接服务器: ssh ${SERVER_USER}@${SERVER_HOST}" -ForegroundColor Cyan
} catch {
    Write-Host ""
    Write-Host "❌ 推送失败: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "💡 提示：" -ForegroundColor Yellow
    Write-Host "1. 确保已配置 SSH 密钥登录" -ForegroundColor Yellow
    Write-Host "2. 在服务器上创建裸仓库: mkdir -p $REMOTE_PATH && cd $REMOTE_PATH && git init --bare" -ForegroundColor Yellow
    exit 1
}
