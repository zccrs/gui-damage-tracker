# GUI Damage Tracker (gdt)

[![Build & Tests](https://img.shields.io/badge/tests-passing-brightgreen.svg)]()
[![Qt](https://img.shields.io/badge/Qt-6.11%2B-blue.svg)]()
[![C++](https://img.shields.io/badge/C%2B%2B-17-purple.svg)]()
[![License](https://img.shields.io/badge/license-LGPL%2FMIT-green.svg)]()

`gui-damage-tracker` (gdt) 是一个面向现代化 GUI 框架与 Wayland 合成器（如 [Treeland](https://github.com/linuxdeepin/treeland) / QtQuick）的**场景图损伤预计算与精确跟踪库**。

它提供类似于 `QSGNode` 的树形场景图管理，支持 2D 矩阵变换、不透明区域判定、遮挡剔除以及特殊的**背景采样扩散损伤（Backdrop Node）**，并能够针对多个带独立矩阵映射的视口（Viewport / Display Output）并行计算渲染所需的最小损伤区域与节点剔除状态。

---

## 角色与职责

### 场景节点

场景图遵循两个固定顺序：

- 同级节点中，先插入的节点在后面，后插入的节点在前面。
- 节点自身内容位于其子节点后面；子节点会覆盖父节点自身内容。

| 角色 | 职责 |
|---|---|
| `Node` | 树结构、可见性、脏位传播和每帧世界坐标结果。默认没有自身内容。 |
| `GeometryNode` | 内容包围盒、内容局部脏区和内容局部不透明区域。 |
| `TransformNode` | 为整棵子树提供平移、缩放、旋转或组合 `QTransform`。 |
| `CustomNode` | 通过 damage/opaque processor 表达非普通几何的渲染依赖。 |
| `BackdropNode` | `CustomNode` 的预设实现；后方内容变化时产生扩张损伤。 |

`GeometryNode::boundingRect()` 位于节点局部坐标系；它不会自动继承父
`GeometryNode` 的左上角。父子空间关系应由 `TransformNode` 表达。

`setOpaqueRegion()` 和 `markContentDirty()` 使用内容局部坐标：

```text
(0, 0) = boundingRect 的左上角
```

例如包围盒为 `(100, 110, 240, 150)` 时：

```cpp
node->markContentDirty(QRect(12, 24, 26, 20));
```

对应的世界损伤从 `(112, 134)` 开始；移动包围盒后该损伤会跟随节点。

### Tracker

`Tracker` 只负责场景状态和场景 damage：

- 共享世界坐标预计算。
- Backdrop 等自定义节点的依赖损伤。
- 世界不透明区域和 renderer 可用的可见区域。
- 将世界 damage 映射并裁剪到各个 `Viewport`。

`Tracker` 不负责 swapchain buffer age，也不负责最终 `flushRegion`。
这些数据属于实际 renderer。

### Viewport

`Viewport` 描述一个输出：

- `outputRect()`：输出坐标边界。
- `worldToOutput()`：世界坐标到输出坐标的变换。
- `accumulatedDamage()`：本次提交后该输出需要处理的场景 damage。

同一轮 `prepareFrame()` 后可对多个 Viewport 依次调用 `commit()`。每个
Viewport 独立执行变换、裁剪和遮挡计算。

### Renderer

Renderer 位于 gdt 之外。它消费 Tracker 的输出，并结合自身状态：

- `Viewport::accumulatedDamage()`
- 当前 swapchain buffer 的 buffer-age damage
- 节点的 `worldVisibleRegion()`
- 输出裁剪和实际绘制策略

Example 中的 `DemoRenderer` 只是一个简单模拟：

```text
incoming = viewportDamage ∪ bufferDamage
flush    = incoming
           ∩ mapToOutput(所有节点 worldVisibleRegion 的并集)
           ∩ outputRect
```

真实 renderer 可以进一步按节点、RenderPass、layer 或 GPU scissor 拆分。

---

## 坐标系与区域

| 数据 | 坐标系 | 含义 |
|---|---|---|
| `boundingRect()` | 内容局部 | 节点自身内容的边界。 |
| `opaqueRegion()` | 内容局部 | 能确定完全不透明的内容像素。 |
| `worldBounds()` | 世界 | 包围盒经过祖先变换后的保守整数边界。 |
| `worldOpaqueRegion()` | 世界 | 内容局部不透明区域映射到世界坐标的结果。 |
| `ownDamage()` | 世界 | 节点自身移动、形变、显隐或内容变化产生的损伤。 |
| `inducedDamage()` | 世界 | Backdrop/Custom processor 产生的依赖损伤。 |
| `worldFrontOpaqueRegion()` | 世界 | 处理节点前，已在它上方积累的不透明区域。 |
| `worldEffectiveFrontOpaqueRegion()` | 世界 | Backdrop processor 打洞后的前方不透明区域。 |
| `worldVisibleRegion()` | 世界 | renderer 不能剔除的节点区域。 |
| `Tracker::worldDamage()` | 世界 | 当前帧所有 own/induced damage 的并集。 |
| `Viewport::accumulatedDamage()` | 输出 | 世界 damage 经该输出变换、裁剪和遮挡后的结果。 |
| renderer `flushRegion` | 输出 | renderer 合并 buffer damage 后真正刷新的区域。 |

这里的 `worldVisibleRegion()` 使用 renderer 语义：Backdrop 采样依赖可以
使一个被普通不透明节点挡住的后方节点仍有非空区域，因为 renderer 仍需
生成 Backdrop 的输入内容。若集成方还需要“最终肉眼直接可见”的区域，应
在 renderer 中单独维护，不能与采样依赖混用。

---

## 每帧算法

### 0. 修改场景

节点 setter 只修改数据并置脏：

- 几何变化：`DirtyGeometry`
- 内容变化：`DirtyContent`
- 不透明变化：`DirtyOpaque`
- 矩阵变化：`DirtyMatrix`
- 子节点变化向祖先传播 `DirtySubtree`

此时不会自动计算 damage。Example 的“自动提交”只是 UI 策略。

### 1. `Tracker::prepareFrame()`：正序遍历

第一轮按绘制顺序从后向前，即同级 `firstChild -> nextSibling`：

1. 累乘 `worldTransform`。
2. 计算 `worldBounds`、`subtreeBounds` 和 `worldOpaqueRegion`。
3. 根据已提交状态生成旧位置和新位置的 `ownDamage`。
4. 将内容局部 dirty 映射成世界 `ownDamage`。
5. 将当前累计 damage 传给 `CustomNode::DamageProcessor`。
6. Backdrop 根据后方相关 damage 生成 `inducedDamage`。
7. 汇总为 `Tracker::worldDamage()`。

世界状态更新和 damage 汇聚在同一次正序遍历中完成。

### 2. `Tracker::prepareFrame()`：逆序遍历

第二轮按前到后的顺序，即同级 `lastChild -> previousSibling`：

1. 累积当前节点上方的世界不透明区域。
2. 写入 `worldFrontOpaqueRegion()`。
3. Custom/Backdrop opaque processor 只在此处以世界坐标运行一次。
4. 写入 processor 处理后的 `worldEffectiveFrontOpaqueRegion()`。
5. 由有效前方不透明区域计算 `worldVisibleRegion()`。
6. 累加节点自身 `worldOpaqueRegion()`，继续处理后方节点。

Backdrop 打洞依赖第一轮产生的 `worldDamage`。因此底层节点即使被顶层
不透明节点覆盖，只要仍是 Backdrop 的采样输入，就不会被 renderer 错误
剔除。

### 3. `Tracker::commit(viewport)`：每输出计算

对每个 Viewport 独立执行：

1. 用 `worldToOutput` 映射世界 damage。
2. 裁剪到 `outputRect`。
3. 按前到后遍历节点，映射不透明和有效前方不透明区域。
4. 消除被不透明内容覆盖的 damage。
5. 处理前景移开等 exposure damage。
6. 写入 `Viewport::accumulatedDamage()`。

`applyOcclusion()` 不再调用 opaque processor。它只映射第二轮已经得到的
世界坐标结果，避免把输出坐标的 leaked region 与世界坐标 bounds 混用。

### 4. Renderer 计算 flush

Renderer 取得 `Viewport::accumulatedDamage()`，再合并当前目标 buffer 的
buffer-age damage。Example 的 `DemoRenderer` 根据节点可见区域生成
`flushRegion`，并提供 Damage/Flush/两者三种叠加显示。

Damage 使用红色到绿色的时间序列；Flush 使用紫色到黄色的时间序列。

### 5. 完成本帧

```cpp
tracker.finishFrame();
viewport.finishFrame();
```

`Tracker::finishFrame()`：

- 提交节点世界状态。
- 清除节点 dirty bits。
- 回到 `Idle`。

`Viewport::finishFrame()`：

- 保存本次输出矩形和矩阵，供下帧检测 Viewport 变化。
- 清空当前 `accumulatedDamage()`。

Tracker 相位为：

```text
Idle --prepareFrame--> Prepared --commit(viewport)--> Committed
     <--finishFrame-----------------------------------------|
```

同一帧可重复 `commit()` 不同 Viewport；不能在 `Prepared/Committed` 状态
再次调用 `prepareFrame()`。

---

## 快速上手

### CMake 集成

```cmake
add_subdirectory(gui-damage-tracker)
target_link_libraries(your_project PRIVATE gdt)
```

### 基本帧循环

```cpp
#include <gdt.h>

using namespace Gdt;

Node root;

auto *transform = new TransformNode;
transform->setTranslation(100, 50);
root.appendChild(transform);

auto *card = new GeometryNode;
card->setBoundingRect(QRectF(0, 0, 400, 300));
card->setFullyOpaque(true);
transform->appendChild(card);

Tracker tracker(&root);
Tracker::Viewport primary(QRect(0, 0, 1920, 1080));
Tracker::Viewport scaled(QRect(1920, 0, 2560, 1440),
                         QTransform::fromScale(1.5, 1.5));

// 修改只置脏，不立即计算。
card->markContentDirty(QRect(12, 24, 26, 20));

tracker.prepareFrame();
tracker.commit(primary);
tracker.commit(scaled);

renderOutput(primary.accumulatedDamage());
renderOutput(scaled.accumulatedDamage());

tracker.finishFrame();
primary.finishFrame();
scaled.finishFrame();
```

### Renderer 合并 Buffer Damage

```cpp
QRegion viewportDamage = primary.accumulatedDamage();
QRegion bufferDamage = swapchain.damageForCurrentBuffer();

QRegion incoming = viewportDamage + bufferDamage;
QRegion flushRegion = renderer.computeFlushRegion(root, primary, incoming);

renderer.render(flushRegion);
swapchain.present(flushRegion);
```

`bufferDamage` 和 `flushRegion` 刻意不放在 Tracker/Viewport 内：只有
renderer 知道当前 render target、buffer age、RenderPass 和最终提交策略。

---

## 性能基准测试 (Performance Benchmark)

基准测试在 Linux x86_64 环境（Intel Core i7-10700 CPU @ 2.90GHz）下通过 `tests/bench_gdt` 执行，涵盖不同树规模（50 ~ 5000 节点）、多视口配置（1 ~ 4 屏幕）及各类典型 GUI 破损场景。

### 1. 仓库记录的历史基准结果

| 场景分类 (Scenario) | 节点数 (Nodes) | Viewport 数 | 平均单帧耗时 (Avg Time) | 最大理论吞吐量 | 60Hz 帧预算占比 | 144Hz 帧预算占比 | 240Hz 帧预算占比 | 360Hz 帧预算占比 |
|---|---|---|---|---|---|---|---|---|
| **空闲帧 (无脏位快速路径)** | 50 | 1 VP | **0.16 µs** | 6,386,852 FPS | 0.001% | 0.002% | 0.004% | 0.006% |
| **空闲帧 (无脏位快速路径)** | 200 | 1 VP | **0.16 µs** | 6,296,238 FPS | 0.001% | 0.002% | 0.004% | 0.006% |
| **空闲帧 (无脏位快速路径)** | 1000 | 4 VP | **0.23 µs** | 4,406,136 FPS | 0.001% | 0.003% | 0.005% | 0.008% |
| **空闲帧 (无脏位快速路径)** | 5000 | 4 VP | **0.28 µs** | 3,554,924 FPS | 0.002% | 0.004% | 0.007% | 0.010% |
| **局部内容损伤 (16x16 px)** | 50 (常规控件) | 1 VP | **66.54 µs** | 15,029 FPS | 0.399% | 0.958% | 1.597% | 2.394% |
| **局部内容损伤 (16x16 px)** | 200 (复杂桌面) | 1 VP | **286.26 µs** | 3,493 FPS | 1.718% | 4.122% | 6.870% | 10.293% |
| **局部内容损伤 (16x16 px)** | 1000 (合成器全树) | 1 VP | **360.14 µs** | 2,777 FPS | 2.161% | 5.186% | 8.643% | 12.953% |
| **局部内容损伤 (16x16 px)** | 5000 (超大树四屏) | 4 VP | **748.34 µs** | 1,336 FPS | 4.490% | 10.776% | 17.960% | 26.916% |
| **单节点几何移动 (平移)** | 200 | 1 VP | **369.90 µs** | 2,703 FPS | 2.219% | 5.327% | 8.878% | 13.306% |
| **单节点几何移动 (平移)** | 1000 | 1 VP | **461.91 µs** | 2,165 FPS | 2.771% | 6.652% | 11.086% | 16.616% |
| **单节点几何移动 (平移)** | 5000 | 1 VP | **776.98 µs** | 1,287 FPS | 4.662% | 11.188% | 18.647% | 27.948% |
| **子树层级旋转 (Transform)** | 200 | 1 VP | **241.27 µs** | 4,145 FPS | 1.448% | 3.474% | 5.791% | 8.680% |
| **子树层级旋转 (Transform)** | 1000 | 1 VP | **775.00 µs** | 1,290 FPS | 4.650% | 11.160% | 18.600% | 27.878% |
| **背景采样扩散 (Backdrop)** | 1000 | 1 VP | **388.02 µs** | 2,577 FPS | 2.328% | 5.587% | 9.312% | 13.957% |
| **高密度多节点损坏 (10% 脏)**| 1000 | 1 VP | **451.56 µs** | 2,215 FPS | 2.709% | 6.502% | 10.837% | 16.236% |

### 2. 刷新率与帧时间预算分析 (Frame Budget Analysis)

在现代图形界面中，CPU 损伤计算应控制在整帧预算的 **10% 以下**，以留出充足的时间给 GPU 进行光栅化与合成：

```
+-------------------+------------------+---------------------+-----------------------+
| 刷新率 (Hz)       | 单帧总时间预算   | 200 节点桌面耗时占比 | 1000 节点大型场景耗时占比|
+-------------------+------------------+---------------------+-----------------------+
| 60 Hz             | 16.67 ms         | 1.72% (~0.29 ms)    | 2.16% (~0.36 ms)      |
| 120 Hz            | 8.33 ms          | 3.44% (~0.29 ms)    | 4.32% (~0.36 ms)      |
| 144 Hz (电竞高刷) | 6.94 ms          | 4.12% (~0.29 ms)    | 5.19% (~0.36 ms)      |
| 240 Hz (超高刷)   | 4.17 ms          | 6.87% (~0.29 ms)    | 8.64% (~0.36 ms)      |
| 360 Hz (极限高刷) | 2.78 ms          | 10.29% (~0.29 ms)   | 12.95% (~0.36 ms)     |
+-------------------+------------------+---------------------+-----------------------+
```

- **跨帧状态**：节点保存 committed bounds/visibility，Viewport 保存上次输出
  几何；空闲帧通过根节点脏位直接短路。
- **结果解释**：以下数据与硬件、Qt 版本和具体提交相关。修改核心算法后应
  重新运行 `bench_gdt`，不要把表中数值视为固定承诺。

## 交互式调试演示工具 (Damage Demo Inspector)

项目内置 QML 调试器 `damage-demo`：

```bash
./build/examples/damage-demo/damage-demo
```

主要能力：

1. 内置遮挡移动、局部内容变化、Backdrop 扩散、采样被遮挡、揭露、
   旋转和缩放场景。
2. 可选择只显示 Damage、只显示 Flush 或同时显示：
   - Damage：最新帧红色，历史帧绿色。
   - Flush：最新帧紫色，历史帧黄色。
3. 可保持最新区域不随历史时长消失。
4. 节点检查器显示：
   - world bounds / subtree bounds
   - own damage / induced damage
   - world opaque / front opaque / effective front opaque
   - renderer 语义的 world visible region
5. 左侧树负责选择、排序和重挂节点；画布拖动不会隐式改变选择。
6. 可关闭自动提交，先修改场景，再手动观察一次完整提交。

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
