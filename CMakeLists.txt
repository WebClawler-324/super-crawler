cmake_minimum_required(VERSION 3.16)

project(WebCrawler VERSION 0.1 LANGUAGES CXX C)  # 保留C语言支持（Gumbo依赖）

# 核心编译配置
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 统一查找Qt模块（兼容Qt5/Qt6，包含WebEngineWidgets）
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
    Widgets
    Network
    WebEngineWidgets  # 爬虫核心依赖：WebEngine
    Sql
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Widgets
    Network
    WebEngineWidgets
    Sql
)

# 2. 配置Gumbo源码（路径校验+防遗漏）
message(STATUS "当前项目根目录：${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Gumbo源码路径：${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src")

# 检查Gumbo路径是否存在（调试用，可选）
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src")
    message(WARNING "Gumbo源码路径不存在！请确认gumbo文件夹放在项目根目录")
endif()

# Gumbo所有C源码文件（确保无遗漏）
set(GUMBO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/attribute.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/char_ref.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/string_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/string_piece.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/tag.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/tokenizer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/utf8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src/vector.c
)

# 3. 添加Gumbo头文件路径（让编译器找到gumbo.h）
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/gumbo/src)

# 4. 项目源码列表
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

# 5. 构建可执行文件（兼容Qt5/Qt6，统一添加Gumbo源码）
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(WebCrawler
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${GUMBO_SOURCES}
        build/Desktop_Qt_6_9_3_MSVC2022_64bit-Debug/ke_cookies.txt
        build/Desktop_Qt_6_9_3_MSVC2022_64bit-Debug/h5st_encrypt.js
        Test.html
        Crawl.h
        Crawl.cpp
        HouseData.h
        LLMClient.h
        LLMClient.cpp
        MYSQL.h
        MYSQL.cpp
        AliCrawl.h
        AliCrawl.cpp
        HouseInfo.h
        CrawlThreadManager.h

        ali_cookies.txt
        build/Desktop_Qt_6_9_3_MSVC2022_64bit-Debug/ali_cookies.txt





        # Gumbo源码必须加入编译
    )
else()
    if(ANDROID)
        add_library(WebCrawler SHARED
            ${PROJECT_SOURCES}
            ${GUMBO_SOURCES}
        )
    else()
        add_executable(WebCrawler
            ${PROJECT_SOURCES}
            ${GUMBO_SOURCES}
        )
    endif()
endif()

# 6. 链接Qt库（统一兼容Qt5/Qt6，避免重复链接）
target_link_libraries(WebCrawler PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::WebEngineWidgets  # 关键：链接WebEngine
    Qt${QT_VERSION_MAJOR}::Sql
)

# 7. 目标属性配置（精简+兼容）
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.WebCrawler)
endif()

set_target_properties(WebCrawler PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# 8. 安装配置（默认）
include(GNUInstallDirs)
install(TARGETS WebCrawler
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 9. Qt6最终化配置
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(WebCrawler)
endif()
