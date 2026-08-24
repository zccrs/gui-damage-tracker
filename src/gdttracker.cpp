#include "gdttracker.h"

#include <utility>

namespace Gdt {

Tracker::Viewport::Viewport(const QRect &outputRect_,
                            const QTransform &worldToOutput_,
                            const QRegion &damage_)
    : outputRect(outputRect_)
    , worldToOutput(worldToOutput_)
    , damage(damage_)
{
    state.viewport = this;
}

Tracker::Viewport::~Viewport()
{
    clearNodeData();
}

Tracker::Viewport::Viewport(const Viewport &other)
    : outputRect(other.outputRect)
    , worldToOutput(other.worldToOutput)
    , damage(other.damage)
{
    state.viewport = this;
    state.damage = other.state.damage;
}

Tracker::Viewport &Tracker::Viewport::operator=(const Viewport &other)
{
    if (this != &other) {
        clearNodeData();
        outputRect = other.outputRect;
        worldToOutput = other.worldToOutput;
        damage = other.damage;
        state.viewport = this;
        state.damage = other.state.damage;
    }
    return *this;
}

Tracker::Viewport::Viewport(Viewport &&other) noexcept
    : outputRect(other.outputRect)
    , worldToOutput(other.worldToOutput)
    , damage(std::move(other.damage))
    , state(std::move(other.state))
    , nodeDataPool(std::move(other.nodeDataPool))
{
    state.viewport = this;
    for (const auto &item : std::as_const(nodeDataPool)) {
        if (item && item->node) {
            item->viewport = this;
            item->node->attachViewportData(item.get());
        }
    }
}

Tracker::Viewport &Tracker::Viewport::operator=(Viewport &&other) noexcept
{
    if (this != &other) {
        clearNodeData();
        outputRect = other.outputRect;
        worldToOutput = other.worldToOutput;
        damage = std::move(other.damage);
        state = std::move(other.state);
        state.viewport = this;
        for (const auto &item : std::as_const(nodeDataPool)) {
            if (item && item->node) {
                item->viewport = this;
                item->node->attachViewportData(item.get());
            }
        }
    }
    return *this;
}

NodeViewportData *Tracker::Viewport::getOrCreateNodeData(Node *node)
{
    if (!node)
        return nullptr;
    auto item = std::make_unique<NodeViewportData>();
    item->viewport = this;
    NodeViewportData *ptr = item.get();
    nodeDataPool.push_back(std::move(item));
    node->attachViewportData(ptr);
    return ptr;
}

void Tracker::Viewport::clearNodeData()
{
    for (const auto &item : std::as_const(nodeDataPool)) {
        if (item && item->node)
            item->node->detachViewportData(item.get());
    }
    nodeDataPool.clear();
}

bool Tracker::Viewport::State::isCulled(const Node *node) const
{
    if (!node)
        return true;
    return node->isCulled(viewport);
}

bool Tracker::Viewport::State::isFullyOccluded(const Node *node) const
{
    if (!node)
        return false;
    return node->isFullyOccluded(viewport);
}

QRegion Tracker::Viewport::State::visibleDamage(const Node *node) const
{
    if (!node)
        return {};
    return node->visibleDamage(viewport);
}

QRegion Tracker::Viewport::State::occludedRegion(const Node *node) const
{
    if (!node)
        return {};
    return node->occludedRegion(viewport);
}

Tracker::Tracker(Node *root)
    : m_root(root)
{
}

void Tracker::setRoot(Node *root)
{
    m_root = root;
}

void Tracker::computeViewport(Viewport &vp, const QRegion &worldDamage)
{
    const bool usableTransform = vp.worldToOutput.isAffine()
        && vp.worldToOutput.isInvertible();

    QRegion remaining;
    if (!usableTransform && !vp.outputRect.isEmpty()
        && (!worldDamage.isEmpty() || !vp.damage.isEmpty())) {
        remaining = QRegion(vp.outputRect);
    } else {
        remaining = mapRegionOuter(vp.worldToOutput, worldDamage);
        if (!vp.damage.isEmpty())
            remaining += vp.damage;
    }
    if (!vp.outputRect.isEmpty())
        remaining &= vp.outputRect;

    QRegion frontOpaque;
    QRegion exposed;
    QRegion screen;

    vp.clearNodeData();
    auto factory = [&vp](Node *n) -> NodeViewportData * {
        return vp.getOrCreateNodeData(n);
    };

    if (m_root) {
        m_root->applyOcclusion(frontOpaque, remaining, exposed, screen,
                               vp.worldToOutput, vp.outputRect, &vp, factory);
    }

    if (!vp.outputRect.isEmpty())
        remaining &= vp.outputRect;
    screen += remaining;
    vp.state.damage = screen;
}

void Tracker::computeAllViewports(QVector<Viewport> &viewports, const QRegion &worldDamage)
{
    std::vector<ViewportOcclusionState> states(viewports.size());

    for (int i = 0; i < viewports.size(); ++i) {
        Viewport &vp = viewports[i];
        const bool usableTransform = vp.worldToOutput.isAffine()
            && vp.worldToOutput.isInvertible();

        QRegion remaining;
        if (!usableTransform && !vp.outputRect.isEmpty()
            && (!worldDamage.isEmpty() || !vp.damage.isEmpty())) {
            remaining = QRegion(vp.outputRect);
        } else {
            remaining = mapRegionOuter(vp.worldToOutput, worldDamage);
            if (!vp.damage.isEmpty())
                remaining += vp.damage;
        }
        if (!vp.outputRect.isEmpty())
            remaining &= vp.outputRect;

        vp.clearNodeData();
        states[i].remaining = std::move(remaining);
        states[i].viewport = &vp;
    }

    auto factory = [](Node *n, Viewport *vp) -> NodeViewportData * {
        return vp->getOrCreateNodeData(n);
    };

    if (m_root) {
        m_root->applyOcclusionMulti(states.data(), viewports.size(), worldDamage, factory);
    }

    for (int i = 0; i < viewports.size(); ++i) {
        Viewport &vp = viewports[i];
        QRegion remaining = std::move(states[i].remaining);
        if (!vp.outputRect.isEmpty())
            remaining &= vp.outputRect;
        vp.state.damage = states[i].screen + remaining;
    }
}

void Tracker::prepareFrame()
{
    m_worldDamage = {};

    if (Q_UNLIKELY(!m_root))
        return;

    const bool treeDirty = m_root->isDirty();
    if (Q_LIKELY(!treeDirty))
        return; // idle frame, nothing to do

    m_root->updateWorld(QTransform(), false);

    // collectBackdrop: world-space damage accumulation (no viewport damage)
    // Viewport swapchain damage is handled per-viewport in renderViewport.
    m_root->collectBackdrop(m_worldDamage);
}

void Tracker::renderViewport(Viewport &vp)
{
    if (Q_UNLIKELY(!m_root)) {
        vp.state.damage = {};
        vp.clearNodeData();
        return;
    }

    const bool treeDirty = m_root->isDirty();
    if (Q_LIKELY(!treeDirty && vp.damage.isEmpty())) {
        vp.state.damage = {};
        return;
    }

    // If tree wasn't dirty but viewport has swapchain damage, we still need
    // to clear per-frame damage (updateWorld wasn't called in prepareFrame)
    if (Q_LIKELY(!treeDirty))
        m_root->clearFrameDamageRecursive();

    // Merge viewport swapchain damage into the per-viewport computation
    QRegion worldDamage = m_worldDamage;
    if (Q_UNLIKELY(!vp.damage.isEmpty())) {
        const QTransform inverse = vp.worldToOutput.inverted();
        worldDamage += mapRegionOuter(inverse, vp.damage);
    }

    computeViewport(vp, worldDamage);
}

void Tracker::finishFrame()
{
    if (Q_UNLIKELY(!m_root))
        return;

    if (m_root->isDirty())
        m_root->commitState();
}

void Tracker::commit(QVector<Viewport> &viewports)
{
    if (Q_UNLIKELY(!m_root)) {
        for (Viewport &vp : viewports) {
            vp.state.damage = {};
            vp.clearNodeData();
        }
        return;
    }

    bool hasViewportDamage = false;
    for (const Viewport &vp : std::as_const(viewports)) {
        if (Q_UNLIKELY(!vp.damage.isEmpty())) {
            hasViewportDamage = true;
            break;
        }
    }

    const bool treeDirty = m_root->isDirty();
    if (Q_LIKELY(!treeDirty && !hasViewportDamage)) {
        for (Viewport &in : viewports)
            in.state.damage = {};
        return;
    }

    // Phase 1: shared
    prepareFrame();

    // If tree was clean but viewport has damage, we need clearFrameDamage
    if (Q_LIKELY(!treeDirty))
        m_root->clearFrameDamageRecursive();

    // Phase 2: per-viewport
    for (Viewport &in : viewports)
        renderViewport(in);

    // Phase 3: shared
    finishFrame();
}

} // namespace Gdt
