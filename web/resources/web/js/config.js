// 配置加载脚本
// 此文件用于动态加载配置，特别是第三方API密钥

// 百度地图配置（从服务端配置文件中读取）
// 注意：在实际部署时，需要在config.json中配置您的百度地图AK
// 临时方案：直接在HTML中替换YOUR_BAIDU_MAP_AK为实际的AK

// 获取配置
async function loadConfig() {
    try {
        const response = await fetch('/api/config');
        if (response.ok) {
            const config = await response.json();
            return config;
        }
    } catch (error) {
        console.warn('无法加载配置文件，使用默认配置');
    }
    return {};
}

// 动态加载百度地图API
function loadBaiduMapAPI(ak) {
    return new Promise((resolve, reject) => {
        if (typeof BMap !== 'undefined') {
            resolve();
            return;
        }
        
        const script = document.createElement('script');
        script.src = `https://api.map.baidu.com/api?v=3.0&ak=${ak}&callback=initBaiduMap`;
        script.onerror = reject;
        
        window.initBaiduMap = function() {
            resolve();
        };
        
        document.head.appendChild(script);
    });
}
