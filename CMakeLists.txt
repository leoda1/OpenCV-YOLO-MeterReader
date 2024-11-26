cmake_minimum_required(VERSION 3.16)

# 项目名称和版本
project(grab_images  VERSION 1.0 LANGUAGES CXX)


# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 指定 Pylon 安装目录（修改为实际路径）
set(PYLON_ROOT "C:/Program Files/Basler/pylon 8")

# 添加 Pylon 的 include 和库路径
include_directories("${PYLON_ROOT}/Development/include" "E:/opencv/build/include")
include_directories("inc")
link_directories("${PYLON_ROOT}/Development/lib/Win32")

# 自动添加当前项目的所有源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 自动添加当前项目的所有头文件
file(GLOB_RECURSE HEADERS "inc/*.h")

# 添加可执行文件
add_executable(grab_images src/analysis/grab_images.cpp)

# 链接 Pylon 库
target_link_libraries(grab_images PRIVATE
    PylonBase_v9
    GenApi_MD_VC141_v3_1_Basler_pylon
    PylonUtility_v9
    GCBase_MD_VC141_v3_1_Basler_pylon
    ${OpenCV_LIBS}
)
