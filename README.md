# GUI Damage Tracker (gdt)

[![Build & Tests](https://img.shields.io/badge/tests-passing-brightgreen.svg)]()
[![Qt](https://img.shields.io/badge/Qt-6.11%2B-blue.svg)]()
[![C++](https://img.shields.io/badge/C%2B%2B-17-purple.svg)]()
[![License](https://img.shields.io/badge/license-LGPL%2FMIT-green.svg)]()

`gui-damage-tracker` (gdt) 是一个面向现代化 GUI 框架与 Wayland 合成器（如 [Treeland](https://github.com/linuxdeepin/treeland) / QtQuick）的**场景图损伤预计算与精确跟踪库**。

它提供类似于 `QSGNode` 的树形场景图管理，支持 2D 矩阵变换、不透明区域判定、遮挡剔除，并能够针对多个带独立矩阵映射的视口（Viewport / Display Output）并行计算渲染所需的最小损伤区域与节点剔除状态。背景采样扩张由 Renderer 在绘制时处理，Tracker 不感知 Backdrop。

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

### Pixman Region API

gdt 核心区域全部使用 `pixman_region32_t`。公开 getter 直接返回只读 native
handle，不再返回 `QRegion`：

```cpp
const pixman_region32_t *damage = viewport.outputDamageRegion();
const pixman_region32_t *visible = node->worldVisibleRegion();
```

需要拥有/修改区域时使用 RAII `Gdt::Region`：

```cpp
Gdt::Region region(viewport.outputDamageRegion());
region += QRect(10, 20, 30, 40);
pixman_region32_t *native = region.native();
```

`Region` 提供 copy/move、union/intersect/subtract、translate、extents 和
native handle。Qt 使用者若需要 `QRegion`，应在应用或 UI 边界自行转换。
核心库不再依赖 `QRegion`，也不使用 `pixman_region64f_t`。

### Tracker

`Tracker` 只负责场景分析和最终画面变化：

- 世界矩阵、bounds 和 opaque。
- 节点原始 `ownDamage`。
- 节点最终直接可见区域。
- 遮挡过滤后的世界损伤。
- 将世界损伤保守映射到各个 `Viewport`。

Tracker 不负责 RenderTarget、buffer age、RenderPass 或实际 flush。

### Viewport

`Viewport` 描述一个输出：

- `outputRect()`：输出坐标边界。
- `worldToOutput()`：世界坐标到输出坐标的变换。
- `outputDamageRegion()`：世界损伤映射到该输出后的送显候选。
- `flushRegion()`：Renderer 绘制时写入；Tracker 不填充。

同一轮 `prepareFrame()` 后一次 `commit(viewports)` 映射全部 Viewport。

### Renderer

Renderer 位于 gdt 之外，负责：

- 为主输出和每个 Effect 分配 RenderTarget。
- 合并每个目标自己的 buffer-age damage。
- 按 Effect 依赖顺序执行 RenderPass。
- 记录每个目标的实际 flush。
- 将最终主输出 `presentDamage` 交给显示 backend。

Example 的 `DemoRenderer` 正序累加 flush（不做 backdrop 加回），
`addFlushRegion()` 写入 Viewport。swapchain buffer 只并进重绘区域。

---

## 坐标系与区域

| 数据 | 坐标系 | 含义 |
|---|---|---|
| `boundingRect()` | 内容局部 | 节点自身内容的边界。 |
| `opaqueRegion()` | 内容局部 | 能确定完全不透明的内容像素。 |
| `worldBounds()` | 世界 | 包围盒经过祖先变换后的保守整数边界。 |
| `worldFrontOpaqueRegion()` | 世界 | 节点上方已经积累的不透明区域。 |
| `worldValidRegion()` | 世界 | bounds − 前方不透明（backdrop 打孔，供 cache，不是上屏可见）。 |
| `worldVisibleRegion()` | 世界 | bounds − 前方 backdrop 覆盖。未脏节点只重绘这里。 |
| `committedWorldValidRegion()` | 世界 | 上一帧提交的有效区域。 |
| `Tracker::damageRegion()` | 世界 | 遮挡过滤后的画面变化。 |
| `Viewport::worldDamageRegion()` | 世界 | 该输出相交后的世界损伤。 |
| `Viewport::outputDamageRegion()` | 输出 | 世界损伤的保守输出映射。 |
| `Viewport::flushRegion()` | 输出 | Renderer 绘制时计算的送显区域，不含 bufferDamage。 |
| renderer flush damage | 目标坐标 | Renderer 实际写入该 RenderTarget 的区域。 |

节点不会输出超过 `worldBounds` 的内容。需要更大输出时扩大 `boundingRect`。

被不透明前景完全覆盖的底层节点 `worldValidRegion` 为空（无 backdrop 打孔时）。
有 backdrop 时打孔，采样后方仍 valid。未脏节点只重绘 `worldVisibleRegion()`。

---

## 每帧算法

### 0. 修改场景

节点 setter 只修改数据并置脏：

- 几何变化：`DirtyGeometry`
- 内容变化：`DirtyContent`
- 不透明变化：`DirtyOpaque`
- 矩阵变化：`DirtyMatrix`
- 子节点变化向祖先传播 `DirtySubtree`

此时不会自动计算 damage。

### 1. `Tracker::prepareFrame()`：正序更新世界状态

按绘制顺序从后向前（`firstChild -> nextSibling`）：

1. 累乘 `worldTransform`。
2. 计算 `worldBounds`、`subtreeBounds` 和 `worldOpaqueRegion`。
3. 根据已提交状态生成旧位置和新位置的 `ownDamage`。
4. 将内容局部 dirty 映射成世界 `ownDamage`。
5. 从不透明前景减去遮挡，再并入节点 `ownDamage`。
6. `needsBackdrop` 节点：`backdropDamage += geometry ∩ behindDamage`。
7. 最终 `damageRegion = 遮挡过滤损伤 ∪ backdropDamage`。

每个节点在正序时写入 `behindDamageRegion()`，即绘制到该节点之前的累计损伤。

### 2. `Tracker::prepareFrame()`：逆序计算可见性

按前到后（`lastChild -> previousSibling`）：

1. 累积前方不透明 → `worldValidRegion = bounds − frontOpaque`（backdrop 打孔）。
2. 累积前方 backdrop 覆盖 → `worldVisibleRegion = bounds − frontBackdrop`。
3. `needsBackdrop` 覆盖区内，前方不透明不让后方变 invalid（cache 仍要画）。

### 3. `Tracker::commit(viewports)`：每输出映射

```text
outputDamage =
    mapOuter(worldToOutput, worldDamage)
    ∩ outputRect
```

不可逆矩阵退化为整个 `outputRect`。`flushRegion` 保持为空。

### 4. Renderer

- 正序累加 `currentDamageRegion`（与 Tracker 第一轮相同，但不把
  backdropDamage 加回，被前景不透明盖住的区域不进 flush）。
- 画到 `needsBackdrop` 节点时，copy source = `worldBounds ∩ currentDamageRegion`
  （该 Viewport 本轮绘制累计）。未脏节点只重绘 `worldVisibleRegion()`。
- `flushRegion` = 上述累加结果（可含采样扩张 extra）。
- swapchain bufferDamage 只扩大重绘，不扩大 flush。

### 5. 完成本帧

```cpp
tracker.finishFrame();
viewport.finishFrame();
```

`Tracker::finishFrame()` 提交节点世界状态并回到 `Idle`。
`Viewport::finishFrame()` 保存输出矩形/矩阵并清空损伤。

Tracker 相位为：

```text
Idle --prepareFrame--> Prepared --commit(viewports)--> Committed
     <--finishFrame------------------------------------------|
```

不能在 `Prepared/Committed` 状态再次调用 `prepareFrame()`。

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
const pixman_region32_t *presentDamage = primary.accumulatedDamage();
const pixman_region32_t *mainBufferDamage = swapchain.damageForCurrentBuffer();

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

基准测试在 Linux x86_64（Intel Core i7-10700）、Qt 6.11.2、Pixman 0.46.4
环境执行：

```bash
./build-gdt/tests/bench_gdt
```

### Pixman Region 当前结果

| 场景 | 节点数 | Viewport | 平均单帧 | 60Hz 占比 | 144Hz 占比 |
|---|---:|---:|---:|---:|---:|
| 空闲帧 | 5000 | 4 | **0.34 µs** | 0.002% | 0.005% |
| 局部内容损伤 16×16 | 50 | 1 | **38.05 µs** | 0.228% | 0.548% |
| 局部内容损伤 16×16 | 200 | 1 | **183.13 µs** | 1.099% | 2.637% |
| 局部内容损伤 16×16 | 1000 | 1 | **955.59 µs** | 5.734% | 13.760% |
| 局部内容损伤 16×16 | 5000 | 1 | **5699.99 µs** | 34.200% | 82.080% |
| 单节点几何移动 | 200 | 1 | **193.37 µs** | 1.160% | 2.785% |
| 单节点几何移动 | 1000 | 1 | **969.29 µs** | 5.816% | 13.958% |
| 单节点几何移动 | 5000 | 1 | **5919.16 µs** | 35.515% | 85.236% |
| 子树旋转 | 200 | 1 | **149.24 µs** | 0.895% | 2.149% |
| 子树旋转 | 1000 | 1 | **714.52 µs** | 4.287% | 10.289% |
| Backdrop 16px | 200 | 1 | **198.06 µs** | 1.188% | 2.852% |
| Backdrop 16px | 1000 | 1 | **972.92 µs** | 5.837% | 14.010% |
| 10% 节点同时损坏 | 200 | 1 | **219.05 µs** | 1.314% | 3.154% |
| 10% 节点同时损坏 | 1000 | 1 | **1314.08 µs** | 7.884% | 18.923% |

### 同算法 QRegion / Pixman 对比

基线使用迁移前提交 `6e5cf72`，在独立 worktree、相同编译器和同一台机器
运行相同 benchmark：

| 场景 | 节点数 | QRegion | Pixman | 加速 |
|---|---:|---:|---:|---:|
| 局部内容损伤 | 200 | 300.68 µs | 183.13 µs | **1.64×** |
| 局部内容损伤 | 1000 | 3025.95 µs | 955.59 µs | **3.17×** |
| 局部内容损伤 | 5000 | 61263.64 µs | 5699.99 µs | **10.75×** |
| 单节点移动 | 200 | 369.65 µs | 193.37 µs | **1.91×** |
| 单节点移动 | 1000 | 3034.87 µs | 969.29 µs | **3.13×** |
| 单节点移动 | 5000 | 61447.91 µs | 5919.16 µs | **10.38×** |
| Backdrop 16px | 200 | 294.33 µs | 198.06 µs | **1.49×** |
| Backdrop 16px | 1000 | 3138.01 µs | 972.92 µs | **3.23×** |
| 10% 节点损坏 | 1000 | 3210.72 µs | 1314.08 µs | **2.44×** |

完整 benchmark suite wall time 从 **14.53s** 降到 **4.75s**，降低约
**67.3%**，整体约 **3.06×**。

结果与硬件、编译器、Qt/Pixman 版本和场景碎片度相关。5000 节点场景已明显
超过高刷新率预算，后续优化重点应是局部 update region，而不是继续替换
Region 容器。

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
