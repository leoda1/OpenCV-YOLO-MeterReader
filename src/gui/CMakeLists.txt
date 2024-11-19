# gui/CMakeLists.txt

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 收集源文件
file(GLOB GUI_SOURCES src/*.cpp)
file(GLOB GUI_HEADERS include/*.h)
file(GLOB GUI_FORMS forms/*.ui)
file(GLOB GUI_RESOURCES resources/*.qrc)

# 添加库
add_library(gui_lib ${GUI_SOURCES} ${GUI_HEADERS} ${GUI_FORMS} ${GUI_RESOURCES})

# 链接Qt库
target_link_libraries(gui_lib PRIVATE Qt5::Widgets Qt5::Core Qt5::Gui)

# 包含头文件路径
target_include_directories(gui_lib PUBLIC include)
