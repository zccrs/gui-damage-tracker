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
  - `Node`：场景图基类，支持 `hasContent()` 控制自身是否绘制，支持 `setVisible()` 控制整棵子树可见性。
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

// 4. 提交计算当前帧损伤 (无状态就地计算，结果直接写入各 Viewport::state)
tracker.commit(viewports);

// 5. 直接从对应视口获取 Damage 与节点剔除状态
qDebug() << "Viewport 1 Damage:" << viewports[0].state.damage;

// 检查节点在指定视口中是否被剔除
if (!viewports[0].state.isCulled(card)) {
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

QVector<Tracker::Viewport> vps{vp};
tracker.commit(vps);

// 结果直接保存在 vps[0].state 中，自动融合了场景图增量 Damage 与 Swapchain 补偿 Damage
QRegion finalBufferDamage = vps[0].state.damage;
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

### 1. 基准测试结果汇总 (Attached Data 零 QHash 实测最新数据)

| 场景分类 (Scenario) | 节点数 (Nodes) | Viewport 数 | 平均单帧耗时 (Avg Time) | 最大理论吞吐量 | 60Hz 帧预算占比 | 144Hz 帧预算占比 | 240Hz 帧预算占比 | 360Hz 帧预算占比 |
|---|---|---|---|---|---|---|---|---|
| **空闲帧 (无脏位快速路径)** | 50 | 1 VP | **0.41 µs** | 2,456,329 FPS | 0.002% | 0.006% | 0.010% | 0.015% |
| **空闲帧 (无脏位快速路径)** | 200 | 1 VP | **0.53 µs** | 1,886,520 FPS | 0.003% | 0.008% | 0.013% | 0.019% |
| **空闲帧 (无脏位快速路径)** | 1000 | 2 VP | **0.60 µs** | 1,660,182 FPS | 0.004% | 0.009% | 0.014% | 0.022% |
| **空闲帧 (无脏位快速路径)** | 5000 | 4 VP | **1.08 µs** | 926,999 FPS | 0.006% | 0.016% | 0.026% | 0.039% |
| **局部内容损伤 (16x16 px)** | 50 (常规控件) | 1 VP | **67.38 µs** | 14,842 FPS | 0.404% | 0.970% | 1.617% | 2.424% |
| **局部内容损伤 (16x16 px)** | 200 (复杂桌面) | 1 VP | **284.87 µs** | 3,510 FPS | 1.709% | 4.102% | 6.837% | 10.247% |
| **局部内容损伤 (16x16 px)** | 1000 (合成器全树) | 1 VP | **367.49 µs** | 2,721 FPS | 2.205% | 5.292% | 8.820% | 13.219% |
| **局部内容损伤 (16x16 px)** | 5000 (超大树四屏) | 4 VP | **720.95 µs** | 1,387 FPS | 4.326% | 10.382% | 17.303% | 25.933% |
| **单节点几何移动 (平移)** | 200 | 1 VP | **365.28 µs** | 2,738 FPS | 2.192% | 5.260% | 8.767% | 13.140% |
| **单节点几何移动 (平移)** | 1000 | 1 VP | **387.02 µs** | 2,584 FPS | 2.322% | 5.573% | 9.289% | 13.922% |
| **单节点几何移动 (平移)** | 5000 | 1 VP | **829.53 µs** | 1,206 FPS | 4.977% | 11.945% | 19.909% | 29.839% |
| **子树层级旋转 (Transform)** | 200 | 1 VP | **239.88 µs** | 4,169 FPS | 1.439% | 3.454% | 5.757% | 8.629% |
| **子树层级旋转 (Transform)** | 1000 | 1 VP | **772.21 µs** | 1,295 FPS | 4.633% | 11.120% | 18.533% | 27.777% |
| **背景采样扩散 (Backdrop)** | 1000 | 1 VP | **356.83 µs** | 2,802 FPS | 2.141% | 5.138% | 8.564% | 12.836% |
| **高密度多节点损坏 (10% 脏)**| 1000 | 1 VP | **443.15 µs** | 2,257 FPS | 2.659% | 6.381% | 10.636% | 15.941% |

### 2. 刷新率与帧时间预算分析 (Frame Budget Analysis)

在现代图形界面中，CPU 损伤计算应控制在整帧预算的 **10% 以下**，以留出充足的时间给 GPU 进行光栅化与合成：

```
+-------------------+------------------+---------------------+-----------------------+
| 刷新率 (Hz)       | 单帧总时间预算   | 200 节点桌面耗时占比 | 1000 节点大型场景耗时占比|
+-------------------+------------------+---------------------+-----------------------+
| 60 Hz             | 16.67 ms         | 1.71% (~0.28 ms)    | 2.21% (~0.37 ms)      |
| 120 Hz            | 8.33 ms          | 3.42% (~0.28 ms)    | 4.41% (~0.37 ms)      |
| 144 Hz (电竞高刷) | 6.94 ms          | 4.10% (~0.28 ms)    | 5.29% (~0.37 ms)      |
| 240 Hz (超高刷)   | 4.17 ms          | 6.84% (~0.28 ms)    | 8.82% (~0.37 ms)      |
| 360 Hz (极限高刷) | 2.78 ms          | 10.25% (~0.28 ms)   | 13.22% (~0.37 ms)     |
+-------------------+------------------+---------------------+-----------------------+
```

- **Attached Data 侵入式挂载与零 QHash**：消除了每帧几千次节点的哈希运算与离散堆分配，空闲帧耗时降低至 **400 纳秒 (0.41 µs)**。
- **超大规模场景突破**：在 5000 节点超复杂界面下，局部损伤与移动计算均稳定在 **0.74 ~ 0.83 ms**（进入亚毫秒级别！）。

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
