#ifndef GDTTRACKER_H
#define GDTTRACKER_H

#include "gdtnode.h"

#include <QRect>
#include <QTransform>
#include <QVector>
#include <QSet>

namespace Gdt {

// Per-output mapping of scene damage. flushRegion is filled by the Renderer.
class Viewport
{
public:
    Viewport() = default;
    Viewport(const QRect &outputRect, const QTransform &worldToOutput = QTransform())
        : m_outputRect(outputRect), m_worldToOutput(worldToOutput) {}

    void setOutputRect(const QRect &rect);
    void setWorldToOutput(const QTransform &transform);

    QRect outputRect() const { return m_outputRect; }
    QTransform worldToOutput() const { return m_worldToOutput; }
    bool isDirty() const { return m_dirty; }

    const pixman_region32_t *worldDamageRegion() const { return m_worldDamage.native(); }
    const pixman_region32_t *outputDamageRegion() const { return m_outputDamage.native(); }
    const pixman_region32_t *flushRegion() const { return m_flush.native(); }
    // True if node's ownDamage maps into this output this frame.
    bool isNodeDirty(quint64 id) const { return m_dirtyNodes.contains(id); }

    void addFlushRegion(const pixman_region32_t *damage);
    void addFlushRegion(const QRect &damage);

    void finishFrame();

private:
    friend class Tracker;

    QRect m_outputRect;
    QTransform m_worldToOutput;
    QRect m_committedOutputRect;
    QTransform m_committedWorldToOutput;
    bool m_dirty = false;
    bool m_transformFallback = false;

    Region m_worldDamage;
    Region m_outputDamage;
    Region m_flush;
    QSet<quint64> m_dirtyNodes;
};

class Tracker
{
public:
    using Viewport = Gdt::Viewport;
    enum class Phase { Idle, Prepared, Committed };

    Tracker() = default;
    explicit Tracker(Node *root);
    ~Tracker();

    void setRoot(Node *root);
    Node *root() const { return m_root; }

    void prepareFrame();
    void commit(QVector<Viewport> &viewports);
    void finishFrame();

    const pixman_region32_t *damageRegion() const { return m_damage.native(); }

private:
    void mapViewport(Viewport &viewport);
    void collectDirtyNodes(Node *node, Viewport &viewport);

    Node *m_root = nullptr;
    Region m_damage;
    Phase m_phase = Phase::Idle;
};

} // namespace Gdt

#endif // GDTTRACKER_H
