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

### 3 表盘参数
#### BYQ:
几何参数
半径尺寸
R16.1-R17.1: 彩色带
R15.1-R16.1: 小刻度线（线宽是0.2）
R14.6-R16.1: 大刻度线 （线宽是0.4）
R14: 数字标注位置半径 （2A按 HB5888）
α (alpha): 总弧度角度，约100°
文字标注参数
主文字: "MPa"
位置: 表盘中央圆心上方7(字体：2PA、2Pb:按HB5888)
#### YYQY
一、圆弧和半径结构
表盘总角度：α 可以先初始化一个300度，但是不管是280度还是300度 都是表盘形状都是左右对称的 刻度不对对称的 因为最大的6.3MPa
R15.5-R16.3：彩色带（0-4MPa是黑色，4-6.3Mpa是红色）
R13.8-R15.5：小刻度线 （线宽是0.3）
R12-R15.5：大刻度线（线宽是0.8）
然后R-11.8往圆心内是3号黑体刻度数字0，1，2，3，4，5，6
圆心正上方R3位置往上也有一个2号黑体“MPa”
二、刻度与数字
总共60个刻度线，分布于360度圆周（每6度一线）
主刻度线（数字1~6对应位置）更长
副刻度线更短（位于主刻度之间）
每个主刻度对应数字：1、2、3、4、5、6，位置对应整点（30°为单位）
三、文字与图形元素
“禁油”： 圆心左边R2.5-R6.5 3号黑体
“氧气”： 圆心右边R2.5-R6.5 3号黑体， 氧气有一个蓝色 宽0.5圆角的酚酞蓝色PB06下划线
四、定位的小黑点
0刻度线减1度 然后在R11.5的位置，是一个Φ1 H11通孔，所以需要刻度0这个数字微微向上边偏移一点点确保不会和这个小黑点重合
五、商标

六、导出尺寸
格式：tiff
分辨率：1200x1200
尺寸：1890x1890
### 进度
// 检测点：init 问题
// 理论角度根据相机检测直接得到 或 直接显示最终角度
// 满量程角度提供外在的输入（0到满）
// 迟滞当前的数据需要确定当前能不能接着往下走
// 第七个点不参与计算 只涉及到总角度
// 初始的时候需要测个几次来确定最大角度