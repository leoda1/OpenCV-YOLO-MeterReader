@ [11.25-12.02的工作内容](11.25-12.02的工作内容)

&emsp;&emsp;这个项目的开发配置：代码IDEA选了vscode，因为basler和QT需要visual studio，所以我直接vscode + cmake + vs2019 + qt来实现整个过程。其中qt需要使用科大云才能完成下载，避雷qsinghua源，报错n次。

&emsp;&emsp;接下来第一件事情是手上这个basler相机,我应该怎么用官方给的SDK实现我要的功能（1.开启相机 2.record各种时间间隔的图片 3.多次拍摄 4.对相机的配置能够超参数修改 5.简单的图片处理 6.关闭相机 7.相机连接失败的时候怎么删除相机设备并重新连接。。。）这里的参考资料为：[官方的SDK开发指南](https://docs.baslerweb.com/pylonapi/cpp/pylon_programmingguide#common-settings-for-building-applications-with-pylon-windows
) 和 pylon/development/samples/c++目录下的样例实现上述功能。

* 先将运行时，链接库，debug项目配置的路径加到cmakelists.txt，这里是参考开发指南的location on windows进行配置项目的。
```sh
# 指定 Pylon 安装目录（修改为实际路径）
set(PYLON_ROOT "C:/Program Files/Basler/pylon 8")

# 添加 Pylon 的 include 和库路径
include_directories("${PYLON_ROOT}/Development/include")
include_directories("inc")
link_directories("${PYLON_ROOT}/Development/lib/x64")
```
&emsp;&emsp;后续在项目的inc/analysis路径下加了CameraEventPrinter.h, ConfigurationEventPrinter.h, ImageEventPrinter.h, PixelFormatAndAoiConfiguration.h, SampleImageCreator.h这五个官方给的Include Files，因为接下来要用到Samples中的派生类去实现我以上的功能，所以就将这些头文件copy了过来。

* 首先第一个测试用例，grab_images.cpp。参考官方的smaples实现了上述1，2，3，5，6，7，暂时还没想好相机的拍摄配置该怎么调，图是拍512x512还是更高的3840x2738再resize，光圈，对焦距离，成像色彩等等，所以暂时能拍就行，等后续确定了技术路线的输入图片要求后再返工这部分代码。
```sh
cmake_minimum_required(VERSION 3.16)

# 项目名称和版本
project(grab_images  VERSION 1.0 LANGUAGES CXX)


# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 指定 Pylon 安装目录（修改为实际路径）
set(PYLON_ROOT "C:/Program Files/Basler/pylon 8")

# 添加 Pylon 的 include 和库路径
include_directories("${PYLON_ROOT}/Development/include")
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
)
```
测试结果图如下：（当前的清晰度，对焦举例，光圈大小等等都没有进行微调。。。，默认拍摄是3840x2748分辨率图片）
![alt text](fig_doc\image1.png)
