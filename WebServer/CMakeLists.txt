cmake_minimum_required(VERSION 3.16)

project(HouseInfoServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用Qt MOC自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置Qt路径（根据实际安装路径调整）
set(CMAKE_PREFIX_PATH "/usr/lib/qt6" CACHE PATH "Qt6 installation prefix")

# 查找Qt包
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/src)

# 源文件
set(SOURCES
    src/main.cpp
    src/database/DatabaseManager.cpp
    src/services/EmailService.cpp
    src/services/AIService.cpp
    src/server/HttpServer.cpp
)

# 头文件
set(HEADERS
    src/database/DatabaseManager.h
    src/services/EmailService.h
    src/services/AIService.h
    src/server/HttpServer.h
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接Qt库
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# 复制配置文件
install(DIRECTORY config/
    DESTINATION /etc/house-server/
    FILES_MATCHING PATTERN "*.json"
)

# 复制Web资源
install(DIRECTORY resources/web/
    DESTINATION /var/www/house-server/
    FILES_MATCHING PATTERN "*"
)

# 设置编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -g -O0)
endif()

# 打印配置信息
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Qt Version: ${Qt6_VERSION}")
