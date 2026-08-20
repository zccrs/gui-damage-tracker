#ifndef GDTTRACKER_H
#define GDTTRACKER_H

#include "gdtnode.h"

#include <QHash>
#include <QRect>
#include <QRegion>
#include <QVector>

namespace Gdt {

// Mutations only mark dirty bits. commit() runs the region math. Viewports
// are per-commit world-to-output mappings, not persistent Tracker state.
class Tracker
{
public:
    using ViewportId = quint64;
    using NodeView = NodeViewportView;
    static constexpr ViewportId PrimaryViewport = 0;

    struct Viewport {
        ViewportId id = PrimaryViewport;
        QRect outputRect; // output coordinates; empty means no clipping
        QTransform worldToOutput = QTransform(); // invertible 2D affine transform
    };

    Tracker() = default;
    explicit Tracker(Node *root);

    void setRoot(Node *root);
    Node *root() const { return m_root; }

    QRegion commit();
    QRegion commit(const QRect &outputRect);
    QHash<ViewportId, QRegion> commit(const QVector<Viewport> &viewports);

    // All per-viewport regions use that viewport's output coordinate system.

    QRegion lastDamage() const;
    QRegion damage(ViewportId id) const;
    QRegion visibleDamage(ViewportId id, const Node *node) const;
    QRegion occludedRegion(ViewportId id, const Node *node) const;
    bool isFullyOccluded(ViewportId id, const Node *node) const;
    bool isCulled(ViewportId id, const Node *node) const;

private:
    struct ViewportState {
        ViewportId id = PrimaryViewport;
        QRect outputRect;
        QTransform worldToOutput;
        QRegion lastDamage;
        QHash<quint64, NodeView> nodes;
    };

    const ViewportState *findResult(ViewportId id) const;
    QRegion computeViewport(ViewportState *vp, const QRegion &worldDamage,
                            const QRegion &extra);
    void mirrorNodeViews(const ViewportState &vp);

    Node *m_root = nullptr;
    QVector<ViewportState> m_results;
    QHash<ViewportId, Viewport> m_lastViewports;
    bool m_hasCommitted = false;
};

} // namespace Gdt

#endif // GDTTRACKER_H
