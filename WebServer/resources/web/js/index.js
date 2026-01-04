// 首页JavaScript
let currentPage = 1;
const pageSize = 50;
let currentFilters = {};

// 加载房源列表
async function loadHouses(page = 1) {
    const loadingIndicator = document.getElementById('loadingIndicator');
    const housesList = document.getElementById('housesList');
    
    loadingIndicator.style.display = 'block';
    housesList.innerHTML = '';
    
    const offset = (page - 1) * pageSize;
    let data;
    
    if (Object.keys(currentFilters).length > 0) {
        // 使用搜索
        data = await apiRequest('/api/houses/search', {
            method: 'POST',
            body: JSON.stringify(currentFilters)
        });
    } else {
        // 获取所有房源
        data = await apiRequest(`/api/houses?limit=${pageSize}&offset=${offset}`);
    }
    
    loadingIndicator.style.display = 'none';
    
    if (data.success && data.data) {
        const houses = data.data;
        
        if (houses.length === 0) {
            housesList.innerHTML = '<p style="text-align: center; padding: 2rem; color: #999;">暂无房源数据</p>';
            return;
        }
        
        houses.forEach(house => {
            const card = createHouseCard(house);
            housesList.appendChild(card);
        });
        
        currentPage = page;
        updatePagination();
    } else {
        showMessage(data.message || '加载失败', 'error');
    }
}

// 创建房源卡片
function createHouseCard(house) {
    const card = document.createElement('div');
    card.className = 'house-card';
    card.innerHTML = `
        <div class="house-image">🏠</div>
        <div class="house-content">
            <h3 class="house-title">${house.houseTitle || '未命名房产'}</h3>
            <div class="house-info">
                <div class="house-info-item">
                    <span>小区：</span>
                    <span>${house.communityName || '-'}</span>
                </div>
                <div class="house-info-item">
                    <span>户型：</span>
                    <span>${house.houseType || '-'}</span>
                </div>
                <div class="house-info-item">
                    <span>面积：</span>
                    <span>${house.area || '-'} ㎡</span>
                </div>
                <div class="house-info-item">
                    <span>楼层：</span>
                    <span>${house.floor || '-'}</span>
                </div>
            </div>
            <div class="house-price">
                ¥ ${formatNumber(house.price)} 万
                <div class="house-unit-price">单价: ${formatNumber(house.unitPrice)} 元/㎡</div>
            </div>
            <div class="house-actions">
                <button class="btn btn-primary btn-sm" onclick="viewHouseDetail(${house.ID})">查看详情</button>
                ${currentUser ? `<button class="btn btn-success btn-sm" onclick="toggleFavorite(${house.ID}, this)">收藏</button>` : ''}
            </div>
        </div>
    `;
    return card;
}

// 查看房源详情
function viewHouseDetail(houseId) {
    window.location.href = `/house-detail.html?id=${houseId}`;
}

// 切换收藏状态
async function toggleFavorite(houseId, button) {
    if (!currentUser) {
        showMessage('请先登录', 'error');
        return;
    }
    
    const isFavorite = button.textContent === '取消收藏';
    const endpoint = '/api/favorites';
    const method = isFavorite ? 'DELETE' : 'POST';
    
    const data = await apiRequest(endpoint, {
        method: method,
        body: JSON.stringify({
            userId: currentUser.userId,
            houseId: houseId
        })
    });
    
    if (data.success) {
        button.textContent = isFavorite ? '收藏' : '取消收藏';
        button.className = isFavorite ? 'btn btn-success btn-sm' : 'btn btn-danger btn-sm';
        showMessage(data.message, 'success');
    } else {
        showMessage(data.message, 'error');
    }
}

// 更新分页
function updatePagination() {
    const pageInfo = document.getElementById('pageInfo');
    const prevBtn = document.getElementById('prevPage');
    const nextBtn = document.getElementById('nextPage');
    
    pageInfo.textContent = `第 ${currentPage} 页`;
    prevBtn.disabled = currentPage === 1;
}

// 搜索房源
async function searchHouses() {
    const minPrice = document.getElementById('minPrice').value;
    const maxPrice = document.getElementById('maxPrice').value;
    const minArea = document.getElementById('minArea').value;
    const maxArea = document.getElementById('maxArea').value;
    const communityName = document.getElementById('communityName').value;
    const houseType = document.getElementById('houseType').value;
    
    currentFilters = {};
    
    if (minPrice) currentFilters.minPrice = parseFloat(minPrice);
    if (maxPrice) currentFilters.maxPrice = parseFloat(maxPrice);
    if (minArea) currentFilters.minArea = parseFloat(minArea);
    if (maxArea) currentFilters.maxArea = parseFloat(maxArea);
    if (communityName) currentFilters.communityName = communityName;
    if (houseType) currentFilters.houseType = houseType;
    
    loadHouses(1);
}

// 重置搜索
function resetSearch() {
    document.getElementById('minPrice').value = '';
    document.getElementById('maxPrice').value = '';
    document.getElementById('minArea').value = '';
    document.getElementById('maxArea').value = '';
    document.getElementById('communityName').value = '';
    document.getElementById('houseType').value = '';
    
    currentFilters = {};
    loadHouses(1);
}

// 登录
async function handleLogin(event) {
    event.preventDefault();
    
    const form = event.target;
    const formData = new FormData(form);
    
    const data = await apiRequest('/api/login', {
        method: 'POST',
        body: JSON.stringify({
            username: formData.get('username'),
            password: formData.get('password')
        })
    });
    
    if (data.success) {
        currentUser = data.data;
        localStorage.setItem('currentUser', JSON.stringify(currentUser));
        updateUserUI();
        closeModal('loginModal');
        showMessage('登录成功', 'success');
        form.reset();
    } else {
        showMessage(data.message, 'error');
    }
}

// 注册
async function handleRegister(event) {
    event.preventDefault();
    
    const form = event.target;
    const formData = new FormData(form);
    
    const data = await apiRequest('/api/register', {
        method: 'POST',
        body: JSON.stringify({
            username: formData.get('username'),
            email: formData.get('email'),
            password: formData.get('password'),
            code: formData.get('code')
        })
    });
    
    if (data.success) {
        currentUser = data.data;
        localStorage.setItem('currentUser', JSON.stringify(currentUser));
        updateUserUI();
        closeModal('registerModal');
        showMessage('注册成功', 'success');
        form.reset();
    } else {
        showMessage(data.message, 'error');
    }
}

// 发送验证码
let sendCodeTimer = null;
async function sendVerificationCode() {
    const emailInput = document.querySelector('#registerForm input[name="email"]');
    const email = emailInput.value;
    
    if (!email) {
        showMessage('请输入邮箱地址', 'error');
        return;
    }
    
    const sendCodeBtn = document.getElementById('sendCodeBtn');
    sendCodeBtn.disabled = true;
    
    const data = await apiRequest('/api/send-code', {
        method: 'POST',
        body: JSON.stringify({
            email: email,
            type: 'register'
        })
    });
    
    if (data.success) {
        showMessage(data.message, 'success');
        
        // 倒计时
        let countdown = 60;
        sendCodeBtn.textContent = `${countdown}秒后重发`;
        
        sendCodeTimer = setInterval(() => {
            countdown--;
            if (countdown <= 0) {
                clearInterval(sendCodeTimer);
                sendCodeBtn.disabled = false;
                sendCodeBtn.textContent = '发送验证码';
            } else {
                sendCodeBtn.textContent = `${countdown}秒后重发`;
            }
        }, 1000);
    } else {
        showMessage(data.message, 'error');
        sendCodeBtn.disabled = false;
    }
}

// 模态框控制
function openModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) {
        modal.style.display = 'block';
    }
}

function closeModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) {
        modal.style.display = 'none';
    }
}

// 事件监听
document.addEventListener('DOMContentLoaded', function() {
    // 加载房源
    loadHouses(1);
    
    // 搜索按钮
    const searchBtn = document.getElementById('searchBtn');
    if (searchBtn) {
        searchBtn.addEventListener('click', searchHouses);
    }
    
    // 重置按钮
    const resetBtn = document.getElementById('resetBtn');
    if (resetBtn) {
        resetBtn.addEventListener('click', resetSearch);
    }
    
    // 分页按钮
    const prevBtn = document.getElementById('prevPage');
    const nextBtn = document.getElementById('nextPage');
    
    if (prevBtn) {
        prevBtn.addEventListener('click', () => {
            if (currentPage > 1) {
                loadHouses(currentPage - 1);
            }
        });
    }
    
    if (nextBtn) {
        nextBtn.addEventListener('click', () => {
            loadHouses(currentPage + 1);
        });
    }
    
    // 登录按钮
    const loginBtn = document.getElementById('loginBtn');
    if (loginBtn) {
        loginBtn.addEventListener('click', () => openModal('loginModal'));
    }
    
    // 注册按钮
    const registerBtn = document.getElementById('registerBtn');
    if (registerBtn) {
        registerBtn.addEventListener('click', () => openModal('registerModal'));
    }
    
    // 关闭模态框
    const closeLogin = document.getElementById('closeLogin');
    if (closeLogin) {
        closeLogin.addEventListener('click', () => closeModal('loginModal'));
    }
    
    const closeRegister = document.getElementById('closeRegister');
    if (closeRegister) {
        closeRegister.addEventListener('click', () => closeModal('registerModal'));
    }
    
    // 点击模态框外部关闭
    window.addEventListener('click', (event) => {
        const loginModal = document.getElementById('loginModal');
        const registerModal = document.getElementById('registerModal');
        
        if (event.target === loginModal) {
            closeModal('loginModal');
        }
        if (event.target === registerModal) {
            closeModal('registerModal');
        }
    });
    
    // 表单提交
    const loginForm = document.getElementById('loginForm');
    if (loginForm) {
        loginForm.addEventListener('submit', handleLogin);
    }
    
    const registerForm = document.getElementById('registerForm');
    if (registerForm) {
        registerForm.addEventListener('submit', handleRegister);
    }
    
    // 发送验证码按钮
    const sendCodeBtn = document.getElementById('sendCodeBtn');
    if (sendCodeBtn) {
        sendCodeBtn.addEventListener('click', sendVerificationCode);
    }
});
