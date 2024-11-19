目录结构如下：
```markdown
├── CMakeLists.txt           # 项目主CMake构建文件
├── README.md                # 项目说明文件
├── data_acquisition/        # 数据采集模块
│   ├── include/             # 头文件
│   └── src/                 # 源代码
├── image_processing/        # 图片处理模块
│   ├── include/
│   └── src/
├── image_analysis/          # 图像分析模块
│   ├── include/
│   ├── src/
│   ├── opencv_method/       # OpenCV算法实现
│   └── deep_learning/       # 深度学习模型实现
├── data_processing/         # 数据处理模块
│   ├── include/
│   └── src/
├── dial_rendering/          # 表盘绘制模块
│   ├── include/
│   └── src/
├── gui/                     # 客户端界面模块
│   ├── include/
│   └── src/
├── third_party/             # 第三方库（如OpenCV、libtorch等）
├── resources/               # 资源文件（配置、模板等）
├── tests/                   # 测试代码
│   ├── unit_tests/          # 单元测试
│   └── integration_tests/   # 集成测试
└── docs/                    # 文档
```