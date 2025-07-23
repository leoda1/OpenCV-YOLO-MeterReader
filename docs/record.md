## [工作内容]()

&emsp;&emsp;这个项目的开发配置：代码IDEA选了vscode，因为basler和QT需要visual studio，所以我直接vscode + cmake + vs2019 + qt来实现整个过程。其中qt需要使用科大云才能完成下载，避雷qsinghua源，报错n次。
## 1.相机采集
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

### 1.1 第一版本
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
测试结果图如下：（用了pylon viewer软件直接了当了修改分辨率为1920x1080 并手动将相机的焦距和对焦进行了调整，当前可以看清楚图片中内容）
![alt text](fig_doc\image1.png)

### 1.1 第二版本
当前对grab_images.cpp中的功能进行针对项目的具体化修改。
生成grab_images.exe的指令是：
```sh
cmake -B build -G "Visual Studio 16 2019"
cmake --build build
```
* 问题1：测试的时候发现中文无法显示，所以需要打印的文字都使用英文
* 问题2： exposureTime（代码中曝光时间怎么设定的）
    * 这里我通过pylon viewer软件来测试相机在20cm对焦距离内拍清楚需要多少曝光和光圈 确定了是35000us，这里根据测试结果在代码中进行修改。
        ```cpp
            exposureTime->SetValue(35000.0); // 设置曝光时间为35000微秒
        ```
* 问题3：
## 2 识别表盘和指针
### 2.1 采集后处理
&emsp;&emsp;目前这个grab_images.cpp需要将功能封装成一个类，涉及到三个小阶段的内容

1. 封装后能要提供接口给QT，点击开始的时候能一次完成10个（时间间隔/压力控制器数据的间隔）的拍摄并处理10张图片后返回结果。
2. 两个表盘可能需要一次将类派生出两种对象实现不同的细节要求。
3. 每组图片数据应该有一个队列去存，先存的先删，后存的后删，能够保持一个大概存取7天内的结果内存。

### 2.2 识别算法部分
#### 2.2.1 OpenCV来实现指针识别
&emsp;&emsp;使用OpenCV表盘1和2的指针角度实时识别。Opencv_hp.h是封装后的实现，具体实现代码是Opencv_hp.cpp。
* 任务包括两种表盘 首先从scale1和scale2的角度两种方法实现 其中获取表盘、圆心、半径等等为通用计算函数，用protect保护，数据部分则正常private，在highPreciseDetector这个类中我们只用showScale1Result和showScale2Result返回两种表盘的结果。
* 具体实现原理为cpp中，首先

#### 2.2.2 YoloV8-Pose定位指针关键点来实现指针识别
&emsp;&emsp;Nanodet+YoloV8-Pose实现指针仪表的实时检测。



// 标记的位置选中后可以修改， 字体默认到黑体 完成
// 绘制表盘的预览和保存的位置不同 完成

// 导出表格
配置压力表参数： 完成
产品型号、名称、刻度盘图号、支组编号 完成
满量程压力（默认3.0 MPa）完成
满量程角度（默认270°） 完成
基本误差限值（默认±0.1 MPa） 完成
迟滞误差限值（默认0.15 MPa） 完成
设置检测点：默认有0、1、2、3 MPa四个检测点，可以添加或删除 完成

// 检测点配置 完成
// 产品型号 完成
// 确定excle闭环逻辑 待确定