#include "gdtnode.h"
#include "gdttracker.h"

#include <QMatrix4x4>
namespace Gdt {

quint64 Node::s_nextId = 1;

static QString typeName(Node::Type t)
{
    switch (t) {
    case Node::Type::Basic:
        return QStringLiteral("Basic");
    case Node::Type::Transform:
        return QStringLiteral("Transform");
    case Node::Type::Geometry:
        return QStringLiteral("Geometry");
    case Node::Type::Backdrop:
        return QStringLiteral("Backdrop");
    case Node::Type::Renderer:
        return QStringLiteral("Renderer");
    }
    return QStringLiteral("?");
}

Node::Node(Type type)
    : m_type(type)
    , m_id(s_nextId++)
{
}

Node::~Node()
{
    while (m_viewportDataHead)
        detachViewportData(m_viewportDataHead);

    if (m_parent)
        m_parent->removeChild(this);

    while (m_firstChild) {
        Node *child = m_firstChild;
        unlink(child);
        delete child;
    }
}

TransformNode *Node::toTransform()
{
    return m_type == Type::Transform ? static_cast<TransformNode *>(this) : nullptr;
}

GeometryNode *Node::toGeometry()
{
    return (m_type == Type::Geometry || m_type == Type::Backdrop || m_type == Type::Renderer)
        ? static_cast<GeometryNode *>(this)
        : nullptr;
}

BackdropNode *Node::toBackdrop()
{
    return m_type == Type::Backdrop ? static_cast<BackdropNode *>(this) : nullptr;
}

RendererNode *Node::toRenderer()
{
    return m_type == Type::Renderer ? static_cast<RendererNode *>(this) : nullptr;
}

const TransformNode *Node::toTransform() const
{
    return m_type == Type::Transform ? static_cast<const TransformNode *>(this) : nullptr;
}

const GeometryNode *Node::toGeometry() const
{
    return (m_type == Type::Geometry || m_type == Type::Backdrop || m_type == Type::Renderer)
        ? static_cast<const GeometryNode *>(this)
        : nullptr;
}

const BackdropNode *Node::toBackdrop() const
{
    return m_type == Type::Backdrop ? static_cast<const BackdropNode *>(this) : nullptr;
}

const RendererNode *Node::toRenderer() const
{
    return m_type == Type::Renderer ? static_cast<const RendererNode *>(this) : nullptr;
}

const NodeViewportData *Node::viewportData(const Viewport *viewport) const
{
    if (!viewport)
        return m_viewportDataHead;
    for (const NodeViewportData *curr = m_viewportDataHead; curr; curr = curr->nextOnNode) {
        if (curr->viewport == viewport)
            return curr;
    }
    return nullptr;
}

NodeViewportData *Node::viewportData(const Viewport *viewport)
{
    if (!viewport)
        return m_viewportDataHead;
    for (NodeViewportData *curr = m_viewportDataHead; curr; curr = curr->nextOnNode) {
        if (curr->viewport == viewport)
            return curr;
    }
    return nullptr;
}

void Node::attachViewportData(NodeViewportData *data)
{
    if (!data)
        return;
    if (data->node == this)
        return;
    if (data->node)
        data->node->detachViewportData(data);

    data->node = this;
    data->nextOnNode = m_viewportDataHead;
    data->prevOnNode = nullptr;
    if (m_viewportDataHead)
        m_viewportDataHead->prevOnNode = data;
    m_viewportDataHead = data;
}

void Node::detachViewportData(NodeViewportData *data)
{
    if (!data || data->node != this)
        return;

    if (data->prevOnNode)
        data->prevOnNode->nextOnNode = data->nextOnNode;
    else
        m_viewportDataHead = data->nextOnNode;

    if (data->nextOnNode)
        data->nextOnNode->prevOnNode = data->prevOnNode;

    data->node = nullptr;
    data->nextOnNode = nullptr;
    data->prevOnNode = nullptr;
}

QRegion Node::visibleDamage(const Viewport *viewport) const
{
    const auto *d = viewportData(viewport);
    return d ? d->visibleDamage : QRegion();
}

QRegion Node::occludedRegion(const Viewport *viewport) const
{
    const auto *d = viewportData(viewport);
    return d ? d->occludedRegion : QRegion();
}

bool Node::isFullyOccluded(const Viewport *viewport) const
{
    const auto *d = viewportData(viewport);
    return d ? d->fullyOccluded : false;
}

bool Node::isCulled(const Viewport *viewport) const
{
    const auto *d = viewportData(viewport);
    return d ? d->culled : hasContent();
}

void Node::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    markDirty(DirtyVisibility);
}

void Node::setHasContent(bool hasContent)
{
    if (m_hasContent == hasContent)
        return;
    m_hasContent = hasContent;
    markDirty(DirtyGeometry);
}

void Node::markDirty(DirtyBits bits)
{
    m_dirty |= bits;
    for (Node *p = m_parent; p; p = p->m_parent) {
        if (p->m_dirty & DirtySubtree)
            break;
        p->m_dirty |= DirtySubtree;
    }
}

void Node::adopt(Node *child)
{
    if (child->m_parent)
        child->m_parent->removeChild(child);
}

void Node::attach(Node *child, Node *prev, Node *next)
{
    adopt(child);
    child->m_parent = this;
    child->m_prev = prev;
    child->m_next = next;
    if (prev)
        prev->m_next = child;
    else
        m_firstChild = child;
    if (next)
        next->m_prev = child;
    else
        m_lastChild = child;
    ++m_childCount;
    for (Node *p = this; p; p = p->m_parent)
        p->m_subtreeNodeCount += child->m_subtreeNodeCount;
    child->markDirty(DirtyAdded);
    markDirty(DirtyStructure);
}

void Node::unlink(Node *child)
{
    if (child->m_prev)
        child->m_prev->m_next = child->m_next;
    else
        m_firstChild = child->m_next;
    if (child->m_next)
        child->m_next->m_prev = child->m_prev;
    else
        m_lastChild = child->m_prev;
    child->m_parent = nullptr;
    child->m_prev = nullptr;
    child->m_next = nullptr;
    --m_childCount;
    for (Node *p = this; p; p = p->m_parent)
        p->m_subtreeNodeCount -= child->m_subtreeNodeCount;
}

void Node::prependChild(Node *child)
{
    Q_ASSERT(child);
    Q_ASSERT(child != this);
    attach(child, nullptr, m_firstChild);
}

void Node::appendChild(Node *child)
{
    Q_ASSERT(child);
    Q_ASSERT(child != this);
    attach(child, m_lastChild, nullptr);
}

void Node::insertChildBefore(Node *child, Node *before)
{
    Q_ASSERT(child);
    Q_ASSERT(child != this);
    if (!before) {
        appendChild(child);
        return;
    }
    Q_ASSERT(before->m_parent == this);
    attach(child, before->m_prev, before);
}

void Node::insertChildAfter(Node *child, Node *after)
{
    Q_ASSERT(child);
    Q_ASSERT(child != this);
    if (!after) {
        prependChild(child);
        return;
    }
    Q_ASSERT(after->m_parent == this);
    attach(child, after, after->m_next);
}

void Node::removeChild(Node *child)
{
    Q_ASSERT(child);
    Q_ASSERT(child->m_parent == this);
    m_pendingRemovedDamage += child->m_committedSubtreeAABB;
    unlink(child);
    markDirty(DirtyStructure);
}

void Node::removeAllChildren()
{
    while (m_firstChild)
        removeChild(m_firstChild);
}

Node *Node::takeChild(Node *child)
{
    removeChild(child);
    return child;
}

void Node::clearFrameDamageRecursive()
{
    m_ownDamage = {};
    m_inducedDamage = {};
    m_visibleDamage = {};
    m_occludedRegion = {};
    m_fullyOccluded = false;
    m_culled = false;
    for (Node *child = m_firstChild; child; child = child->m_next)
        child->clearFrameDamageRecursive();
}

void Node::updateWorld(const QTransform &parentWorld, bool parentWorldChanged)
{
    m_inducedDamage = {};
    m_visibleDamage = {};
    m_occludedRegion = {};
    m_fullyOccluded = false;
    m_culled = false;
    m_ownDamage = m_pendingRemovedDamage;
    m_pendingRemovedDamage = {};

    const bool matrixChanged = parentWorldChanged
        || (m_type == Type::Transform && (m_dirty & DirtyMatrix));

    if (!matrixChanged && m_dirty == 0) {
        clearFrameDamageRecursive();
        return;
    }

    if (m_type == Type::Transform)
        m_worldTransform = parentWorld * static_cast<TransformNode *>(this)->m_matrix;
    else
        m_worldTransform = parentWorld;

    if (!m_visible) {
        if (m_committedVisible)
            m_ownDamage += m_committedSubtreeAABB;
        m_worldBounds = {};
        m_subtreeAABB = {};
        m_worldOpaque = {};
        return;
    }

    const bool appearing = !m_committedVisible || (m_dirty & DirtyAdded);
    const bool visibilityFlipped = m_visible != m_committedVisible;
    const bool shapeChanged = matrixChanged
        || (m_dirty & (DirtyGeometry | DirtyOpaque))
        || visibilityFlipped;

    if (hasContent()) {
        auto *geo = static_cast<GeometryNode *>(this);
        if (geo->m_fullyOpaque)
            geo->syncFullyOpaqueRegion();

        m_worldBounds = mapOuter(m_worldTransform, geo->m_boundingRect);
        m_worldOpaque = mapRegionInner(m_worldTransform, geo->m_opaqueRegion);

        if (appearing) {
            m_ownDamage += m_worldBounds;
        } else {
            if (shapeChanged) {
                m_ownDamage += m_committedWorldBounds;
                m_ownDamage += m_worldBounds;
            }
            if (m_dirty & DirtyContent) {
                QRegion mapped = mapRegionOuter(m_worldTransform, geo->m_pendingContentDamage);
                mapped &= m_worldBounds;
                m_ownDamage += mapped;
            }
        }
        geo->m_pendingContentDamage = {};
    } else if (matrixChanged) {
        m_worldBounds = {};
    }

    QRect subtree = m_worldBounds;
    for (Node *child = m_firstChild; child; child = child->m_next) {
        child->updateWorld(m_worldTransform, matrixChanged);
        subtree = subtree.united(child->m_subtreeAABB);
    }
    m_subtreeAABB = subtree;
}

void Node::collectBackdrop(QRegion &acc)
{
    if (!m_visible) {
        acc += m_ownDamage;
        return;
    }

    if (m_type == Type::Backdrop) {
        auto *bg = static_cast<BackdropNode *>(this);
        const QRect sample = m_worldBounds.marginsAdded(bg->m_expansion);
        QRegion relevant = acc;
        relevant &= sample;
        m_inducedDamage = dilateRegion(relevant, bg->m_expansion);
        if (bg->m_clipExpansion)
            m_inducedDamage &= m_worldBounds;
        else
            m_inducedDamage &= m_worldBounds.marginsAdded(bg->m_expansion);
        acc += m_inducedDamage;
    } else if (m_type == Type::Renderer) {
        auto *rnd = static_cast<RendererNode *>(this);
        if (rnd->m_damageFunc) {
            RenderContext ctx;
            ctx.viewport = nullptr;
            ctx.overallDamage = acc;
            ctx.worldTransform = m_worldTransform;
            ctx.renderMatrix = m_worldTransform;
            ctx.boundingRect = rnd->boundingRect();
            ctx.worldBounds = m_worldBounds;
            ctx.outputBounds = m_worldBounds;
            ctx.node = rnd;

            const QRegion customDamage = rnd->m_damageFunc(ctx);
            m_inducedDamage = customDamage;
            acc += customDamage;
        }
    }
    acc += m_ownDamage;

    for (Node *child = m_firstChild; child; child = child->m_next)
        child->collectBackdrop(acc);
}

void Node::applyOcclusion(QRegion &frontOpaque, QRegion &remaining, QRegion &exposed,
                          QRegion &screen, const QTransform &worldToOutput,
                          const QRect &outputRect, const Viewport *viewport,
                          const std::function<NodeViewportData *(Node *)> &dataFactory)
{
    if (!m_visible) {
        if (hasContent()) {
            NodeViewportData *view = dataFactory(this);
            view->viewport = viewport;
            view->culled = true;
            view->fullyOccluded = false;
            view->occludedRegion = {};
            view->visibleDamage = {};
        }
        return;
    }

    // Subtree AABB culling: skip entire off-screen subtrees.
    if (!outputRect.isEmpty() && !m_subtreeAABB.isEmpty()) {
        const QRect outputSubtreeAABB = mapOuter(worldToOutput, QRectF(m_subtreeAABB));
        if (!outputSubtreeAABB.intersects(outputRect))
            return;
    }

    for (Node *child = m_lastChild; child; child = child->m_prev) {
        child->applyOcclusion(frontOpaque, remaining, exposed, screen,
                              worldToOutput, outputRect, viewport, dataFactory);
    }

    const bool usableTransform = worldToOutput.isAffine() && worldToOutput.isInvertible();
    const bool hasDamage = !m_ownDamage.isEmpty() || !m_inducedDamage.isEmpty();

    // Map damage to output coordinates. When only one source is non-empty,
    // map it directly — avoids a QRegion::united() allocation.
    QRegion localDamage;
    if (hasDamage) {
        QRegion worldLocalDamage;
        if (m_inducedDamage.isEmpty())
            worldLocalDamage = m_ownDamage;       // COW copy, no alloc
        else if (m_ownDamage.isEmpty())
            worldLocalDamage = m_inducedDamage;   // COW copy, no alloc
        else
            worldLocalDamage = m_ownDamage + m_inducedDamage;

        if (usableTransform) {
            localDamage = mapRegionOuter(worldToOutput, worldLocalDamage);
        } else if (!outputRect.isEmpty()) {
            localDamage = QRegion(outputRect);
        } else {
            localDamage = mapRegionOuter(worldToOutput, worldLocalDamage);
        }
        if (!outputRect.isEmpty())
            localDamage &= outputRect;
    }

    if (!hasContent()) {
        if (!localDamage.isEmpty())
            screen += localDamage;
        return;
    }

    // Fast path: damage-free, non-opaque, non-exposed displayable node.
    // No contribution to screen/remaining/exposed/frontOpaque — only need
    // culled/fullyOccluded/occludedRegion for the view. Uses QRect when the
    // front-opaque AABB doesn't intersect (avoids QRegion heap ops).
    if (!hasDamage && m_worldOpaque.isEmpty() && exposed.isEmpty()) {
        QRect boundsRect;
        if (usableTransform)
            boundsRect = mapOuter(worldToOutput, QRectF(m_worldBounds));
        else if (!outputRect.isEmpty())
            boundsRect = outputRect;
        else
            boundsRect = mapOuter(worldToOutput, QRectF(m_worldBounds));
        if (!outputRect.isEmpty())
            boundsRect &= outputRect;

        NodeViewportData *view = dataFactory(this);
        view->viewport = viewport;
        view->outputBounds = boundsRect;
        view->visibleDamage = {};
        if (boundsRect.isEmpty()) {
            view->culled = true;
            view->fullyOccluded = false;
            view->occludedRegion = {};
        } else if (!frontOpaque.isEmpty()) {
            // Check full occlusion with QRect API — avoids QRegion subtraction.
            if (frontOpaque.contains(boundsRect)) {
                view->fullyOccluded = true;
                view->culled = true;
                view->occludedRegion = QRegion(boundsRect);
            } else {
                view->fullyOccluded = false;
                view->culled = false;
                const QRect frontAABB = frontOpaque.boundingRect();
                if (frontAABB.intersects(boundsRect))
                    view->occludedRegion = frontOpaque & boundsRect;
                else
                    view->occludedRegion = {};
            }
        } else {
            view->culled = false;
            view->fullyOccluded = false;
            view->occludedRegion = {};
        }
        return;
    }

    QRegion boundsReg;
    if (usableTransform) {
        boundsReg = QRegion(mapOuter(worldToOutput, QRectF(m_worldBounds)));
    } else if (!outputRect.isEmpty()) {
        boundsReg = QRegion(outputRect);
    } else {
        boundsReg = QRegion(mapOuter(worldToOutput, QRectF(m_worldBounds)));
    }
    if (!outputRect.isEmpty())
        boundsReg &= outputRect;

    QRegion opaque;
    if (!m_worldOpaque.isEmpty()) {
        if (usableTransform)
            opaque = mapRegionInner(worldToOutput, m_worldOpaque);
        if (!outputRect.isEmpty())
            opaque &= outputRect;
    }

    QRegion visibleLocalDamage;
    QRegion nonOccludedBounds;
    NodeViewportData *view = dataFactory(this);
    view->viewport = viewport;
    view->outputBounds = boundsReg.boundingRect();
    if (frontOpaque.isEmpty()) {
        view->occludedRegion = {};
        view->fullyOccluded = false;
        view->culled = boundsReg.isEmpty();
        nonOccludedBounds = boundsReg;
        visibleLocalDamage = localDamage;
    } else {
        const QRect frontAABB = frontOpaque.boundingRect();
        const QRect nodeAABB = boundsReg.boundingRect();

        if (!frontAABB.intersects(nodeAABB)) {
            view->occludedRegion = {};
            view->fullyOccluded = false;
            view->culled = boundsReg.isEmpty();
            nonOccludedBounds = boundsReg;
            visibleLocalDamage = localDamage;
        } else {
            view->occludedRegion = boundsReg & frontOpaque;
            nonOccludedBounds = boundsReg - frontOpaque;
            view->fullyOccluded = !boundsReg.isEmpty() && nonOccludedBounds.isEmpty();
            view->culled = boundsReg.isEmpty() || view->fullyOccluded;
            visibleLocalDamage = localDamage.isEmpty() ? QRegion() : (localDamage - frontOpaque);
        }
    }

    if (exposed.isEmpty()) {
        if (!visibleLocalDamage.isEmpty())
            view->visibleDamage = visibleLocalDamage & boundsReg;
        else
            view->visibleDamage = {};
    } else {
        if (!nonOccludedBounds.isEmpty())
            view->visibleDamage = exposed & nonOccludedBounds;
        else
            view->visibleDamage = {};
        if (!visibleLocalDamage.isEmpty())
            view->visibleDamage += visibleLocalDamage & boundsReg;
    }

    if (!visibleLocalDamage.isEmpty())
        screen += visibleLocalDamage;

    if (!opaque.isEmpty()) {
        if (!remaining.isEmpty())
            remaining -= opaque;
        if (!exposed.isEmpty())
            exposed -= opaque;
        if (!visibleLocalDamage.isEmpty()) {
            // Short-circuit: if AABBs don't intersect, subtraction is a no-op.
            const QRect opaqueAABB = opaque.boundingRect();
            const QRect damageAABB = visibleLocalDamage.boundingRect();
            if (opaqueAABB.intersects(damageAABB))
                exposed += visibleLocalDamage - opaque;
            else
                exposed += visibleLocalDamage;
        }
        frontOpaque += opaque;
    } else {
        if (!visibleLocalDamage.isEmpty())
            exposed += visibleLocalDamage;
    }
}

void Node::applyOcclusionMulti(ViewportOcclusionState *states, int count,
                                const QRegion &worldDamage,
                                const std::function<NodeViewportData *(Node *, Viewport *)> &dataFactory)
{
    if (!m_visible) {
        if (hasContent()) {
            for (int i = 0; i < count; ++i) {
                auto &vps = states[i];
                NodeViewportData *view = dataFactory(this, vps.viewport);
                view->viewport = vps.viewport;
                view->culled = true;
                view->fullyOccluded = false;
                view->occludedRegion = {};
                view->visibleDamage = {};
            }
        }
        return;
    }

    // Subtree AABB culling per viewport: skip off-screen subtrees for that viewport only.
    // The child recursion still happens once — each viewport's remaining/visible state
    // is preserved because applyOcclusion for that viewport was skipped.
    for (int i = 0; i < count; ++i) {
        auto &vps = states[i];
        const auto &vp = *vps.viewport;
        if (!vp.outputRect.isEmpty() && !m_subtreeAABB.isEmpty()) {
            const QRect outputSubtreeAABB = mapOuter(vp.worldToOutput, QRectF(m_subtreeAABB));
            if (!outputSubtreeAABB.intersects(vp.outputRect)) {
                // Mark all content nodes in this subtree as culled for this viewport
                if (hasContent()) {
                    NodeViewportData *view = dataFactory(this, vps.viewport);
                    view->viewport = vps.viewport;
                    view->culled = true;
                    view->fullyOccluded = false;
                    view->occludedRegion = {};
                    view->visibleDamage = {};
                }
                vps.skipped = true;
            }
        }
    }

    // Recurse children ONCE (not per viewport)
    for (Node *child = m_lastChild; child; child = child->m_prev) {
        // Save/restore per-viewport state so each child sees the parent's accumulated frontOpaque
        child->applyOcclusionMulti(states, count, worldDamage, dataFactory);
    }

    // Per-viewport occlusion math for THIS node
    const bool hasDamage = !m_ownDamage.isEmpty() || !m_inducedDamage.isEmpty();
    QRegion worldLocalDamage;
    if (hasDamage) {
        if (m_inducedDamage.isEmpty())
            worldLocalDamage = m_ownDamage;
        else if (m_ownDamage.isEmpty())
            worldLocalDamage = m_inducedDamage;
        else
            worldLocalDamage = m_ownDamage + m_inducedDamage;
    }

    for (int i = 0; i < count; ++i) {
        auto &vps = states[i];
        if (vps.skipped) {
            vps.skipped = false;
            continue;
        }
        const auto &vp = *vps.viewport;

        const bool usableTransform = vp.worldToOutput.isAffine() && vp.worldToOutput.isInvertible();

        QRegion localDamage;
        if (hasDamage) {
            if (usableTransform) {
                localDamage = mapRegionOuter(vp.worldToOutput, worldLocalDamage);
            } else if (!vp.outputRect.isEmpty()) {
                localDamage = QRegion(vp.outputRect);
            } else {
                localDamage = mapRegionOuter(vp.worldToOutput, worldLocalDamage);
            }
            if (!vp.outputRect.isEmpty())
                localDamage &= vp.outputRect;
        }

        if (!hasContent()) {
            if (!localDamage.isEmpty())
                vps.screen += localDamage;
            continue;
        }

        // Fast path: damage-free, non-opaque, non-exposed
        if (!hasDamage && m_worldOpaque.isEmpty() && vps.exposed.isEmpty()) {
            QRect boundsRect;
            if (usableTransform)
                boundsRect = mapOuter(vp.worldToOutput, QRectF(m_worldBounds));
            else if (!vp.outputRect.isEmpty())
                boundsRect = vp.outputRect;
            else
                boundsRect = mapOuter(vp.worldToOutput, QRectF(m_worldBounds));
            if (!vp.outputRect.isEmpty())
                boundsRect &= vp.outputRect;

            NodeViewportData *view = dataFactory(this, vps.viewport);
            view->viewport = vps.viewport;
            view->outputBounds = boundsRect;
            view->visibleDamage = {};
            if (boundsRect.isEmpty()) {
                view->culled = true;
                view->fullyOccluded = false;
                view->occludedRegion = {};
            } else if (!vps.frontOpaque.isEmpty()) {
                if (vps.frontOpaque.contains(boundsRect)) {
                    view->fullyOccluded = true;
                    view->culled = true;
                    view->occludedRegion = QRegion(boundsRect);
                } else {
                    view->fullyOccluded = false;
                    view->culled = false;
                    const QRect frontAABB = vps.frontOpaque.boundingRect();
                    if (frontAABB.intersects(boundsRect))
                        view->occludedRegion = vps.frontOpaque & boundsRect;
                    else
                        view->occludedRegion = {};
                }
            } else {
                view->culled = false;
                view->fullyOccluded = false;
                view->occludedRegion = {};
            }
            continue;
        }

        QRegion boundsReg;
        if (usableTransform)
            boundsReg = QRegion(mapOuter(vp.worldToOutput, QRectF(m_worldBounds)));
        else if (!vp.outputRect.isEmpty())
            boundsReg = QRegion(vp.outputRect);
        else
            boundsReg = QRegion(mapOuter(vp.worldToOutput, QRectF(m_worldBounds)));
        if (!vp.outputRect.isEmpty())
            boundsReg &= vp.outputRect;

        QRegion opaque;
        if (!m_worldOpaque.isEmpty()) {
            if (usableTransform)
                opaque = mapRegionInner(vp.worldToOutput, m_worldOpaque);
            if (!vp.outputRect.isEmpty())
                opaque &= vp.outputRect;
        }

        QRegion visibleLocalDamage;
        QRegion nonOccludedBounds;
        NodeViewportData *view = dataFactory(this, vps.viewport);
        view->viewport = vps.viewport;
        view->outputBounds = boundsReg.boundingRect();
        if (vps.frontOpaque.isEmpty()) {
            view->occludedRegion = {};
            view->fullyOccluded = false;
            view->culled = boundsReg.isEmpty();
            nonOccludedBounds = boundsReg;
            visibleLocalDamage = localDamage;
        } else {
            const QRect frontAABB = vps.frontOpaque.boundingRect();
            const QRect nodeAABB = boundsReg.boundingRect();

            if (!frontAABB.intersects(nodeAABB)) {
                view->occludedRegion = {};
                view->fullyOccluded = false;
                view->culled = boundsReg.isEmpty();
                nonOccludedBounds = boundsReg;
                visibleLocalDamage = localDamage;
            } else {
                view->occludedRegion = boundsReg & vps.frontOpaque;
                nonOccludedBounds = boundsReg - vps.frontOpaque;
                view->fullyOccluded = !boundsReg.isEmpty() && nonOccludedBounds.isEmpty();
                view->culled = boundsReg.isEmpty() || view->fullyOccluded;
                visibleLocalDamage = localDamage.isEmpty() ? QRegion() : (localDamage - vps.frontOpaque);
            }
        }

        if (vps.exposed.isEmpty()) {
            if (!visibleLocalDamage.isEmpty())
                view->visibleDamage = visibleLocalDamage & boundsReg;
            else
                view->visibleDamage = {};
        } else {
            if (!nonOccludedBounds.isEmpty())
                view->visibleDamage = vps.exposed & nonOccludedBounds;
            else
                view->visibleDamage = {};
            if (!visibleLocalDamage.isEmpty())
                view->visibleDamage += visibleLocalDamage & boundsReg;
        }

        if (!visibleLocalDamage.isEmpty())
            vps.screen += visibleLocalDamage;

        if (!opaque.isEmpty()) {
            if (!vps.remaining.isEmpty())
                vps.remaining -= opaque;
            if (!vps.exposed.isEmpty())
                vps.exposed -= opaque;
            if (!visibleLocalDamage.isEmpty()) {
                const QRect opaqueAABB = opaque.boundingRect();
                const QRect damageAABB = visibleLocalDamage.boundingRect();
                if (opaqueAABB.intersects(damageAABB))
                    vps.exposed += visibleLocalDamage - opaque;
                else
                    vps.exposed += visibleLocalDamage;
            }
            vps.frontOpaque += opaque;
        } else {
            if (!visibleLocalDamage.isEmpty())
                vps.exposed += visibleLocalDamage;
        }
    }
}

void Node::commitState()
{
    if (m_visible) {
        m_committedWorldBounds = m_worldBounds;
        m_committedSubtreeAABB = m_subtreeAABB;
    } else {
        m_committedWorldBounds = {};
        m_committedSubtreeAABB = {};
    }
    m_committedVisible = m_visible;
    m_dirty = {};

    for (Node *child = m_firstChild; child; child = child->m_next)
        child->commitState();
}

void Node::dumpTreeRecursive(QString &out, int depth) const
{
    out += QString(depth * 2, QLatin1Char(' '));
    out += typeName(m_type);
    if (!m_name.isEmpty()) {
        out += QLatin1Char(' ');
        out += QLatin1Char('"');
        out += m_name;
        out += QLatin1Char('"');
    }
    out += QStringLiteral(" id=");
    out += QString::number(m_id);
    if (hasContent()) {
        out += QStringLiteral(" bounds=");
        const QRect r = m_worldBounds.isEmpty() ? outerAligned(toGeometry()->boundingRect())
                                                : m_worldBounds;
        out += QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
    }
    if (m_type == Type::Transform) {
        const QTransform t = static_cast<const TransformNode *>(this)->matrix();
        out += QStringLiteral(" m=(%1,%2)").arg(t.dx()).arg(t.dy());
    }
    if (!m_visible)
        out += QStringLiteral(" hidden");
    if (m_fullyOccluded && hasContent())
        out += QStringLiteral(" occluded");
    else if (m_culled && hasContent())
        out += QStringLiteral(" culled");
    out += QLatin1Char('\n');
    for (Node *child = m_firstChild; child; child = child->m_next)
        child->dumpTreeRecursive(out, depth + 1);
}

QString Node::dumpTree() const
{
    QString out;
    dumpTreeRecursive(out, 0);
    return out;
}

TransformNode::TransformNode()
    : Node(Type::Transform)
{
}

void TransformNode::setMatrix(const QTransform &matrix)
{
    if (m_matrix == matrix)
        return;
    m_matrix = matrix;
    markDirty(DirtyMatrix);
}

void TransformNode::setMatrix(const QMatrix4x4 &matrix)
{
    setMatrix(matrix.toTransform());
}

void TransformNode::setTranslation(qreal x, qreal y)
{
    QTransform t;
    t.translate(x, y);
    setMatrix(t);
}

void TransformNode::setScale(qreal sx, qreal sy)
{
    QTransform t;
    t.scale(sx, sy);
    setMatrix(t);
}

void TransformNode::setRotation(qreal degrees, Qt::Axis axis)
{
    QTransform t;
    t.rotate(degrees, axis);
    setMatrix(t);
}

GeometryNode::GeometryNode()
    : Node(Type::Geometry)
{
    m_hasContent = true;
}

GeometryNode::GeometryNode(Type type)
    : Node(type)
{
    m_hasContent = true;
}
void GeometryNode::syncFullyOpaqueRegion()
{
    m_opaqueRegion = QRegion(innerAligned(m_boundingRect));
}

void GeometryNode::setBoundingRect(const QRectF &rect)
{
    if (m_boundingRect == rect)
        return;
    m_boundingRect = rect;
    if (m_fullyOpaque)
        syncFullyOpaqueRegion();
    markDirty(DirtyGeometry);
}

void GeometryNode::setOpaqueRegion(const QRegion &localOpaque)
{
    if (m_opaqueRegion == localOpaque && !m_fullyOpaque)
        return;
    m_fullyOpaque = false;
    m_opaqueRegion = localOpaque;
    markDirty(DirtyOpaque);
}

void GeometryNode::setFullyOpaque(bool fullyOpaque)
{
    if (m_fullyOpaque == fullyOpaque && fullyOpaque)
        return;
    if (!fullyOpaque && !m_fullyOpaque && m_opaqueRegion.isEmpty())
        return;
    m_fullyOpaque = fullyOpaque;
    if (fullyOpaque)
        syncFullyOpaqueRegion();
    else
        m_opaqueRegion = {};
    markDirty(DirtyOpaque);
}

void GeometryNode::markContentDirty(const QRegion &localRegion)
{
    if (localRegion.isEmpty())
        return;
    m_pendingContentDamage += localRegion;
    markDirty(DirtyContent);
}

void GeometryNode::markContentDirty(const QRect &localRect)
{
    if (localRect.isEmpty())
        return;
    markContentDirty(QRegion(localRect));
}

BackdropNode::BackdropNode()
    : GeometryNode(Type::Backdrop)
{
}

void BackdropNode::setBackdropExpansion(const QMargins &margins)
{
    if (m_expansion == margins)
        return;
    m_expansion = margins;
    markDirty(DirtyGeometry);
}

void BackdropNode::setBackdropExpansion(int px)
{
    setBackdropExpansion(QMargins(px, px, px, px));
}

void BackdropNode::setClipExpansion(bool clip)
{
    if (m_clipExpansion == clip)
        return;
    m_clipExpansion = clip;
    markDirty(DirtyGeometry);
}

RendererNode::RendererNode()
    : GeometryNode(Type::Renderer)
{
}

void RendererNode::setDamageFunction(DamageFunction func)
{
    m_damageFunc = std::move(func);
    markDirty(DirtyContent);
}

} // namespace Gdt
