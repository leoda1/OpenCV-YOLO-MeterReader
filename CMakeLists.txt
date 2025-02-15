cmake_minimum_required(VERSION 3.16)
project(InstrumentDashboard)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)                  # 启用自动 MOC（Meta-Object Compiler），用于 Qt 的信号槽机制
set(CMAKE_INCLUDE_CURRENT_DIR ON)      # 包含当前目录
set(CMAKE_BUILD_TYPE debug )           # 设置编译类型为 Debug
set(CMAKE_PREFIX_PATH "E:/QT/6.6.3/msvc2019_64/lib/cmake/Qt6")
# add_definitions ( -Wall )

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Boost COMPONENTS log log_setup thread filesystem system thread)
# 添加 Pylon,QT的 include 和库路径
include_directories(
    ${CMAKE_SOURCE_DIR}                   # 包含源代码目录
    ${CMAKE_CURRENT_BINARY_DIR}           # 包含当前二进制目录
    "C:/Program Files/Basler/pylon 8/Development/include"       # Pylon include 路径
    "E:/opencv/build/include"             # OpenCV include 路径
    Qt5::Core Qt5::Gui                    # Qt 模块
)

link_directories(
    "C:/Program Files/Basler/pylon 8/Runtime/x64"
    "C:/Program Files/Basler/pylon 8/Development/lib/x64"

)

# 获取 Qt5 Core 库的实际文件路径
get_target_property(QtCore_location Qt5::Core LOCATION)

# 设置源文件
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/grab_images2.cpp
)

# 设置头文件
set(HEADERS
    src/grab_images2.h
)

# 设置 Qt 使用的 UI 文件
set(UI_FILES
    src/mainwindow.ui
)
qt5_wrap_ui(UIS ${UI_FILES})

set(UI_MOCS
    src/mainwindow.h
    src/ui_mainwindow.h
)
qt5_wrap_cpp(MOCS ${UI_MOCS})

add_executable(InstrumentDashboard ${SOURCES} ${HEADERS} ${UI_FILES} ${UIS} ${MOCS})

# 链接 Pylon 库、OpenCV 库和 Qt 库
target_link_libraries(InstrumentDashboard Qt5::Core Qt5::Gui Qt5::Widgets
    ${PYLON_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${OpenCV_LIBS}  # OpenCV 库
)
