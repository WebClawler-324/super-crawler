# 使用 rsync 同步到云服务器
# 适合不需要 Git 版本控制的场景

# ============ 配置区域 ============
$SERVER_USER = "your_username"
$SERVER_HOST = "your_server_ip"
$SERVER_PORT = "22"
$REMOTE_PATH = "~/projects/house3/"

# 排除的文件和目录
$EXCLUDE = @(
    '.git',
    'build',
    '.vs',
    'out',
    '*.user',
    'CMakeFiles',
    'CMakeCache.txt',
    'cmake-build-*',
    '.idea',
    'node_modules',
    '*.log'
)
# ==================================

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  rsync 同步到云服务器" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查是否安装了 WSL 或 Git Bash
$hasRsync = $false
$rsyncCmd = ""

# 检查 WSL
try {
    $wslCheck = wsl which rsync 2>$null
    if ($LASTEXITCODE -eq 0) {
        $hasRsync = $true
        $rsyncCmd = "wsl rsync"
        Write-Host "✅ 检测到 WSL，使用 WSL 的 rsync" -ForegroundColor Green
    }
} catch {}

# 检查 Git Bash
if (-not $hasRsync) {
    $gitBashPaths = @(
        "C:\Program Files\Git\usr\bin\rsync.exe",
        "C:\Program Files (x86)\Git\usr\bin\rsync.exe"
    )
    foreach ($path in $gitBashPaths) {
        if (Test-Path $path) {
            $hasRsync = $true
            $rsyncCmd = "`"$path`""
            Write-Host "✅ 检测到 Git Bash，使用 Git Bash 的 rsync" -ForegroundColor Green
            break
        }
    }
}

if (-not $hasRsync) {
    Write-Host "❌ 错误：未找到 rsync" -ForegroundColor Red
    Write-Host ""
    Write-Host "请安装以下工具之一：" -ForegroundColor Yellow
    Write-Host "1. WSL (Windows Subsystem for Linux)" -ForegroundColor Yellow
    Write-Host "   wsl --install" -ForegroundColor Cyan
    Write-Host "2. Git for Windows (包含 rsync)" -ForegroundColor Yellow
    Write-Host "   https://git-scm.com/download/win" -ForegroundColor Cyan
    exit 1
}

# 构建排除参数
$excludeArgs = $EXCLUDE | ForEach-Object { "--exclude='$_'" }
$excludeStr = $excludeArgs -join " "

# 转换 Windows 路径为 WSL 路径（如果使用 WSL）
$sourcePath = if ($rsyncCmd -like "*wsl*") {
    $currentPath = (Get-Location).Path
    $wslPath = $currentPath -replace '\\', '/' -replace '^([A-Z]):', { "/mnt/$($_.Groups[1].Value.ToLower())" }
    "$wslPath/"
} else {
    "./"
}

# 构建 rsync 命令
$server = "${SERVER_USER}@${SERVER_HOST}"
$sshOpt = "-e `"ssh -p $SERVER_PORT`""
$rsyncOpts = "-avz --delete --progress"

Write-Host "📁 源目录: $(Get-Location)" -ForegroundColor Cyan
Write-Host "🎯 目标: ${server}:${REMOTE_PATH}" -ForegroundColor Cyan
Write-Host "🚫 排除: $($EXCLUDE -join ', ')" -ForegroundColor Yellow
Write-Host ""

$confirm = Read-Host "确认开始同步？(y/n)"
if ($confirm -ne "y") {
    Write-Host "❌ 已取消" -ForegroundColor Yellow
    exit 0
}

Write-Host ""
Write-Host "🚀 开始同步..." -ForegroundColor Cyan
Write-Host ""

# 执行 rsync
$fullCmd = "$rsyncCmd $rsyncOpts $excludeStr $sshOpt `"$sourcePath`" `"${server}:${REMOTE_PATH}`""

try {
    Invoke-Expression $fullCmd
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "  ✅ 同步成功！" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "📍 代码已同步到: ${server}:${REMOTE_PATH}" -ForegroundColor Cyan
    } else {
        Write-Host ""
        Write-Host "❌ 同步失败，退出码: $LASTEXITCODE" -ForegroundColor Red
    }
} catch {
    Write-Host ""
    Write-Host "❌ 同步失败: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "💡 提示：" -ForegroundColor Yellow
    Write-Host "1. 确保已配置 SSH 密钥登录" -ForegroundColor Yellow
    Write-Host "2. 检查服务器地址和端口是否正确" -ForegroundColor Yellow
    Write-Host "3. 确保服务器上目标目录存在或有权限创建" -ForegroundColor Yellow
}
