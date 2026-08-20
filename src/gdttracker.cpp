#include "gdttracker.h"

#include <utility>

namespace Gdt {

Tracker::Tracker(Node *root)
    : m_root(root)
{
}

void Tracker::setRoot(Node *root)
{
    m_root = root;
    m_results.clear();
    m_lastViewports.clear();
    m_hasCommitted = false;
}

const Tracker::ViewportState *Tracker::findResult(ViewportId id) const
{
    for (const ViewportState &vp : m_results) {
        if (vp.id == id)
            return &vp;
    }
    return nullptr;
}

QRegion Tracker::computeViewport(ViewportState *vp, const QRegion &worldDamage,
                                 const QRegion &extra)
{
    const bool usableTransform = vp->worldToOutput.isAffine()
        && vp->worldToOutput.isInvertible();

    QRegion remaining;
    if (!usableTransform && !vp->outputRect.isEmpty()
        && (!worldDamage.isEmpty() || !extra.isEmpty())) {
        remaining = QRegion(vp->outputRect);
    } else {
        remaining = mapRegionOuter(vp->worldToOutput, worldDamage) + extra;
    }
    if (!vp->outputRect.isEmpty())
        remaining &= vp->outputRect;

    QRegion frontOpaque;
    QRegion exposed;
    QRegion screen;
    vp->nodes.clear();
    if (m_root)
        vp->nodes.reserve(m_root->subtreeNodeCount());
    m_root->applyOcclusion(frontOpaque, remaining, exposed, screen,
                           vp->worldToOutput, vp->outputRect, &vp->nodes);
    if (!vp->outputRect.isEmpty())
        remaining &= vp->outputRect;
    screen += remaining;
    vp->lastDamage = screen;
    return screen;
}

void Tracker::mirrorNodeViews(const ViewportState &vp)
{
    m_root->applyViewsRecursive(vp.nodes);
}

QRegion Tracker::commit()
{
    return commit(QVector<Viewport>{{PrimaryViewport, QRect()}}).value(PrimaryViewport);
}

QRegion Tracker::commit(const QRect &outputRect)
{
    return commit(QVector<Viewport>{{PrimaryViewport, outputRect}}).value(PrimaryViewport);
}

QHash<Tracker::ViewportId, QRegion> Tracker::commit(const QVector<Viewport> &viewports)
{
    QHash<ViewportId, QRegion> out;

    if (!m_root) {
        m_results.clear();
        m_lastViewports.clear();
        m_hasCommitted = true;
        return out;
    }

    QHash<ViewportId, QRegion> extra;
    bool extraDamage = false;
    for (const Viewport &in : viewports) {
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
                    extra.insert(in.id, damage);
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
                damage = mapRegionOuter(it->worldToOutput, bounds)
                    + mapRegionOuter(in.worldToOutput, bounds);
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
        QVector<ViewportState> kept;
        kept.reserve(viewports.size());
        for (const Viewport &in : viewports) {
            out.insert(in.id, QRegion());
            for (ViewportState &vp : m_results) {
                if (vp.id == in.id) {
                    vp.lastDamage = {};
                    vp.outputRect = in.outputRect;
                    vp.worldToOutput = in.worldToOutput;
                    kept.append(std::move(vp));
                    break;
                }
            }
        }
        m_results = std::move(kept);
        m_lastViewports.clear();
        for (const Viewport &in : viewports)
            m_lastViewports.insert(in.id, in);
        m_hasCommitted = true;
        return out;
    }

    if (treeDirty)
        m_root->updateWorld(QTransform(), false);
    else
        m_root->clearFrameDamageRecursive();

    QRegion worldDamage;
    m_root->collectBackdrop(worldDamage);

    m_results.clear();
    m_results.reserve(viewports.size());
    int mirrorIndex = -1;
    for (const Viewport &in : viewports) {
        ViewportState vp;
        vp.id = in.id;
        vp.outputRect = in.outputRect;
        vp.worldToOutput = in.worldToOutput;
        computeViewport(&vp, worldDamage, extra.value(in.id));
        out.insert(in.id, vp.lastDamage);
        m_results.append(std::move(vp));
        if (mirrorIndex < 0 || in.id == PrimaryViewport)
            mirrorIndex = m_results.size() - 1;
    }
    if (mirrorIndex >= 0)
        mirrorNodeViews(m_results.at(mirrorIndex));

    if (treeDirty)
        m_root->commitState();

    m_lastViewports.clear();
    for (const Viewport &in : viewports)
        m_lastViewports.insert(in.id, in);
    m_hasCommitted = true;
    return out;
}

QRegion Tracker::lastDamage() const
{
    if (const ViewportState *primary = findResult(PrimaryViewport))
        return primary->lastDamage;
    if (!m_results.isEmpty())
        return m_results.front().lastDamage;
    return {};
}

QRegion Tracker::damage(ViewportId id) const
{
    if (const ViewportState *vp = findResult(id))
        return vp->lastDamage;
    return {};
}

QRegion Tracker::visibleDamage(ViewportId id, const Node *node) const
{
    if (!node)
        return {};
    const ViewportState *vp = findResult(id);
    if (!vp)
        return {};
    const auto it = vp->nodes.constFind(node->id());
    if (it == vp->nodes.cend())
        return {};
    return it->visibleDamage;
}

QRegion Tracker::occludedRegion(ViewportId id, const Node *node) const
{
    if (!node)
        return {};
    const ViewportState *vp = findResult(id);
    if (!vp)
        return {};
    const auto it = vp->nodes.constFind(node->id());
    if (it == vp->nodes.cend())
        return {};
    return it->occludedRegion;
}

bool Tracker::isFullyOccluded(ViewportId id, const Node *node) const
{
    if (!node)
        return false;
    const ViewportState *vp = findResult(id);
    if (!vp)
        return false;
    const auto it = vp->nodes.constFind(node->id());
    if (it == vp->nodes.cend())
        return false;
    return it->fullyOccluded;
}

bool Tracker::isCulled(ViewportId id, const Node *node) const
{
    if (!node)
        return true;
    const ViewportState *vp = findResult(id);
    if (!vp)
        return true;
    const auto it = vp->nodes.constFind(node->id());
    if (it == vp->nodes.cend())
        return node->isDisplayable();
    return it->culled;
}

} // namespace Gdt
