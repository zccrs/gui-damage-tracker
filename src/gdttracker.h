#ifndef GDTTRACKER_H
#define GDTTRACKER_H

#include "gdtnode.h"

#include <QRect>
#include <QRegion>
#include <QVector>
#include <memory>
#include <vector>

namespace Gdt {

// Mutations only mark dirty bits. commit() runs the region math. Viewports
// are per-commit world-to-output mappings, not persistent Tracker state.
struct Viewport {
    QRect outputRect; // output coordinates; empty means no clipping
    QTransform worldToOutput = QTransform(); // invertible 2D affine transform
    QRegion damage; // In: Viewport/Swapchain buffer damage; Out: populated in state

    struct State {
        QRegion damage; // Effective computed damage region for this viewport
        Viewport *viewport = nullptr;

        bool isCulled(const Node *node) const;
        bool isFullyOccluded(const Node *node) const;
        QRegion visibleDamage(const Node *node) const;
        QRegion occludedRegion(const Node *node) const;
    } state;

    // Viewport owns the attached node data instances (memory pool)
    std::vector<std::unique_ptr<NodeViewportData>> nodeDataPool;

    Viewport(const QRect &outputRect = QRect(),
             const QTransform &worldToOutput = QTransform(),
             const QRegion &damage = QRegion());
    ~Viewport();
    Viewport(const Viewport &other);
    Viewport &operator=(const Viewport &other);
    Viewport(Viewport &&other) noexcept;
    Viewport &operator=(Viewport &&other) noexcept;

    NodeViewportData *getOrCreateNodeData(Node *node);
    void clearNodeData();
};

// Per-viewport mutable occlusion state during merged multi-viewport traversal.
struct ViewportOcclusionState {
    QRegion frontOpaque;
    QRegion remaining;
    QRegion exposed;
    QRegion screen;
    Viewport *viewport = nullptr;
    bool skipped = false;
};

// Tracker runs region math. Three-phase API enables per-viewport async rendering.
class Tracker
{
public:
    using Viewport = Gdt::Viewport;
    using NodeView = NodeViewportData;
    Tracker() = default;
    explicit Tracker(Node *root);

    void setRoot(Node *root);
    Node *root() const { return m_root; }

    // Legacy: one-shot commit for all viewports (calls prepare/render/finish)
    void commit(QVector<Viewport> &viewports);

    // Phase 1: shared world-space computation (updateWorld + collectBackdrop)
    // Run once per frame, shared by all viewports on the same tree.
    void prepareFrame();

    // Phase 2: per-viewport occlusion culling (applyOcclusion)
    // Run independently per viewport, supports different refresh rates.
    // Swapchain buffer damage is merged here, not in collectBackdrop.
    void renderViewport(Viewport &vp);

    // Phase 3: shared state commit (commitState)
    // Run once after all viewports have finished rendering.
    void finishFrame();

    // Access the world damage computed in prepareFrame (for debugging/testing)
    QRegion worldDamage() const { return m_worldDamage; }

private:
    void computeViewport(Viewport &viewport, const QRegion &worldDamage);
    void computeAllViewports(QVector<Viewport> &viewports, const QRegion &worldDamage);

    Node *m_root = nullptr;
    QRegion m_worldDamage;
};

} // namespace Gdt

#endif // GDTTRACKER_H
