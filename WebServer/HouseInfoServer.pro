QT += core network sql
QT -= gui

CONFIG += c++17 console
CONFIG -= app_bundle

# 项目名称
TARGET = HouseInfoServer

# 源文件
SOURCES += \
    src/main.cpp \
    src/database/DatabaseManager.cpp \
    src/services/EmailService.cpp \
    src/services/AIService.cpp \
    src/server/HttpServer.cpp

# 头文件
HEADERS += \
    src/database/DatabaseManager.h \
    src/services/EmailService.h \
    src/services/AIService.h \
    src/server/HttpServer.h

# 包含路径
INCLUDEPATH += src

# 编译选项
QMAKE_CXXFLAGS += -std=c++17

# Release配置
Release:QMAKE_CXXFLAGS += -O3

# Debug配置
Debug:QMAKE_CXXFLAGS += -g -O0

# 默认部署规则
unix {
    target.path = /usr/local/bin
    INSTALLS += target
}
