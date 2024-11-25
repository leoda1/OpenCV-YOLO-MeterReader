目录结构如下：
```markdown
├── CMakeLists.txt           # 项目主CMake构建文件
├── README.md                # 项目说明文件
├── src/                     # 源代码目录
│   ├── main.cpp
│   ├── data_handling/       # 数据采集和图片处理合并
│   ├── analysis/            # 图像分析和数据处理合并
│   ├── dial_rendering/
│   └── gui/
├── inc/                     # 头文件目录
│   ├── data_handling/
│   ├── analysis/
│   ├── dial_rendering/
│   └── gui/
├── resources/               # 资源文件（如配置、图标、样式表等）
├── third_party/             # 第三方库（如OpenCV、libtorch等）
├── tests/                   # 测试代码
│   ├── unit_tests/          # 单元测试
│   └── integration_tests/   # 集成测试
└── docs/                    # 文档
```