cmake_minimum_required(VERSION 3.14)
project(your_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找第三方库
find_package(OpenCV REQUIRED)
find_package(Torch REQUIRED)  # libtorch
find_package(Qt5 COMPONENTS Widgets Core Gui REQUIRED)

# 包含子目录
add_subdirectory(data_acquisition)
add_subdirectory(image_processing)
add_subdirectory(image_analysis)
add_subdirectory(data_processing)
add_subdirectory(dial_rendering)
add_subdirectory(gui)

# 可执行文件
add_executable(your_project main.cpp)

# 链接库
target_link_libraries(your_project
    PRIVATE
    data_acquisition_lib
    image_processing_lib
    image_analysis_lib
    data_processing_lib
    dial_rendering_lib
    gui_lib
    ${OpenCV_LIBS}
    ${TORCH_LIBRARIES}
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
)

# 包含头文件路径
target_include_directories(your_project PRIVATE
    data_acquisition/include
    image_processing/include
    image_analysis/include
    data_processing/include
    dial_rendering/include
    gui/include
)
