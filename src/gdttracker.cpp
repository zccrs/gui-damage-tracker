#include "gdttracker.h"

#include <utility>

namespace Gdt {

// --- Viewport ---

void Viewport::setOutputRect(const QRect &rect)
{
    if (m_outputRect == rect)
        return;
    m_outputRect = rect;
    m_dirty = true;
}

void Viewport::setWorldToOutput(const QTransform &transform)
{
    if (m_worldToOutput == transform)
        return;
    m_worldToOutput = transform;
    m_dirty = true;
}

void Viewport::finishFrame()
{
    m_committedOutputRect = m_outputRect;
    m_committedWorldToOutput = m_worldToOutput;
    m_dirty = false;
    m_accumulatedDamage = {};
}

// --- Tracker ---

void Tracker::computeViewport(Viewport &vp, const QRegion &worldDamage)
{
    const QTransform &w2o = vp.worldToOutput();
    const QRect &outRect = vp.outputRect();
    const bool usableTransform = w2o.isAffine() && w2o.isInvertible();

    QRegion remaining;
    if (!usableTransform && !outRect.isEmpty() && !worldDamage.isEmpty()) {
        remaining = QRegion(outRect);
    } else {
        remaining = mapRegionOuter(w2o, worldDamage);
    }
    if (!outRect.isEmpty())
        remaining &= outRect;

    QRegion frontOpaque;
    QRegion exposed;
    QRegion screen;

    if (m_root) {
        m_root->applyOcclusion(frontOpaque, remaining, exposed, screen,
                               w2o, outRect);
    }

    if (!outRect.isEmpty())
        remaining &= outRect;
    screen += remaining;

    vp.m_accumulatedDamage += screen;
    if (!outRect.isEmpty())
        vp.m_accumulatedDamage &= outRect;

}

void Tracker::prepareFrame()
{
    Q_ASSERT(m_phase == Phase::Idle);
    m_phase = Phase::Prepared;
    m_worldDamage = {};
    if (Q_UNLIKELY(!m_root))
        return;
    const bool treeDirty = m_root->isDirty();
    if (Q_LIKELY(!treeDirty))
        return;
    m_root->updateWorld(QTransform(), false, m_worldDamage);
    QRegion worldFrontOpaque;
    m_root->computeWorldVisibility(worldFrontOpaque, m_worldDamage);
}

void Tracker::commit(Viewport &vp)
{
    Q_ASSERT(m_phase == Phase::Prepared || m_phase == Phase::Committed);
    m_phase = Phase::Committed;
    if (Q_UNLIKELY(!m_root))
        return;

    const bool treeDirty = m_root->isDirty();
    if (Q_LIKELY(!treeDirty))
        return;

    QRegion worldDamage = m_worldDamage;
    computeViewport(vp, worldDamage);
}

void Tracker::finishFrame()
{
    Q_ASSERT(m_phase == Phase::Committed);
    m_phase = Phase::Idle;
    if (Q_UNLIKELY(!m_root))
        return;
    if (m_root->isDirty())
        m_root->commitState();
}

Tracker::Tracker(Node *root)
    : m_root(root)
{
}

Tracker::~Tracker() = default;

void Tracker::setRoot(Node *root)
{
    m_root = root;
}

} // namespace Gdt
