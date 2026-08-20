# GUI Damage Tracker (gdt)

[![Build & Tests](https://img.shields.io/badge/tests-passing-brightgreen.svg)]()
[![Qt](https://img.shields.io/badge/Qt-6.11%2B-blue.svg)]()
[![C++](https://img.shields.io/badge/C%2B%2B-17-purple.svg)]()
[![License](https://img.shields.io/badge/license-LGPL%2FMIT-green.svg)]()

`gui-damage-tracker` (gdt) 是一个面向现代化 GUI 框架与 Wayland 合成器（如 [Treeland](https://github.com/linuxdeepin/treeland) / QtQuick）的**场景图损伤预计算与精确跟踪库**。

它提供类似于 `QSGNode` 的树形场景图管理，支持 2D 矩阵变换、不透明区域判定、遮挡剔除以及特殊的**背景采样扩散损伤（Backdrop Node）**，并能够针对多个带独立矩阵映射的视口（Viewport / Display Output）并行计算渲染所需的最小损伤区域与节点剔除状态。

---

## 核心特性 (Features)

- 🌲 **轻量级场景图抽象**：
  - `Basic Node`：空白容器节点，用于划分层级子树。
  - `TransformNode`：2D 仿射变换节点（平移、旋转、缩放、组合矩阵）。
  - `GeometryNode`：具有边界尺寸、内容脏区及不透明区域（`isFullyOpaque` / `opaqueRegion`）的可视内容节点。
  - `BackdropNode`：类似毛玻璃/背景模糊效果的采样节点，具有外扩半径（`backdropExpansion`），背后内容更新时自动向外发散损伤。
- 🎯 **多 Viewport 矩阵映射与独立计算**：
  - 单次遍历统一提取世界坐标损伤，针对每个 Viewport 的 `worldToOutput` 矩阵（支持缩放、旋转、偏移）进行精准映射。
  - 每个 Viewport 拥有独立的有效损伤区域（`visibleDamage`）与遮挡判定。
- ⚡ **遮挡剔除与跳过绘制 (Occlusion Culling)**：
  - 从前向后（Top-to-Bottom）动态累积不透明区域（`opaqueRegion`）。
  - 被完全遮挡的节点自动标记 `isCulled(vpId) == true`，在渲染遍历时直接跳过，显著降低 GPU 渲染带宽与 Draw Call。
- 🔍 **双缓冲损伤发散与暴露修复 (Exposure Damage)**：
  - 图元移动、形变或层级调整时，精准产生**旧位置修复损伤（Old Rect Damage）**与**新位置绘制损伤（New Rect Damage）**。
  - 前景移开时被遮挡的后方静止内容自动触发**局部暴露重绘**。
- 🚀 **极低 CPU 开销**：
  - 内置无脏位快速路径（Fast-path），空闲帧耗时仅 **1.5 µs**。
  - 复杂 200 节点桌面界面单帧耗时仅 **0.4 ms**（占 144Hz 帧预算的 5.7%）。
- 🖥️ **交互式 QML 调试器与实时可视化**：
  - 实时查看树形层级、拖拽调整父子关系与兄弟顺序。
  - 红色（当前帧最新损伤）与绿色（历史历史帧损伤）分层高亮呈现。

---

## 架构与原理 (Architecture)

```
                       [ Tracker::commit(viewports) ]
                                     |
              +----------------------+----------------------+
              |                                             |
     [ 1. 脏位传播与世界变换 ]                      [ 2. 背景采样发散 ]
 (Clean check & World Rect Calc)            (Backdrop Node expansion)
              |                                             |
              +----------------------+----------------------+
                                     |
                         [ 3. 世界 Damage 汇聚 ]
                 (World-space Damage Region Union)
                                     |
              +----------------------+----------------------+
              |                      |                      |
     [ Viewport 0 (Primary) ]  [ Viewport 1 (4K Scaled) ]  [ Viewport 2 (Rotated) ]
     - worldToOutput 投影      - worldToOutput 投影       - worldToOutput 投影
     - 局部 Damage 裁剪        - 局部 Damage 裁剪         - 局部 Damage 裁剪
     - 从前向后遮挡累积        - 从前向后遮挡累积         - 从前向后遮挡累积
     - 节点 Culling 状态更新   - 节点 Culling 状态更新    - 节点 Culling 状态更新
```

---

## 快速上手 (Quick Start)

### CMake 集成

```cmake
add_subdirectory(gui-damage-tracker)
target_link_libraries(your_project PRIVATE gdt)
```

### C++ 使用示例

```cpp
#include <gdt.h>

using namespace Gdt;

// 1. 创建场景图
auto *root = new Node(Node::Type::Basic);

auto *transform = new TransformNode();
transform->setTranslation(100, 50);
root->appendChild(transform);

auto *card = new GeometryNode();
card->setBoundingRect(QRectF(0, 0, 400, 300));
card->setFullyOpaque(true); // 声明为不透明图元，参与遮挡计算
transform->appendChild(card);

// 2. 初始化损伤跟踪器
Tracker tracker(root);

// 3. 配置视口列表 (例如双显示器输出)
QVector<Tracker::Viewport> viewports;

Tracker::Viewport vp1;
vp1.id = 1;
vp1.outputRect = QRect(0, 0, 1920, 1080);
vp1.worldToOutput = QTransform(); // 1:1 无缩放

Tracker::Viewport vp2;
vp2.id = 2;
vp2.outputRect = QRect(0, 0, 3840, 2160);
QTransform t2;
t2.scale(2.0, 2.0); // 2x 缩放 4K 屏
vp2.worldToOutput = t2;

viewports.push_back(vp1);
viewports.push_back(vp2);

// 4. 提交计算当前帧损伤
tracker.commit(viewports);

// 5. 获取各视口损伤与渲染节点
QRegion damageVp1 = tracker.damage(1);
qDebug() << "Viewport 1 Damage:" << damageVp1;

// 检查节点在指定视口中是否被剔除
if (!card->isCulled(1)) {
    // 执行渲染绘制...
}
```

---

## 性能基准测试 (Performance Benchmark)

基准测试在 Linux x86_64 环境（Intel Core i7-10700 CPU @ 2.90GHz）下通过 `tests/bench_gdt` 执行，涵盖不同树规模（50 ~ 5000 节点）、多视口配置（1 ~ 4 屏幕）及各类典型 GUI 破损场景。

### 1. 基准测试结果汇总

| 场景分类 (Scenario) | 节点数 (Nodes) | Viewport 数 | 平均单帧耗时 (Avg Time) | 最大理论吞吐量 | 60Hz 帧预算占比 | 144Hz 帧预算占比 | 240Hz 帧预算占比 | 360Hz 帧预算占比 |
|---|---|---|---|---|---|---|---|---|
| **空闲帧 (无脏位快速路径)** | 50 | 1 VP | **1.52 µs** | 658,218 FPS | 0.009% | 0.022% | 0.036% | 0.055% |
| **空闲帧 (无脏位快速路径)** | 200 | 1 VP | **1.52 µs** | 658,175 FPS | 0.009% | 0.022% | 0.036% | 0.055% |
| **空闲帧 (无脏位快速路径)** | 1000 | 2 VP | **2.23 µs** | 448,614 FPS | 0.013% | 0.032% | 0.053% | 0.080% |
| **空闲帧 (无脏位快速路径)** | 5000 | 4 VP | **3.55 µs** | 281,363 FPS | 0.021% | 0.051% | 0.085% | 0.128% |
| **局部内容损伤 (16x16 px)** | 50 (常规控件) | 1 VP | **81.06 µs** | 12,336 FPS | 0.486% | 1.167% | 1.945% | 2.918% |
| **局部内容损伤 (16x16 px)** | 50 (常规控件) | 2 VP (双屏) | **109.28 µs** | 9,151 FPS | 0.656% | 1.574% | 2.623% | 3.934% |
| **局部内容损伤 (16x16 px)** | 200 (复杂桌面) | 1 VP | **398.83 µs** | 2,507 FPS | 2.393% | 5.743% | 9.572% | 14.358% |
| **局部内容损伤 (16x16 px)** | 200 (复杂桌面) | 2 VP (双屏) | **557.12 µs** | 1,795 FPS | 3.343% | 8.023% | 13.371% | 20.056% |
| **局部内容损伤 (16x16 px)** | 1000 (合成器全树) | 1 VP | **1084.30 µs** | 922 FPS | 6.506% | 15.614% | 26.023% | 39.034% |
| **单节点几何移动 (平移)** | 50 | 1 VP | **96.18 µs** | 10,397 FPS | 0.577% | 1.385% | 2.308% | 3.462% |
| **单节点几何移动 (平移)** | 200 | 1 VP | **462.66 µs** | 2,161 FPS | 2.776% | 6.662% | 11.104% | 16.656% |
| **子树层级旋转 (Transform)** | 200 | 1 VP | **283.29 µs** | 3,530 FPS | 1.700% | 4.079% | 6.799% | 10.198% |
| **背景采样扩散 (Backdrop)** | 200 | 1 VP | **403.37 µs** | 2,479 FPS | 2.420% | 5.808% | 9.681% | 14.521% |
| **高密度多节点损坏 (10% 脏)**| 200 | 1 VP | **419.51 µs** | 2,384 FPS | 2.517% | 6.041% | 10.068% | 15.102% |

### 2. 刷新率与帧时间预算分析 (Frame Budget Analysis)

在现代图形界面中，CPU 损伤计算应控制在整帧预算的 **10% 以下**，以留出充足的时间给 GPU 进行光栅化与合成：

```
+------------------+------------------+---------------------+-----------------------+
| 刷新率 (Hz)      | 单帧总时间预算   | 50 节点 GUI 耗时占比 | 200 节点复杂桌面耗时占比|
+------------------+------------------+---------------------+-----------------------+
| 60 Hz            | 16.67 ms         | 0.48% (~0.08 ms)    | 2.39% (~0.40 ms)      |
| 120 Hz           | 8.33 ms          | 0.97% (~0.08 ms)    | 4.78% (~0.40 ms)      |
| 144 Hz           | 6.94 ms          | 1.17% (~0.08 ms)    | 5.74% (~0.40 ms)      |
| 240 Hz (电竞级)  | 4.17 ms          | 1.95% (~0.08 ms)    | 9.57% (~0.40 ms)      |
| 360 Hz (极限高刷)| 2.78 ms          | 2.92% (~0.08 ms)    | 14.36% (~0.40 ms)     |
+------------------+------------------+---------------------+-----------------------+
```

- **极速空闲检测**：当界面无元素变化时（如静态展示），Tracker 自动触发 `O(1)` 级别的干净状态快速短路，耗时仅 **1.5 微秒**，几乎零 CPU 开销。
- **高刷新率从容应对**：在 144Hz ~ 240Hz 甚至 360Hz 极限高刷场景下，常规桌面界面（200 个图形节点）的损伤计算与遮挡剔除仅消耗不到 10% 的帧时间，完美满足丝滑交互要求。

---

## 交互式调试演示工具 (Damage Demo Inspector)

项目内置了功能丰富的 QML 交互式调试器 (`damage-demo`)，可用于直观体验和验证损伤跟踪原理：

```bash
# 启动交互式演示
./build/examples/damage-demo/damage-demo
```

### 演示功能：
1. **多内置典型场景**：
   - 基础平移 (Translation)
   - 矩阵缩放 (Scale)
   - 连续旋转 (Rotation)
   - 遮挡与重叠 (Occlusion)
   - 揭露后方节点 (Reveal / Exposure)
   - 背景采样模糊扩张 (Backdrop Blur Dilation)
2. **直观损伤分层高亮**：
   - **红色矩形**：当前帧产生的最新 Damage。
   - **绿色矩形**：历史帧 Damage 衰减拖尾，清晰观察运动轨迹。
3. **可视化节点树拖拽**：
   - 树节点支持拖拽排序与调整父子层级。
   - 画布图元支持实时抓取拖动，自动同步更新损伤并反映在属性面板中。

---

## 构建与测试 (Build & Test)

```bash
# 配置并编译
mkdir build && cd build
cmake ..
cmake --build .

# 运行全量单元测试与基准测试
ctest --output-on-failure
```

---

## 授权协议 (License)

本项目遵循 LGPL-2.1+ / MIT 双重授权协议。
