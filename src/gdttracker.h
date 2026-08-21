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
        QRegion damage; // In: Viewport/Swapchain buffer damage; Out: populated in state

        struct State {
            QRegion damage; // Effective computed damage region for this viewport
            QHash<quint64, NodeView> nodes;

            bool isCulled(const Node *node) const {
                if (!node) return true;
                const auto it = nodes.constFind(node->id());
                return (it != nodes.cend()) ? it->culled : node->hasContent();
            }
            bool isFullyOccluded(const Node *node) const {
                if (!node) return false;
                const auto it = nodes.constFind(node->id());
                return (it != nodes.cend()) ? it->fullyOccluded : false;
            }
            QRegion visibleDamage(const Node *node) const {
                if (!node) return {};
                const auto it = nodes.constFind(node->id());
                return (it != nodes.cend()) ? it->visibleDamage : QRegion();
            }
            QRegion occludedRegion(const Node *node) const {
                if (!node) return {};
                const auto it = nodes.constFind(node->id());
                return (it != nodes.cend()) ? it->occludedRegion : QRegion();
            }
        } state;
    };

    Tracker() = default;
    explicit Tracker(Node *root);

    void setRoot(Node *root);
    Node *root() const { return m_root; }

    // Primary commit: stateless, populates each Viewport::state in-place
    void commit(QVector<Viewport> &viewports);

    // Convenience overloads
    QRegion commit();
    QRegion commit(const QRect &outputRect);

    // Mirror node views from a given viewport to node accessors
    void mirrorNodeViews(const Viewport &vp);

private:
    void computeViewport(Viewport &vp, const QRegion &worldDamage, const QRegion &extra);

    Node *m_root = nullptr;
    QHash<ViewportId, Viewport> m_lastViewports;
    bool m_hasCommitted = false;
};

} // namespace Gdt

#endif // GDTTRACKER_H
