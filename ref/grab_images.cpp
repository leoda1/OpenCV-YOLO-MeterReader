/*
    该程序使用Basler Pylon SDK实现以下功能：
    1. 开启相机
    2. 按照各种时间间隔拍摄图片
    3. 多次拍摄
    4. 可以修改相机的超参数配置
    5. 简单的图像处理
    6. 关闭相机
    7. 当相机连接失败时，删除相机设备并重新连接
*/

// 包含使用Pylon API的头文件
#include <pylon/CameraEventHandler.h>
#include <pylon/PylonIncludes.h>
#ifdef PYLON_WIN_BUILD
#include <pylon/PylonGUI.h>
#endif
#include <iostream>
#include <chrono>
#include <thread>
// 命名空间
using namespace GenApi;
using namespace std;
using namespace Pylon;

// 定义拍摄的图像数量
static const uint32_t c_countOfImagesToGrab = 10;

int main(int argc, char* argv[]) {
    int exitCode = 0;                       // 程序的退出代码
    Pylon::PylonAutoInitTerm autoInitTerm;  // 自动初始化Pylon资源，并在程序退出时释放
    PylonInitialize();                      // 在使用任何Pylon方法之前，必须初始化Pylon运行时

    try {
        CInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());       // 创建相机对象
        cout << "Using device: " << camera.GetDeviceInfo().GetModelName() << endl;  // 打印相机的型号名称
        camera.Open(); // 打开相机

        // 设置相机参数，例如曝光时间和增益（超参数配置）
        // 这里可以根据需要修改相机的参数
        INodeMap& nodemap = camera.GetNodeMap();
        CFloatPtr exposureTime = nodemap.GetNode("ExposureTime");
        if (IsWritable(exposureTime))
        {
            exposureTime->SetValue(10000.0); // 设置曝光时间为10000微秒
        }

        camera.StartGrabbing(); // 开始抓取图像

        CGrabResultPtr ptrGrabResult; // 定义图像抓取结果指针

        int captureCount = 5; // Number of captures (according to task requirements)

        for (int count = 0; count < captureCount; ++count)
        {
            int intervalMilliseconds = 1000; // 每隔1秒拍摄一次
            for (int i = 0; i < c_countOfImagesToGrab; ++i)
            {
                // 抓取一张图片，超时时间为5000ms
                if (camera.RetrieveResult(5000, ptrGrabResult, TimeoutHandling_ThrowException))
                {
                    // 判断是否抓取成功
                    if (ptrGrabResult->GrabSucceeded())
                    {
                        // 获取图像缓冲区
                        const uint8_t* pImageBuffer = (uint8_t*)ptrGrabResult->GetBuffer();

                        // 进行简单的图像处理，例如打印第一像素的灰度值
                        cout << "Captured image " << i + 1 << endl;
                        cout << "Image dimensions: " << ptrGrabResult->GetWidth() << " x " << ptrGrabResult->GetHeight() << endl;
                        cout << "First pixel intensity: " << (uint32_t)pImageBuffer[0] << endl;
#ifdef PYLON_WIN_BUILD
                        Pylon::DisplayImage(1, ptrGrabResult); // 显示抓取的图像
#endif
                    }
                    else {
                        cout << "Capture error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << endl; // 抓取失败，输出错误信息
                    }
                }

                // 等待指定的时间间隔
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMilliseconds));
                // Pylon::PylonSleep(intervalMilliseconds);
            }
            cout << "finish di" << count + 1 << "grab" << endl;
        }
        camera.StopGrabbing(); // 停止抓取
        camera.Close(); // 关闭相机
    }
    catch (const GenericException& e) {
        cerr << "An exception occurred: " << e.GetDescription() << endl; // 错误处理
        cerr << "Attempting to reconnect the camera..." << endl; // 当相机连接失败时，删除相机设备并重新连接
        try {
            CInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice()); // 重新初始化相机
            camera.Open(); // 打开相机
        }
        catch (const GenericException& e) {
            cerr << "Failed to reconnect the camera: " << e.GetDescription() << endl; // 重新连接相机失败
            exitCode = 1;
        }
    }
    PylonTerminate(); // 释放Pylon资源
    return exitCode;
}