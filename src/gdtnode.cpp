#include "gdtnode.h"
#include "gdttracker.h"

#include <QMatrix4x4>

#include <QtCore/qglobal.h>
#include <utility>
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
    }
    return QStringLiteral("?");
}

static QTransform contentToWorld(const QTransform &world, const QRectF &bounds)
{
    if (Q_LIKELY(qFuzzyIsNull(bounds.x()) && qFuzzyIsNull(bounds.y())))
        return world;
    return QTransform::fromTranslate(bounds.x(), bounds.y()) * world;
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
    return m_type == Type::Geometry ? static_cast<GeometryNode *>(this) : nullptr;
}

const TransformNode *Node::toTransform() const
{
    return m_type == Type::Transform ? static_cast<const TransformNode *>(this) : nullptr;
}

const GeometryNode *Node::toGeometry() const
{
    return m_type == Type::Geometry ? static_cast<const GeometryNode *>(this) : nullptr;
}

void Node::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    markDirty(DirtyVisibility);
}
void Node::setNeedsBackdrop(bool needsBackdrop)
{
    if (m_needsBackdrop == needsBackdrop)
        return;
    m_needsBackdrop = needsBackdrop;
    markDirty(DirtyOpaque);
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
    child->collectCommittedVisible(m_pendingRemovedPresentDamage);
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

void Node::updateWorld(const QTransform &parentWorld, bool parentWorldChanged,
                       Region &worldDamage, Region &backdropDamage)
{
    m_structuralPresentDamage = m_pendingRemovedPresentDamage;
    m_pendingRemovedPresentDamage = {};
    m_ownDamage = m_pendingRemovedDamage;
    m_pendingRemovedDamage = {};

    const bool matrixChanged = parentWorldChanged
        || (m_type == Type::Transform && (m_dirty & DirtyMatrix));
    const bool stateDirty = matrixChanged || m_dirty != 0;

    if (stateDirty) {
        if (m_type == Type::Transform)
            m_worldTransform = parentWorld * static_cast<TransformNode *>(this)->m_matrix;
        else
            m_worldTransform = parentWorld;
    }

    if (Q_UNLIKELY(!m_visible)) {
        if (stateDirty && m_committedVisible)
            m_ownDamage += m_committedSubtreeAABB;
        if (stateDirty) {
            m_worldBounds = {};
            m_subtreeAABB = {};
            m_worldOpaque = {};
        }
        m_behindDamage = worldDamage;
        worldDamage += m_ownDamage;
        return;
    }

    if (stateDirty) {
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
            const QTransform c2w = contentToWorld(m_worldTransform, geo->m_boundingRect);
            m_worldOpaque = mapRegionInner(c2w, geo->m_opaqueRegion);

            if (appearing) {
                m_ownDamage += m_worldBounds;
            } else {
                if (shapeChanged) {
                    m_ownDamage += m_committedWorldBounds;
                    m_ownDamage += m_worldBounds;
                }
                if (m_dirty & DirtyContent) {
                    Region mapped = mapRegionOuter(c2w, geo->m_pendingContentDamage);
                    mapped &= m_worldBounds;
                    m_ownDamage += mapped;
                }
            }
            geo->m_pendingContentDamage = {};
        } else {
            m_worldBounds = {};
            m_worldOpaque = {};
            if (auto *geo = toGeometry())
                geo->m_pendingContentDamage = {};
        }
    }

    m_behindDamage = worldDamage;
    if (m_needsBackdrop && hasContent() && !m_worldBounds.isEmpty())
        backdropDamage += m_behindDamage & m_worldBounds;

    if (hasContent() && !m_worldOpaque.isEmpty())
        worldDamage -= m_worldOpaque;
    worldDamage += m_ownDamage;
    QRect subtree = hasContent() ? m_worldBounds : QRect();
    for (Node *child = m_firstChild; child; child = child->m_next) {
        child->updateWorld(m_worldTransform, matrixChanged, worldDamage, backdropDamage);
        if (stateDirty)
            subtree = subtree.united(child->m_subtreeAABB);
    }
    if (stateDirty)
        m_subtreeAABB = subtree;
}

void Node::clearBehindDamageRecursive()
{
    m_behindDamage = {};
    for (Node *child = m_firstChild; child; child = child->m_next)
        child->clearBehindDamageRecursive();
}

void Node::resetWorldVisibleRecursive()
{
    m_worldVisibleRegion = {};
    m_worldFrontOpaque = {};
    for (Node *child = m_firstChild; child; child = child->m_next)
        child->resetWorldVisibleRecursive();
}

void Node::collectCommittedVisible(Region &visible) const
{
    if (!m_committedVisible)
        return;
    visible += m_committedWorldVisibleRegion;
    for (const Node *child = m_firstChild; child; child = child->m_next)
        child->collectCommittedVisible(visible);
}

void Node::computeWorldVisibility(Region &worldFrontOpaque)
{
    if (Q_UNLIKELY(!m_visible)) {
        resetWorldVisibleRecursive();
        return;
    }

    for (Node *child = m_lastChild; child; child = child->m_prev)
        child->computeWorldVisibility(worldFrontOpaque);

    if (!hasContent()) {
        m_worldFrontOpaque = {};
        m_worldVisibleRegion = {};
        return;
    }

    m_worldFrontOpaque.setIntersection(worldFrontOpaque.native(), m_worldBounds);
    m_worldVisibleRegion = Region(m_worldBounds) - m_worldFrontOpaque;

    if (m_needsBackdrop) {
        if (!m_worldBounds.isEmpty())
            worldFrontOpaque -= m_worldBounds;
        return;
    }

    if (Q_UNLIKELY(!m_worldOpaque.isEmpty()))
        worldFrontOpaque += m_worldOpaque;
}

void Node::commitState()
{
    for (Node *child = m_firstChild; child; child = child->m_next)
        child->commitState();

    if (m_visible) {
        m_committedWorldBounds = hasContent() ? m_worldBounds : QRect();
        m_committedSubtreeAABB = m_subtreeAABB;
        m_committedWorldVisibleRegion = hasContent() ? m_worldVisibleRegion : Region();
    } else {
        m_committedWorldBounds = {};
        m_committedSubtreeAABB = {};
        m_committedWorldVisibleRegion = {};
    }
    m_committedVisible = m_visible;
    m_dirty = {};
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
    m_opaqueRegion = Region(innerAligned(QRectF(0, 0, m_boundingRect.width(),
                                                 m_boundingRect.height())));
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

void GeometryNode::setOpaqueRegion(const pixman_region32_t *localOpaque)
{
    const Region region(localOpaque);
    if (m_opaqueRegion == region && !m_fullyOpaque)
        return;
    m_fullyOpaque = false;
    m_opaqueRegion = region;
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

void GeometryNode::markContentDirty(const pixman_region32_t *localRegion)
{
    const Region region(localRegion);
    if (region.isEmpty())
        return;
    m_pendingContentDamage += region;
    markDirty(DirtyContent);
}

void GeometryNode::markContentDirty(const QRect &localRect)
{
    if (localRect.isEmpty())
        return;
    m_pendingContentDamage += localRect;
    markDirty(DirtyContent);
}


} // namespace Gdt
