#ifndef GDTTRACKER_H
#define GDTTRACKER_H

#include "gdtnode.h"

#include <QRect>
#include <QRegion>
#include <QTransform>
#include <QVector>

namespace Gdt {

// Viewport: output geometry, transform and accumulated scene damage.
// Setters mark it dirty relative to the last finishFrame(). Tracker does not
// turn viewport changes into full damage; the caller/renderer decides how to
// combine viewport and buffer damage.
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

    // Called by the renderer after consuming this frame's damage.
    // Commits viewport configuration and clears accumulatedDamage().
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

    // Phase 1: two shared world-space traversals:
    // forward world-state/damage collection, then reverse opacity/visibility.
    void prepareFrame();

    // Phase 2: map, clip and occlusion-filter scene damage for one viewport.
    // May be called for multiple viewports after one prepareFrame().
    void commit(Viewport &vp);

    // Phase 3: commit shared node state and return to Idle.
    // Viewport::finishFrame() remains the renderer's responsibility.
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
