#ifndef GDTTRACKER_H
#define GDTTRACKER_H

#include "gdtnode.h"

#include <QRect>
#include <QRegion>
#include <QTransform>
#include <QVector>

namespace Gdt {

// Viewport: persistent damage ring with self-contained change detection.
// Set outputRect/worldToOutput via setters; if they change since last
// finishFrame(), the viewport is "dirty" and commit() will produce full
// damage (old rect ∪ new rect). finishFrame() clears the dirty flag.
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

    QRegion accumulatedDamage() const { return m_accumulatedDamage; }

    // Called by Tracker::finishFrame() — syncs committed state, clears dirty.
    void finishFrame();

private:
    friend class Tracker;

    QRect m_outputRect;
    QTransform m_worldToOutput;
    QRect m_committedOutputRect;
    QTransform m_committedWorldToOutput;
    bool m_dirty = false;
    QRegion m_accumulatedDamage;
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

    // Phase 1: shared world-space computation (updateWorld + collectWorldDamage)
    void prepareFrame();

    // Phase 2: per-viewport occlusion culling + damage accumulation.
    // If viewport geometry/transform changed since last frame, produces
    // full damage (old rect ∪ new rect) in addition to tree damage.
    void commit(Viewport &vp);

    // Phase 3: shared state commit + sync all viewports' committed state.
    void finishFrame();

    QRegion worldDamage() const { return m_worldDamage; }

private:
    void computeViewport(Viewport &viewport, const QRegion &worldDamage);

    Node *m_root = nullptr;
    QRegion m_worldDamage;
    Phase m_phase = Phase::Idle;
};

} // namespace Gdt

#endif // GDTTRACKER_H
