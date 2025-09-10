cmake_minimum_required(VERSION 3.30.5)
project(InstrumentDashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_BINARY_DIR "D:/86151/liushixiong/InstrumentDashboard")

# Windows 特定配置
if(WIN32)
    message(STATUS "配置Windows构建环境")
    
    # Qt6路径设置
    set(CMAKE_PREFIX_PATH "D:/app/qt/6.5.3/msvc2019_64/lib/cmake/Qt6"  ${CMAKE_PREFIX_PATH})
    
    # OpenCV路径设置
    set(OpenCV_DIR "D:/app/opencv/opencv/build/x64/vc16/lib")
    
    # Pylon SDK路径设置
    set(PYLON_ROOT "D:/app/pylon")
    list(APPEND CMAKE_PREFIX_PATH "${PYLON_ROOT}")
    
    # 包含目录设置
    include_directories(
        "${PYLON_ROOT}/Development/include"
        "D:/app/opencv/opencv/build/include"
    )
    
    # 链接目录设置
    link_directories(
        "${PYLON_ROOT}/Runtime/x64"
        "${PYLON_ROOT}/Development/lib/x64"
    )
    
    # Pylon库列表（根据您的Pylon 8版本调整）
    set(PYLON_LIBS
        PylonBase_v10
        PylonUtility_v10
        GenApi_MD_VC141_v3_1_Basler_pylon_v3
        GCBase_MD_VC141_v3_1_Basler_pylon_v3
    )
    
endif()

# macOS 特定配置
if(APPLE)
    message(STATUS "配置macOS构建环境")
    
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

set(OpenCV_DIR "D:/app/opencv/opencv/build/x64/vc16/lib")

# 查找依赖包
find_package(OpenCV REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# 通用包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

# macOS Pylon SDK配置
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

# 源文件列表
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

# Windows特定的Qt处理
if(WIN32)
    qt6_wrap_ui(UIS ${UI})
    qt6_wrap_cpp(MOCS ${INC})
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${INC} ${UIS} ${MOCS} ${RESOURCES})
else()
    # macOS和其他平台使用自动MOC
    add_executable(${PROJECT_NAME} ${SOURCES} ${INC} ${UI} ${RESOURCES})
endif()

# 链接库
target_link_libraries(${PROJECT_NAME}
    Qt6::Core Qt6::Gui Qt6::Widgets
    ${OpenCV_LIBS}
    ${PYLON_LIBS}
)

# 平台特定的POST_BUILD操作
if(WIN32)
    message(STATUS "配置Windows POST_BUILD操作")
    
    # 复制Pylon运行时库
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PYLON_ROOT}/Runtime/x64"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "复制Pylon运行时库"
    )
    
    # 复制图片资源
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/images"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/images
        COMMENT "复制图片资源"
    )
    
    # 复制OpenCV运行时库
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "D:/app/opencv/opencv/build/x64/vc16/bin"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "复制OpenCV运行时库"
    )
    
    # 自动部署Qt依赖库
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "D:/app/qt/6.5.3/msvc2019_64/bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} 
                    --verbose 2 
                    --no-translations 
                    --no-system-d3d-compiler 
                    --no-opengl-sw 
                    $<TARGET_FILE:${PROJECT_NAME}>
            COMMENT "部署Qt依赖库"
        )
    else()
        message(WARNING "找不到windeployqt工具，请手动复制Qt运行时库")
    endif()
    
elseif(APPLE)
    message(STATUS "配置macOS POST_BUILD操作")
    
    # 复制图片资源
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/images"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/images
        COMMENT "复制图片资源"
    )
                
    # 如果存在 Pylon 运行时库，复制它们
    if(EXISTS "${PYLON_FRAMEWORK}/Libraries")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${PYLON_FRAMEWORK}/Libraries"
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "复制Pylon运行时库"
        )
    endif()
endif()

# Windows打包配置
if(WIN32)
    # 安装配置
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
    )
    
    # 安装资源文件
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/images
        DESTINATION bin
    )
    
    # 安装运行时库
    install(DIRECTORY "${PYLON_ROOT}/Runtime/x64/"
        DESTINATION bin
        FILES_MATCHING PATTERN "*.dll"
    )
    
    install(DIRECTORY "D:/app/opencv/opencv/build/x64/vc16/bin/"
        DESTINATION bin
        FILES_MATCHING PATTERN "*.dll"
    )
    
    # CPack配置用于创建安装程序
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_PACKAGE_NAME "InstrumentDashboard")
    set(CPACK_PACKAGE_VENDOR "Your Company")
    set(CPACK_PACKAGE_VERSION "1.0.0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "仪表盘读数系统")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "InstrumentDashboard")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_DISPLAY_NAME "仪表盘读数系统")
    set(CPACK_NSIS_PACKAGE_NAME "InstrumentDashboard")
    
    include(CPack)
endif()