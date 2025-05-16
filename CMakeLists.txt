cmake_minimum_required(VERSION 3.16)
project(InstrumentDashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE debug)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_PREFIX_PATH "E:/QT/6.6.3/msvc2019_64/lib/cmake/Qt6" ${CMAKE_PREFIX_PATH})
set(OpenCV_DIR "E:/opencv/build/x64/vc16/lib")

find_package(OpenCV REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
set(PYLON_ROOT "C:/Program Files/Basler/pylon 8")
list(APPEND CMAKE_PREFIX_PATH "${PYLON_ROOT}")

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    "${PYLON_ROOT}/Development/include"
    "E:/opencv/build/include"
    Qt6::Core Qt6::Gui Qt6::Widgets
)

link_directories(
    "${PYLON_ROOT}/Runtime/x64"
    "${PYLON_ROOT}/Development/lib/x64"
)

# 获取 Qt6 Core 库的实际文件路径
get_target_property(QtCore_location Qt6::Core LOCATION)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/settings.cpp
    src/settingdialog.cpp
)

set(INC
    src/mainwindow.h
    src/settings.h
    src/settingdialog.h
)

set(UI
    src/mainwindow.ui
    src/settingdialog.ui
)
qt6_wrap_ui(UIS ${UI_FILES})
qt6_wrap_cpp(MOCS src/mainwindow.h src/settings.h src/settingdialog.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${INC} ${UIS} ${MOCS})

target_link_libraries(${PROJECT_NAME}
    Qt6::Core Qt6::Gui Qt6::Widgets
    ${OpenCV_LIBS}  # OpenCV 库
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PYLON_ROOT}/Runtime/x64"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/images"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/images)