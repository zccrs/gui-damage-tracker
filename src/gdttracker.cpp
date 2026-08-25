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

void Tracker::computeViewport(Viewport &vp, const QRegion &presentWorldDamage)
{
    const QTransform &w2o = vp.worldToOutput();
    const QRect &outRect = vp.outputRect();

    QRegion outputDamage;
    if (Q_LIKELY(w2o.isAffine() && w2o.isInvertible())) {
        outputDamage = mapRegionOuter(w2o, presentWorldDamage);
    } else if (!outRect.isEmpty() && !presentWorldDamage.isEmpty()) {
        outputDamage = QRegion(outRect);
    } else {
        outputDamage = mapRegionOuter(w2o, presentWorldDamage);
    }

    if (!outRect.isEmpty())
        outputDamage &= outRect;
    vp.m_accumulatedDamage += outputDamage;
}

void Tracker::prepareFrame()
{
    Q_ASSERT(m_phase == Phase::Idle);
    m_phase = Phase::Prepared;
    m_rawWorldDamage = {};
    m_presentWorldDamage = {};
    if (Q_UNLIKELY(!m_root))
        return;
    const bool treeDirty = m_root->isDirty();
    if (Q_LIKELY(!treeDirty))
        return;
    m_root->updateWorld(QTransform(), false, m_rawWorldDamage);
    QRegion worldFrontOpaque;
    m_root->computeWorldVisibility(worldFrontOpaque, m_presentWorldDamage);
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

    computeViewport(vp, m_presentWorldDamage);
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
