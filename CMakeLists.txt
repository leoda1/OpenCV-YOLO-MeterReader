cmake_minimum_required(VERSION 3.16)
project(InstrumentDashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# macOS 特定配置
if(APPLE)
    # 查找 Homebrew 安装路径
    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE HOMEBREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    # 设置 Qt6 路径 (通过 Homebrew 安装)
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}/opt/qt@6/lib/cmake")
    
    # 设置 OpenCV 路径 (通过 Homebrew 安装)
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}/opt/opencv/lib/cmake")
    
    # Pylon SDK for macOS (Framework 安装)
    set(PYLON_FRAMEWORK "/Library/Frameworks/pylon.framework")
    if(EXISTS "${PYLON_FRAMEWORK}")
        set(PYLON_ROOT "${PYLON_FRAMEWORK}")
        list(APPEND CMAKE_PREFIX_PATH "${PYLON_FRAMEWORK}")
    endif()
endif()

find_package(OpenCV REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

# 如果存在 Pylon SDK，添加相关配置
if(APPLE AND EXISTS "${PYLON_FRAMEWORK}")
    include_directories("${PYLON_FRAMEWORK}/Headers")
    include_directories("${PYLON_FRAMEWORK}/Headers/GenICam")
    link_directories("${PYLON_FRAMEWORK}/Libraries")
    # 添加 Pylon 框架链接
    find_library(PYLON_FRAMEWORK_LIB pylon PATHS /Library/Frameworks)
    if(PYLON_FRAMEWORK_LIB)
        list(APPEND PYLON_LIBS ${PYLON_FRAMEWORK_LIB})
    endif()
endif()

# 获取 Qt6 Core 库的实际文件路径
get_target_property(QtCore_location Qt6::Core LOCATION)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/settings.cpp
    src/settingdialog.cpp
    src/dialmarkdialog.cpp
    src/errortabledialog.cpp
)

set(INC
    src/mainwindow.h
    src/settings.h
    src/settingdialog.h
    src/dialmarkdialog.h
    src/errortabledialog.h
)

set(UI
    src/mainwindow.ui
    src/settingdialog.ui
)

set(RESOURCES
    src/images.qrc
)

add_executable(${PROJECT_NAME} ${SOURCES} ${INC} ${UI} ${RESOURCES})

target_link_libraries(${PROJECT_NAME}
    Qt6::Core Qt6::Gui Qt6::Widgets
    ${OpenCV_LIBS}
    ${PYLON_LIBS}
)

# macOS 特定的 POST_BUILD 操作
if(APPLE)
    # 复制图片资源
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/images"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/images)
                
    # 如果存在 Pylon 运行时库，复制它们
    if(EXISTS "${PYLON_FRAMEWORK}/Libraries")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${PYLON_FRAMEWORK}/Libraries"
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    endif()
else()
    # Windows 配置保留
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PYLON_ROOT}/Runtime/x64"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/images"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/images)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "E:/opencv/build/x64/vc16/bin"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()
