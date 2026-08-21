#include "gdttracker.h"

#include <utility>

namespace Gdt {

Tracker::Tracker(Node *root)
    : m_root(root)
{
}

void Tracker::setRoot(Node *root)
{
    if (m_root == root)
        return;
    m_root = root;
    m_lastViewports.clear();
    m_hasCommitted = false;
}

void Tracker::computeViewport(Viewport &vp, const QRegion &worldDamage,
                             const QRegion &extra)
{
    const bool usableTransform = vp.worldToOutput.isAffine()
        && vp.worldToOutput.isInvertible();

    QRegion remaining;
    if (!usableTransform && !vp.outputRect.isEmpty()
        && (!worldDamage.isEmpty() || !extra.isEmpty())) {
        remaining = QRegion(vp.outputRect);
    } else {
        remaining = mapRegionOuter(vp.worldToOutput, worldDamage);
        remaining += extra;
    }
    if (!vp.outputRect.isEmpty())
        remaining &= vp.outputRect;

    QRegion frontOpaque;
    QRegion exposed;
    QRegion screen;
    vp.state.nodes.clear();
    if (m_root)
        vp.state.nodes.reserve(m_root->subtreeNodeCount());
    m_root->applyOcclusion(frontOpaque, remaining, exposed, screen,
                           vp.worldToOutput, vp.outputRect, &vp.state.nodes);

    if (!vp.outputRect.isEmpty())
        remaining &= vp.outputRect;
    screen += remaining;
    vp.state.damage = screen;
}

void Tracker::mirrorNodeViews(const Viewport &vp)
{
    if (m_root)
        m_root->applyViewsRecursive(vp.state.nodes);
}

QRegion Tracker::commit()
{
    Viewport vp;
    vp.id = PrimaryViewport;
    QVector<Viewport> vps{vp};
    commit(vps);
    return vps.first().state.damage;
}

QRegion Tracker::commit(const QRect &outputRect)
{
    Viewport vp;
    vp.id = PrimaryViewport;
    vp.outputRect = outputRect;
    QVector<Viewport> vps{vp};
    commit(vps);
    return vps.first().state.damage;
}

void Tracker::commit(QVector<Viewport> &viewports)
{
    if (!m_root) {
        m_lastViewports.clear();
        m_hasCommitted = true;
        for (Viewport &vp : viewports) {
            vp.state.damage = {};
            vp.state.nodes.clear();
        }
        return;
    }

    QHash<ViewportId, QRegion> extra;
    bool extraDamage = false;
    for (const Viewport &in : viewports) {
        if (!in.damage.isEmpty()) {
            QRegion clippedDamage = in.damage;
            if (!in.outputRect.isEmpty())
                clippedDamage &= in.outputRect;
            if (!clippedDamage.isEmpty()) {
                extra[in.id] += clippedDamage;
                extraDamage = true;
            }
        }

        const auto it = m_lastViewports.constFind(in.id);
        if (it == m_lastViewports.cend()) {
            if (m_hasCommitted) {
                QRegion damage;
                if (!in.outputRect.isEmpty()) {
                    damage = QRegion(in.outputRect);
                } else {
                    damage = mapRegionOuter(in.worldToOutput,
                                            QRegion(m_root->subtreeBounds()));
                }
                if (!damage.isEmpty()) {
                    extra[in.id] += damage;
                    extraDamage = true;
                }
            }
            continue;
        }

        if (it->worldToOutput != in.worldToOutput) {
            QRegion damage;
            if (!in.outputRect.isEmpty()) {
                damage = QRegion(in.outputRect);
            } else {
                const QRegion bounds(m_root->subtreeBounds());
                damage = mapRegionOuter(it->worldToOutput, bounds);
                damage += mapRegionOuter(in.worldToOutput, bounds);
            }
            if (!damage.isEmpty()) {
                extra.insert(in.id, damage);
                extraDamage = true;
            }
        } else if (!it->outputRect.isEmpty() && !in.outputRect.isEmpty()
                   && it->outputRect != in.outputRect) {
            extra.insert(in.id, QRegion(it->outputRect) ^ QRegion(in.outputRect));
            extraDamage = true;
        }
    }

    const bool treeDirty = m_root->isDirty();
    if (!treeDirty && !extraDamage) {
        for (Viewport &in : viewports) {
            in.state.damage = {};
        }
        m_lastViewports.clear();
        for (const Viewport &in : viewports)
            m_lastViewports.insert(in.id, in);
        m_hasCommitted = true;
        return;
    }

    if (treeDirty)
        m_root->updateWorld(QTransform(), false);
    else
        m_root->clearFrameDamageRecursive();

    QRegion worldDamage;
    m_root->collectBackdrop(worldDamage);

    int mirrorIndex = -1;
    for (int i = 0; i < viewports.size(); ++i) {
        Viewport &in = viewports[i];
        computeViewport(in, worldDamage, extra.value(in.id));
        if (mirrorIndex < 0 || in.id == PrimaryViewport)
            mirrorIndex = i;
    }
    if (mirrorIndex >= 0)
        mirrorNodeViews(viewports.at(mirrorIndex));

    if (treeDirty)
        m_root->commitState();

    m_lastViewports.clear();
    for (const Viewport &in : viewports)
        m_lastViewports.insert(in.id, in);
    m_hasCommitted = true;
}

} // namespace Gdt
