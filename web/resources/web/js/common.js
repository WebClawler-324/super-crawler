// 公共函数和工具
const API_BASE = window.location.origin;

// 存储用户信息
let currentUser = null;

// 初始化
document.addEventListener('DOMContentLoaded', function() {
    // 从localStorage加载用户信息
    const userStr = localStorage.getItem('currentUser');
    if (userStr) {
        currentUser = JSON.parse(userStr);
        updateUserUI();
    }
});

// 更新用户界面
function updateUserUI() {
    const loginBtn = document.getElementById('loginBtn');
    const registerBtn = document.getElementById('registerBtn');
    const userInfo = document.getElementById('userInfo');
    const usernameSpan = document.getElementById('username');
    const favoritesLink = document.getElementById('favoritesLink');
    const adminLink = document.getElementById('adminLink');
    
    if (currentUser) {
        if (loginBtn) loginBtn.style.display = 'none';
        if (registerBtn) registerBtn.style.display = 'none';
        if (userInfo) userInfo.style.display = 'flex';
        if (usernameSpan) usernameSpan.textContent = currentUser.username;
        if (favoritesLink) favoritesLink.style.display = 'block';
        if (adminLink && currentUser.isAdmin) {
            adminLink.style.display = 'block';
        }
    } else {
        if (loginBtn) loginBtn.style.display = 'block';
        if (registerBtn) registerBtn.style.display = 'block';
        if (userInfo) userInfo.style.display = 'none';
        if (favoritesLink) favoritesLink.style.display = 'none';
        if (adminLink) adminLink.style.display = 'none';
    }
}

// API请求封装
async function apiRequest(endpoint, options = {}) {
    const url = `${API_BASE}${endpoint}`;
    const config = {
        headers: {
            'Content-Type': 'application/json',
            ...options.headers
        },
        ...options
    };
    
    if (currentUser && currentUser.token) {
        config.headers['Authorization'] = `Bearer ${currentUser.token}`;
    }
    
    try {
        const response = await fetch(url, config);
        const data = await response.json();
        return data;
    } catch (error) {
        console.error('API request failed:', error);
        return { success: false, message: '网络请求失败' };
    }
}

// 显示消息
function showMessage(message, type = 'info') {
    // 移除之前的消息
    const existingMessages = document.querySelectorAll('.message');
    existingMessages.forEach(msg => msg.remove());
    
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${type}`;
    messageDiv.textContent = message;
    messageDiv.style.cssText = `
        position: fixed;
        top: 80px;
        left: 50%;
        transform: translateX(-50%);
        z-index: 10000;
        min-width: 300px;
        max-width: 500px;
        text-align: center;
        animation: slideDown 0.3s ease-out;
    `;
    
    document.body.appendChild(messageDiv);
    
    setTimeout(() => {
        messageDiv.style.animation = 'slideUp 0.3s ease-in';
        setTimeout(() => {
            messageDiv.remove();
        }, 300);
    }, 3000);
}

// 退出登录
function logout() {
    currentUser = null;
    localStorage.removeItem('currentUser');
    updateUserUI();
    window.location.href = '/';
}

// 格式化数字
function formatNumber(num) {
    return num.toLocaleString('zh-CN');
}

// 格式化日期
function formatDate(dateString) {
    if (!dateString) return '-';
    const date = new Date(dateString);
    return date.toLocaleString('zh-CN');
}

// 添加全局退出登录事件监听（已经改用onclick，此代码保留兼容性）
document.addEventListener('DOMContentLoaded', function() {
    const logoutBtn = document.getElementById('logoutBtn');
    if (logoutBtn && !logoutBtn.onclick) {
        logoutBtn.addEventListener('click', logout);
    }
});
