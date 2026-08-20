#include "gdtnode.h"

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
    return (m_type == Type::Geometry || m_type == Type::Backdrop)
        ? static_cast<GeometryNode *>(this)
        : nullptr;
}

BackdropNode *Node::toBackdrop()
{
    return m_type == Type::Backdrop ? static_cast<BackdropNode *>(this) : nullptr;
}

const TransformNode *Node::toTransform() const
{
    return m_type == Type::Transform ? static_cast<const TransformNode *>(this) : nullptr;
}

const GeometryNode *Node::toGeometry() const
{
    return (m_type == Type::Geometry || m_type == Type::Backdrop)
        ? static_cast<const GeometryNode *>(this)
        : nullptr;
}

const BackdropNode *Node::toBackdrop() const
{
    return m_type == Type::Backdrop ? static_cast<const BackdropNode *>(this) : nullptr;
}

void Node::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    markDirty(DirtyVisibility);
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

    if (isDisplayable()) {
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
                mapped &= QRegion(m_worldBounds);
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
        QRegion induced = dilateRegion(relevant, bg->m_expansion);
        if (bg->m_clipExpansion)
            induced &= m_worldBounds;
        else
            induced &= m_worldBounds.marginsAdded(bg->m_expansion);
        m_inducedDamage = induced;
        acc += induced;
    }

    acc += m_ownDamage;

    for (Node *child = m_firstChild; child; child = child->m_next)
        child->collectBackdrop(acc);
}

void Node::applyOcclusion(QRegion &frontOpaque, QRegion &remaining, QRegion &exposed,
                          QRegion &screen, const QTransform &worldToOutput,
                          const QRect &outputRect, QHash<quint64, NodeViewportView> *out)
{
    if (!m_visible) {
        if (isDisplayable()) {
            NodeViewportView view;
            view.culled = true;
            if (out)
                (*out)[m_id] = view;
            else {
                m_visibleDamage = {};
                m_occludedRegion = {};
                m_fullyOccluded = false;
                m_culled = true;
            }
        }
        return;
    }

    for (Node *child = m_lastChild; child; child = child->m_prev) {
        child->applyOcclusion(frontOpaque, remaining, exposed, screen,
                              worldToOutput, outputRect, out);
    }

    const bool usableTransform = worldToOutput.isAffine() && worldToOutput.isInvertible();
    const QRegion worldLocalDamage = m_ownDamage + m_inducedDamage;
    QRegion localDamage;
    if (usableTransform) {
        localDamage = mapRegionOuter(worldToOutput, worldLocalDamage);
    } else if (!worldLocalDamage.isEmpty() && !outputRect.isEmpty()) {
        localDamage = QRegion(outputRect);
    } else {
        localDamage = mapRegionOuter(worldToOutput, worldLocalDamage);
    }
    if (!outputRect.isEmpty())
        localDamage &= outputRect;

    if (!isDisplayable()) {
        // Child removal damage is stored on its parent. Keep this conservative:
        // the removed child may have painted in front of any surviving sibling.
        screen += localDamage;
        return;
    }

    QRegion boundsReg;
    if (usableTransform) {
        boundsReg = QRegion(mapOuter(worldToOutput, QRectF(m_worldBounds)));
    } else if (!outputRect.isEmpty()) {
        // Unknown projection: retain every visible node and repaint the target.
        boundsReg = QRegion(outputRect);
    } else {
        boundsReg = QRegion(mapOuter(worldToOutput, QRectF(m_worldBounds)));
    }
    if (!outputRect.isEmpty())
        boundsReg &= outputRect;

    QRegion opaque;
    if (usableTransform)
        opaque = mapRegionInner(worldToOutput, m_worldOpaque);
    if (!outputRect.isEmpty())
        opaque &= outputRect;

    NodeViewportView view;
    view.occludedRegion = boundsReg & frontOpaque;
    view.fullyOccluded = !boundsReg.isEmpty() && (boundsReg - frontOpaque).isEmpty();
    view.culled = boundsReg.isEmpty() || view.fullyOccluded;

    // Damage generated by an already-processed front layer can reveal this
    // node. Local damage is evaluated against only the opaque layers in front,
    // so an unchanged opaque node does not inherit hidden damage.
    const QRegion visibleLocalDamage = localDamage - frontOpaque;
    view.visibleDamage = (exposed & (boundsReg - frontOpaque))
        + (visibleLocalDamage & boundsReg);
    screen += visibleLocalDamage;

    remaining -= opaque;
    exposed -= opaque;
    exposed += visibleLocalDamage - opaque;
    frontOpaque += opaque;

    if (out) {
        (*out)[m_id] = view;
    } else {
        m_visibleDamage = view.visibleDamage;
        m_occludedRegion = view.occludedRegion;
        m_fullyOccluded = view.fullyOccluded;
        m_culled = view.culled;
    }
}

void Node::applyViewsRecursive(const QHash<quint64, NodeViewportView> &views)
{
    const auto it = views.constFind(m_id);
    if (it != views.cend()) {
        m_visibleDamage = it->visibleDamage;
        m_occludedRegion = it->occludedRegion;
        m_fullyOccluded = it->fullyOccluded;
        m_culled = it->culled;
    } else {
        m_visibleDamage = {};
        m_occludedRegion = {};
        m_fullyOccluded = false;
        m_culled = isDisplayable();
    }
    for (Node *child = m_firstChild; child; child = child->m_next)
        child->applyViewsRecursive(views);
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
    if (isDisplayable()) {
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
    if (m_fullyOccluded && isDisplayable())
        out += QStringLiteral(" occluded");
    else if (m_culled && isDisplayable())
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
}

GeometryNode::GeometryNode(Type type)
    : Node(type)
{
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

} // namespace Gdt
