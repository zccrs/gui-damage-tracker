#include "gdttracker.h"

#include <cmath>

namespace Gdt {

static bool finiteAffine(const QTransform &transform)
{
    return transform.isAffine()
        && transform.isInvertible()
        && std::isfinite(transform.m11())
        && std::isfinite(transform.m12())
        && std::isfinite(transform.m21())
        && std::isfinite(transform.m22())
        && std::isfinite(transform.dx())
        && std::isfinite(transform.dy());
}

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

void Viewport::addFlushRegion(const pixman_region32_t *damage)
{
    m_flush += Region(damage);
    if (!m_outputRect.isEmpty())
        m_flush &= m_outputRect;
}

void Viewport::addFlushRegion(const QRect &damage)
{
    m_flush += damage;
    if (!m_outputRect.isEmpty())
        m_flush &= m_outputRect;
}

void Viewport::finishFrame()
{
    m_committedOutputRect = m_outputRect;
    m_committedWorldToOutput = m_worldToOutput;
    m_dirty = false;
    m_transformFallback = false;
    m_worldDamage = {};
    m_outputDamage = {};
    m_flush = {};
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

void Tracker::prepareFrame()
{
    Q_ASSERT(m_phase == Phase::Idle);
    m_phase = Phase::Prepared;
    m_damage = {};
    if (Q_UNLIKELY(!m_root))
        return;
    if (Q_LIKELY(!m_root->isDirty())) {
        m_root->clearBehindDamageRecursive();
        return;
    }
    Region backdropDamage;
    m_root->updateWorld(QTransform(), false, m_damage, backdropDamage);
    m_damage += backdropDamage;
}

void Tracker::mapViewport(Viewport &vp)
{
    const QRect treeBounds = m_root ? m_root->subtreeBounds() : QRect();
    Region frameDamage(m_damage);
    if (vp.m_dirty)
        frameDamage += treeBounds;

    const bool validTransform = finiteAffine(vp.m_worldToOutput);
    vp.m_transformFallback = !validTransform;

    if (validTransform) {
        if (!vp.m_outputRect.isEmpty()) {
            bool invertible = false;
            const QTransform outputToWorld =
                vp.m_worldToOutput.inverted(&invertible);
            Q_ASSERT(invertible);
            const Region worldViewport =
                mapRegionOuter(outputToWorld, Region(vp.m_outputRect));
            frameDamage &= worldViewport;
        }
        vp.m_worldDamage += frameDamage;
        vp.m_outputDamage = mapRegionOuter(vp.m_worldToOutput, vp.m_worldDamage);
        if (!vp.m_outputRect.isEmpty())
            vp.m_outputDamage &= vp.m_outputRect;
    } else {
        const QRect fallback = vp.m_outputRect.isEmpty() ? treeBounds : vp.m_outputRect;
        if (!frameDamage.isEmpty()) {
            vp.m_worldDamage += treeBounds;
            vp.m_outputDamage = Region(fallback);
        }
    }
}

void Tracker::commit(QVector<Viewport> &viewports)
{
    Q_ASSERT(m_phase == Phase::Prepared);
    m_phase = Phase::Committed;
    if (Q_UNLIKELY(!m_root))
        return;

    bool idle = !m_root->isDirty() && m_damage.isEmpty();
    if (idle) {
        for (const Viewport &viewport : viewports) {
            if (viewport.m_dirty) {
                idle = false;
                break;
            }
        }
    }
    if (Q_LIKELY(idle))
        return;

    if (m_root->isDirty()) {
        Region worldFrontOpaque;
        m_root->computeWorldVisibility(worldFrontOpaque);
    }

    for (Viewport &viewport : viewports)
        mapViewport(viewport);
}

void Tracker::finishFrame()
{
    Q_ASSERT(m_phase == Phase::Committed);
    m_phase = Phase::Idle;
    if (m_root && m_root->isDirty())
        m_root->commitState();
}

} // namespace Gdt
