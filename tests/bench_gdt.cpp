#include "gdt.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QString>
#include <QTest>
#include <functional>
#include <vector>
using namespace Gdt;

class bench_Gdt : public QObject
{
    Q_OBJECT

private slots:
    void benchmarkSuite();
};

struct BenchmarkResult {
    QString category;
    QString scenario;
    int nodeCount;
    int viewportCount;
    double avgTimeUs;
    double maxFps;
    double budget60Hz;
    double budget144Hz;
    double budget240Hz;
};

static std::vector<BenchmarkResult> s_results;

static Node *buildRealisticTree(int totalNodes, std::vector<GeometryNode *> *outGeos = nullptr,
                                std::vector<TransformNode *> *outTransforms = nullptr,
                                std::vector<BackdropNode *> *outBackdrops = nullptr)
{
    auto *root = new Node(Node::Type::Basic);
    root->setName(QStringLiteral("root"));

    int created = 0;
    int containerId = 0;

    while (created < totalNodes) {
        auto *layer = new TransformNode();
        layer->setName(QStringLiteral("container_%1").arg(++containerId));
        layer->setTranslation((created % 10) * 80.0, (created / 10) * 60.0);
        root->appendChild(layer);
        if (outTransforms)
            outTransforms->push_back(layer);
        created++;

        const int childrenInLayer = std::min(15, totalNodes - created);
        for (int i = 0; i < childrenInLayer; ++i) {
            if (i == 3 && outBackdrops && created + 1 < totalNodes) {
                auto *bg = new BackdropNode();
                bg->setName(QStringLiteral("backdrop_%1").arg(created));
                bg->setBoundingRect(QRectF(10 * i, 10 * i, 200, 150));
                bg->setBackdropExpansion(16);
                layer->appendChild(bg);
                outBackdrops->push_back(bg);
                created++;
                continue;
            }

            auto *geo = new GeometryNode();
            geo->setName(QStringLiteral("item_%1").arg(created));
            geo->setBoundingRect(QRectF(15 * i, 10 * i, 120, 80));
            geo->setFullyOpaque(i % 2 == 0);
            layer->appendChild(geo);
            if (outGeos)
                outGeos->push_back(geo);
            created++;
            if (created >= totalNodes)
                break;
        }
    }

    return root;
}

static QVector<Tracker::Viewport> createViewports(int count)
{
    QVector<Tracker::Viewport> vps;
    vps.reserve(count);
    for (int i = 0; i < count; ++i) {
        Tracker::Viewport vp;
        vp.id = i;
        vp.outputRect = QRect(0, 0, 1920, 1080);
        QTransform t;
        t.translate(-i * 1920.0, 0);
        if (i == 1) {
            t.scale(1.25, 1.25);
        } else if (i == 2) {
            t.rotate(90, Qt::ZAxis);
        }
        vp.worldToOutput = t;
        vps.push_back(vp);
    }
    return vps;
}
static int getIterations(int nodes)
{
    if (nodes <= 50) return 600;
    if (nodes <= 200) return 300;
    if (nodes <= 1000) return 80;
    return 20;
}

static BenchmarkResult runBenchmark(const QString &category, const QString &scenario,
                                    int nodeCount, int viewportCount, int iterations,
                                    const std::function<void(Tracker &, Node *, const std::vector<GeometryNode *> &, const std::vector<TransformNode *> &, const std::vector<BackdropNode *> &, int)> &prepareIteration)
{
    std::vector<GeometryNode *> geos;
    std::vector<TransformNode *> transforms;
    std::vector<BackdropNode *> backdrops;

    std::unique_ptr<Node> root(buildRealisticTree(nodeCount, &geos, &transforms, &backdrops));
    Tracker tracker(root.get());

    const auto viewports = createViewports(viewportCount);
    tracker.commit(viewports); // warm-up & initial commit

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < iterations; ++i) {
        prepareIteration(tracker, root.get(), geos, transforms, backdrops, i);
        tracker.commit(viewports);
    }

    const qint64 elapsedNs = timer.nsecsElapsed();
    const double avgUs = double(elapsedNs) / (double(iterations) * 1000.0);
    const double maxFps = 1000000.0 / std::max(0.001, avgUs);

    BenchmarkResult res;
    res.category = category;
    res.scenario = scenario;
    res.nodeCount = nodeCount;
    res.viewportCount = viewportCount;
    res.avgTimeUs = avgUs;
    res.maxFps = maxFps;
    res.budget60Hz = (avgUs / 16666.67) * 100.0;
    res.budget144Hz = (avgUs / 6944.44) * 100.0;
    res.budget240Hz = (avgUs / 4166.67) * 100.0;
    return res;
}

void bench_Gdt::benchmarkSuite()
{
    qInfo() << "==========================================================================================";
    qInfo() << "                 GUI Damage Tracker Performance Benchmark Suite                           ";
    qInfo() << "==========================================================================================";

    // 1. Idle Frame Fast-Path
    for (int nodes : {50, 200, 1000, 5000}) {
        for (int vps : {1, 2, 4}) {
            s_results.push_back(runBenchmark(
                QStringLiteral("空闲帧 (无脏位快速路径)"),
                QStringLiteral("Idle commit (0 changes)"),
                nodes, vps, getIterations(nodes) * 2,
                [](Tracker &, Node *, const std::vector<GeometryNode *> &, const std::vector<TransformNode *> &, const std::vector<BackdropNode *> &, int) {
                    // no-op
                }
            ));
        }
    }

    // 2. Single Leaf Content Damage (e.g., text cursor blink or small widget repaint)
    for (int nodes : {50, 200, 1000, 5000}) {
        for (int vps : {1, 2, 4}) {
            s_results.push_back(runBenchmark(
                QStringLiteral("局部内容损伤 (16x16 px)"),
                QStringLiteral("Single leaf node markContentDirty"),
                nodes, vps, getIterations(nodes),
                [](Tracker &, Node *, const std::vector<GeometryNode *> &geos, const std::vector<TransformNode *> &, const std::vector<BackdropNode *> &, int iter) {
                    if (!geos.empty()) {
                        auto *target = geos[iter % geos.size()];
                        target->markContentDirty(QRect(10, 10, 16, 16));
                    }
                }
            ));
        }
    }

    // 3. Node Geometry Translation (Window drag / widget motion)
    for (int nodes : {50, 200, 1000, 5000}) {
        for (int vps : {1, 2, 4}) {
            s_results.push_back(runBenchmark(
                QStringLiteral("单节点几何移动 (平移)"),
                QStringLiteral("Single node setBoundingRect translate"),
                nodes, vps, getIterations(nodes),
                [](Tracker &, Node *, const std::vector<GeometryNode *> &geos, const std::vector<TransformNode *> &, const std::vector<BackdropNode *> &, int iter) {
                    if (!geos.empty()) {
                        auto *target = geos[iter % geos.size()];
                        target->setBoundingRect(QRectF(15.0 + (iter % 50), 10.0 + (iter % 30), 120.0, 80.0));
                    }
                }
            ));
        }
    }

    // 4. Subtree Transform Rotation
    for (int nodes : {50, 200, 1000}) {
        for (int vps : {1, 2}) {
            s_results.push_back(runBenchmark(
                QStringLiteral("子树层级旋转 (TransformNode)"),
                QStringLiteral("Subtree rotation matrix change"),
                nodes, vps, getIterations(nodes),
                [](Tracker &, Node *, const std::vector<GeometryNode *> &, const std::vector<TransformNode *> &transforms, const std::vector<BackdropNode *> &, int iter) {
                    if (!transforms.empty()) {
                        auto *target = transforms[iter % transforms.size()];
                        QTransform matrix;
                        matrix.translate(200, 200);
                        matrix.rotate(double(iter % 360), Qt::ZAxis);
                        target->setMatrix(matrix);
                    }
                }
            ));
        }
    }

    // 5. Backdrop Induced Damage Dilation
    for (int nodes : {50, 200, 1000}) {
        for (int vps : {1, 2}) {
            s_results.push_back(runBenchmark(
                QStringLiteral("背景采样扩散 (Backdrop 16px)"),
                QStringLiteral("Backdrop induced damage dilation"),
                nodes, vps, getIterations(nodes),
                [](Tracker &, Node *, const std::vector<GeometryNode *> &geos, const std::vector<TransformNode *> &, const std::vector<BackdropNode *> &, int iter) {
                    if (!geos.empty()) {
                        geos[0]->markContentDirty(QRect(10, 10, 40, 40));
                    }
                }
            ));
        }
    }

    // 6. High-Density Scattered Damage (10% of all nodes dirty simultaneously)
    for (int nodes : {50, 200, 1000}) {
        for (int vps : {1, 2}) {
            s_results.push_back(runBenchmark(
                QStringLiteral("高密度多节点损坏 (10% 节点)"),
                QStringLiteral("10% nodes concurrently dirty"),
                nodes, vps, getIterations(nodes),
                [](Tracker &, Node *, const std::vector<GeometryNode *> &geos, const std::vector<TransformNode *> &, const std::vector<BackdropNode *> &, int iter) {
                    const size_t dirtyCount = std::max<size_t>(1, geos.size() / 10);
                    for (size_t k = 0; k < dirtyCount; ++k) {
                        const size_t idx = (iter + k * 7) % geos.size();
                        geos[idx]->markContentDirty(QRect(5, 5, 20, 20));
                    }
                }
            ));
        }
    }

    // Print Formatted Markdown Table
    printf("\n\n### 性能基准测试结果汇总 (Performance Benchmark Summary)\n\n");
    printf("| 场景分类 (Scenario) | 节点数 (Nodes) | Viewport数 | 平均单帧耗时 (Avg Time) | 最大理论吞吐量 (Throughput) | 60Hz 帧预算占比 | 144Hz 帧预算占比 | 240Hz 帧预算占比 |\n");
    printf("|---|---|---|---|---|---|---|---|\n");
    for (const auto &r : s_results) {
        printf("| %s | %d | %d VP | **%.2f µs** | %.0f FPS | %.3f%% | %.3f%% | %.3f%% |\n",
               r.category.toUtf8().constData(),
               r.nodeCount,
               r.viewportCount,
               r.avgTimeUs,
               r.maxFps,
               r.budget60Hz,
               r.budget144Hz,
               r.budget240Hz);
    }
    printf("\n");
}

QTEST_MAIN(bench_Gdt)
#include "bench_gdt.moc"
