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
| `CustomNode` | 将后方累计 Damage 转换为明确的 Effect 输入/输出依赖。 |
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

`Tracker` 只负责场景分析和最终画面变化：

- 世界矩阵、bounds 和 opaque。
- 节点原始 `ownDamage`。
- Custom/Backdrop 的输入与输出依赖。
- 节点最终直接可见区域。
- 遮挡过滤后的世界送显候选。
- 将送显候选保守映射到各个 `Viewport`。

Tracker 不负责 RenderTarget、buffer age、RenderPass 或实际 flush。

### Viewport

`Viewport` 描述一个输出：

- `outputRect()`：输出坐标边界。
- `worldToOutput()`：世界坐标到输出坐标的变换。
- `accumulatedDamage()`：最终画面变化映射到该输出后的送显候选。

同一轮 `prepareFrame()` 后可对多个 Viewport 依次调用 `commit()`。
`commit()` 不再遍历场景，只做保守映射和输出裁剪。

### Renderer

Renderer 位于 gdt 之外，负责：

- 为主输出和每个 Effect 分配 RenderTarget。
- 合并每个目标自己的 buffer-age damage。
- 按 Effect 依赖顺序执行 RenderPass。
- 记录每个目标的实际 flush。
- 将最终主输出 `presentDamage` 交给显示 backend。

Example 的 `DemoRenderer` 建立三类 Pass：

```text
EffectInput  -> 重画 Backdrop 需要读取的后方内容
EffectOutput -> 重新生成 Effect 可见输出
Main         -> presentDamage ∪ mainBufferDamage
```

模拟 Renderer 使用精确 scissor，因此每个 Pass 的 `flushDamage` 等于该
Pass 的 `renderDamage`。真实 Renderer 可以保守扩大或全量重画。

---

## 坐标系与区域

| 数据 | 坐标系 | 含义 |
|---|---|---|
| `boundingRect()` | 内容局部 | 节点自身内容的边界。 |
| `opaqueRegion()` | 内容局部 | 能确定完全不透明的内容像素。 |
| `worldBounds()` | 世界 | 包围盒经过祖先变换后的保守整数边界。 |
| `worldOpaqueRegion()` | 世界 | 内容局部不透明区域映射到世界坐标的结果。 |
| `ownDamage()` | 世界 | 节点自身移动、形变、显隐或内容变化。 |
| `effectInputDamage()` | 世界 | Custom Effect 必须重新读取/生成的输入。 |
| `inducedDamage()` | 世界 | Custom Effect 重新生成的输出区域。 |
| `worldFrontOpaqueRegion()` | 世界 | 节点上方已经积累的不透明区域。 |
| `worldVisibleRegion()` | 世界 | 节点对最终画面的直接可见区域。 |
| `Tracker::rawWorldDamage()` | 世界 | 未做最终遮挡的 own/effect output 并集。 |
| `Tracker::presentWorldDamage()` | 世界 | 经过直接可见性过滤的最终画面变化。 |
| `Viewport::accumulatedDamage()` | 输出 | `presentWorldDamage` 的保守输出映射。 |
| renderer render damage | 目标坐标 | 当前 RenderTarget 逻辑上需要重画的区域。 |
| renderer flush damage | 目标坐标 | Renderer 实际写入该 RenderTarget 的区域。 |
| output present damage | 输出 | 最终主输出相对上一帧真正变化的区域。 |

一个被不透明前景完全覆盖的底层节点可以同时满足：

```text
worldVisibleRegion = empty
effectInputDamage   = non-empty
```

它对最终画面不可见，但仍是 Backdrop 的输入，必须在 Effect 输入 Pass 中
重画。Effect 输入/离屏 flush 不能并入最终 Output present damage。

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
5. 将当前累计 raw damage 传给 `CustomNode::DamageProcessor`。
6. Custom processor 返回 `EffectDamage { input, output }`。
7. `input` 保存为 `effectInputDamage()`。
8. `output` 保存为 `inducedDamage()` 并继续向前传播。
9. 汇总为 `Tracker::rawWorldDamage()`。

Backdrop 的默认 processor：

```text
input  = accumulatedDamage ∩ sampleBounds
output = dilate(input, expansion)
```

### 2. `Tracker::prepareFrame()`：逆序遍历

第二轮按前到后的顺序，即同级 `lastChild -> previousSibling`：

1. 累积当前节点上方的世界不透明区域。
2. 写入 `worldFrontOpaqueRegion()`。
3. 计算 `worldVisibleRegion = bounds - frontOpaque`。
4. 用节点旧/新直接可见区域裁剪 own/induced damage。
5. 将删除、隐藏和可见性变化加入送显候选。
6. 累加节点自身 `worldOpaqueRegion()`，继续处理后方节点。
7. 汇总为 `Tracker::presentWorldDamage()`。

Backdrop 不再修改前方 opaque，也不会让被遮挡的输入节点变成“直接
可见”。输入依赖只通过 `effectInputDamage()` 表达。

### 3. `Tracker::commit(viewport)`：每输出映射

`commit()` 不再遍历节点。它只执行：

```text
outputPresent =
    mapOuter(worldToOutput, presentWorldDamage)
    ∩ outputRect
```

不可逆或无法安全映射的矩阵会保守退化为整个 `outputRect`。结果写入
`Viewport::accumulatedDamage()`。

### 4. Renderer 执行 RenderPass

Renderer 对每个可见 Effect：

1. 将 `effectInputDamage` 映射到 Effect 输入目标。
2. 合并该输入目标自己的 buffer-age damage。
3. 重画后方输入节点。
4. 将 `inducedDamage ∩ effect.worldVisibleRegion` 映射到 Effect 输出目标。
5. 执行 Effect。

主输出：

```text
mainRenderDamage = viewportPresentDamage ∪ mainBufferDamage
```

Renderer 分别记录每个目标的 flush；只有最终主输出的 present damage
交给显示 backend。

Example 中：

- 渲染/Flush：红色到绿色。
- 最终送显：紫色到黄色。
- Buffer-only 修复会出现在渲染区域中，但不会出现在送显区域中。

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
QRegion presentDamage = primary.accumulatedDamage();
QRegion mainBufferDamage = swapchain.damageForCurrentBuffer();

RenderFramePlan plan = renderer.buildFramePlan(
    root, primary, presentDamage, mainBufferDamage);

renderer.execute(plan);

for (const RenderPassResult &pass : plan.passResults)
    pass.target->damageRing.add(pass.flushDamage);

swapchain.present(plan.presentDamage);
```

Buffer damage、RenderTarget、各 Pass flush 和最终 present damage 刻意不放
在 Tracker/Viewport 内：只有 renderer 知道实际 target 和执行策略。

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
2. 可选择只显示实际渲染/Flush、最终送显或同时显示：
   - 渲染/Flush：最新帧红色，历史帧绿色。
   - 最终送显：最新帧紫色，历史帧黄色。
3. 可保持最新区域不随历史时长消失。
4. 节点检查器显示：
   - world bounds / subtree bounds
   - own damage / effect input / effect output
   - world opaque / front opaque
   - 最终直接可见的 world visible region
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
