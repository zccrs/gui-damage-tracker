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
  - `RendererNode`：自定义渲染节点，支持注册 `DamageFunction` 回调，根据轮到自己绘制时的当前整体 Damage 数据及渲染变换矩阵动态计算自适应 Damage 区域。
- 🎯 **多 Viewport 矩阵映射与独立计算**：
  - 单次遍历统一提取世界坐标损伤，针对每个 Viewport 的 `worldToOutput` 矩阵（支持缩放、旋转、偏移）进行精准映射。
  - 每个 Viewport 拥有独立的有效损伤区域（`visibleDamage`）与遮挡判定。
  - **Swapchain / Buffer Age 原生支持**：`Viewport` 结构体支持携带自身由于双缓冲/三缓冲切换产生的额外 Buffer Damage（`vp.damage`），与场景图计算出的 Damage 完美求并融合。
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

### Swapchain / Buffer Age 多缓冲支持示例

```cpp
// 在双缓冲或三缓冲图形管线中，当前渲染目标 Buffer 可能需要补全历史帧遗漏的区域 (Buffer Damage)
Tracker::Viewport vp;
vp.id = 1;
vp.outputRect = QRect(0, 0, 1920, 1080);
vp.damage = getSwapchainBufferAgeDamage(); // 传入当前输出 Buffer 自身的历史 Damage

auto result = tracker.commit({vp});
// 返回的 Damage 自动融合了场景图计算出的增量 Damage 与该 Buffer 需补偿的 Swapchain Damage
QRegion finalBufferDamage = result.value(1);
```

### RendererNode 自定义渲染节点示例

```cpp
auto *customRenderer = new RendererNode();
customRenderer->setBoundingRect(QRectF(0, 0, 300, 200));

// 设置自定义 Damage 计算函数
customRenderer->setDamageFunction([](const RenderContext &ctx) -> QRegion {
    // ctx 包含当前累积的 overallDamage、世界矩阵 worldTransform、最终渲染矩阵 renderMatrix 等
    if (!ctx.overallDamage.isEmpty() && ctx.overallDamage.intersects(ctx.worldBounds)) {
        // 当后方内容发生变化时，自身产生带有 20px 光晕外扩的重绘损伤
        return ctx.worldBounds.adjusted(-20, -20, 20, 20);
    }
    return ctx.worldBounds;
});
root->appendChild(customRenderer);
```

---

## 性能基准测试 (Performance Benchmark)

基准测试在 Linux x86_64 环境（Intel Core i7-10700 CPU @ 2.90GHz）下通过 `tests/bench_gdt` 执行，涵盖不同树规模（50 ~ 5000 节点）、多视口配置（1 ~ 4 屏幕）及各类典型 GUI 破损场景。

### 1. 基准测试结果汇总 (实测最新数据)

| 场景分类 (Scenario) | 节点数 (Nodes) | Viewport 数 | 平均单帧耗时 (Avg Time) | 最大理论吞吐量 | 60Hz 帧预算占比 | 144Hz 帧预算占比 | 240Hz 帧预算占比 | 360Hz 帧预算占比 |
|---|---|---|---|---|---|---|---|---|
| **空闲帧 (无脏位快速路径)** | 50 | 1 VP | **1.50 µs** | 666,504 FPS | 0.009% | 0.022% | 0.036% | 0.054% |
| **空闲帧 (无脏位快速路径)** | 200 | 1 VP | **1.53 µs** | 652,142 FPS | 0.009% | 0.022% | 0.037% | 0.055% |
| **空闲帧 (无脏位快速路径)** | 1000 | 2 VP | **2.22 µs** | 450,410 FPS | 0.013% | 0.032% | 0.053% | 0.080% |
| **空闲帧 (无脏位快速路径)** | 5000 | 4 VP | **4.51 µs** | 221,820 FPS | 0.027% | 0.065% | 0.108% | 0.162% |
| **局部内容损伤 (16x16 px)** | 50 (常规控件) | 1 VP | **88.98 µs** | 11,238 FPS | 0.534% | 1.281% | 2.136% | 3.201% |
| **局部内容损伤 (16x16 px)** | 200 (复杂桌面) | 1 VP | **367.60 µs** | 2,720 FPS | 2.206% | 5.293% | 8.822% | 13.223% |
| **局部内容损伤 (16x16 px)** | 200 (复杂桌面) | 4 VP (四屏) | **288.50 µs** | 3,466 FPS | 1.731% | 4.154% | 6.924% | 10.378% |
| **局部内容损伤 (16x16 px)** | 1000 (合成器全树) | 1 VP | **429.73 µs** | 2,327 FPS | 2.578% | 6.188% | 10.314% | 15.458% |
| **局部内容损伤 (16x16 px)** | 5000 (超大树四屏) | 4 VP | **1188.17 µs** | 842 FPS | 7.129% | 17.110% | 28.516% | 42.740% |
| **单节点几何移动 (平移)** | 200 | 1 VP | **373.22 µs** | 2,679 FPS | 2.239% | 5.374% | 8.957% | 13.425% |
| **子树层级旋转 (Transform)** | 200 | 1 VP | **248.71 µs** | 4,021 FPS | 1.492% | 3.581% | 5.969% | 8.946% |
| **背景采样扩散 (Backdrop)** | 1000 | 1 VP | **440.24 µs** | 2,272 FPS | 2.641% | 6.339% | 10.566% | 15.836% |
| **高密度多节点损坏 (10% 脏)**| 1000 | 1 VP | **526.51 µs** | 1,899 FPS | 3.159% | 7.582% | 12.636% | 18.939% |

### 2. 刷新率与帧时间预算分析 (Frame Budget Analysis)

在现代图形界面中，CPU 损伤计算应控制在整帧预算的 **10% 以下**，以留出充足的时间给 GPU 进行光栅化与合成：

```
+-------------------+------------------+---------------------+-----------------------+
| 刷新率 (Hz)       | 单帧总时间预算   | 200 节点桌面耗时占比 | 1000 节点大型场景耗时占比|
+-------------------+------------------+---------------------+-----------------------+
| 60 Hz             | 16.67 ms         | 2.21% (~0.37 ms)    | 2.58% (~0.43 ms)      |
| 120 Hz            | 8.33 ms          | 4.41% (~0.37 ms)    | 5.16% (~0.43 ms)      |
| 144 Hz (电竞高刷) | 6.94 ms          | 5.29% (~0.37 ms)    | 6.19% (~0.43 ms)      |
| 240 Hz (超高刷)   | 4.17 ms          | 8.82% (~0.37 ms)    | 10.31% (~0.43 ms)     |
| 360 Hz (极限高刷) | 2.78 ms          | 13.22% (~0.37 ms)   | 15.46% (~0.43 ms)     |
+-------------------+------------------+---------------------+-----------------------+
```

- **深度子树视口裁剪**：当整个子树位于显示视口外部时，直接 $O(1)$ 剪枝，多显示器下开销大幅下降。
- **干净透明节点极速路径**：对绝大多数静态且无遮挡贡献的节点，避免昂贵的 `QRegion` 差集运算，1000 节点计算耗时降至 **0.43 ms**。

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
