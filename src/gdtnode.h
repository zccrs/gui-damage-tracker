#ifndef GDTNODE_H
#define GDTNODE_H

#include "gdtregion.h"

#include <QFlags>
#include <QMatrix4x4>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QTransform>

namespace Gdt {

class Node;
class Tracker;
class Viewport;
class TransformNode;
class GeometryNode;

// Scene-graph node used for GUI damage precomputation.
//
// Tree shape matches QSG: first child is behind later siblings; a node's own
// geometry (if any) is behind its descendants.
//
// Parent owns children. Removing a child does not delete it. Destroying a
// parent deletes remaining children. Not thread-safe.
class Node
{
public:
    enum class Type {
        Basic = 0,
        Transform,
        Geometry,
    };
    enum DirtyBit {
        DirtyMatrix      = 1 << 0,
        DirtyGeometry    = 1 << 1,
        DirtyContent     = 1 << 2,
        DirtyAdded       = 1 << 3,
        DirtyStructure   = 1 << 4,
        DirtyVisibility  = 1 << 5,
        DirtyOpaque      = 1 << 6,
        DirtySubtree     = 1 << 7
    };
    Q_DECLARE_FLAGS(DirtyBits, DirtyBit)

    explicit Node(Type type = Type::Basic);
    virtual ~Node();

    Node(const Node &) = delete;
    Node &operator=(const Node &) = delete;

    Type type() const { return m_type; }
    bool hasContent() const { return m_hasContent; }
    void setHasContent(bool hasContent);

    TransformNode *toTransform();
    GeometryNode *toGeometry();
    const TransformNode *toTransform() const;
    const GeometryNode *toGeometry() const;

    void setName(const QString &name) { m_name = name; }
    QString name() const { return m_name; }
    quint64 id() const { return m_id; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);
    void setNeedsBackdrop(bool needsBackdrop);
    bool needsBackdrop() const { return m_needsBackdrop; }


    Node *parent() const { return m_parent; }
    Node *firstChild() const { return m_firstChild; }
    Node *lastChild() const { return m_lastChild; }
    Node *nextSibling() const { return m_next; }
    Node *previousSibling() const { return m_prev; }
    int childCount() const { return m_childCount; }
    int subtreeNodeCount() const { return m_subtreeNodeCount; }
    void prependChild(Node *child);
    void appendChild(Node *child);
    void insertChildBefore(Node *child, Node *before);
    void insertChildAfter(Node *child, Node *after);
    void removeChild(Node *child);
    void removeAllChildren(); // does not delete
    Node *takeChild(Node *child); // alias of removeChild, returns child

    // After Tracker::commit(). World fields are viewport-independent.
    QTransform worldTransform() const { return m_worldTransform; }
    QRect worldBounds() const { return m_worldBounds; }
    QRect subtreeBounds() const { return m_subtreeAABB; }
    const pixman_region32_t *worldOpaqueRegion() const { return m_worldOpaque.native(); }
    // Bounds minus front opaque. Backdrop punches this so sampled-behind
    // stays valid for cache; not "on-screen visible".
    const pixman_region32_t *worldValidRegion() const { return m_worldValidRegion.native(); }
    const pixman_region32_t *committedWorldValidRegion() const { return m_committedWorldValidRegion.native(); }
    const pixman_region32_t *worldFrontOpaqueRegion() const { return m_worldFrontOpaque.native(); }
    // Bounds minus front needsBackdrop coverage. On-screen draw of this node.
    // Clean nodes only redraw this; dirty nodes also draw the sampled part.
    const pixman_region32_t *worldVisibleRegion() const { return m_worldVisibleRegion.native(); }
    const pixman_region32_t *worldFrontBackdropRegion() const { return m_worldFrontBackdrop.native(); }
    const pixman_region32_t *behindDamageRegion() const { return m_behindDamage.native(); }

    // Dirty bits from setters, not "content dirty for this viewport frame".
    bool isDirty() const { return m_dirty != 0; }
    QString dumpTree() const;

protected:
    void markDirty(DirtyBits bits);
    bool m_hasContent = false;
    friend class Tracker;
    friend class NodeTestAccess;
    friend class GeometryNode;
    void attach(Node *child, Node *prev, Node *next);
    void unlink(Node *child);
    void adopt(Node *child);
    void updateWorld(const QTransform &parentWorld, bool parentWorldChanged,
                     Region &worldDamage, Region &backdropDamage);
    void clearBehindDamageRecursive();
    void computeWorldVisibility(Region &worldFrontOpaque, Region &worldFrontBackdrop);
    void resetWorldVisibleRecursive();
    void collectCommittedVisible(Region &visible) const;
    void commitState();
    void dumpTreeRecursive(QString &out, int depth) const;
    Type m_type;
    quint64 m_id = 0;
    QString m_name;

    Node *m_parent = nullptr;
    Node *m_firstChild = nullptr;
    Node *m_lastChild = nullptr;
    Node *m_prev = nullptr;
    Node *m_next = nullptr;
    int m_childCount = 0;
    int m_subtreeNodeCount = 1;

    bool m_visible = true;
    bool m_needsBackdrop = false;

    DirtyBits m_dirty = DirtyAdded;

    QTransform m_worldTransform;
    QRect m_worldBounds;
    QRect m_subtreeAABB;
    Region m_worldOpaque;
    Region m_ownDamage;
    Region m_behindDamage;
    Region m_structuralPresentDamage;
    Region m_pendingRemovedDamage;
    Region m_pendingRemovedPresentDamage;
    Region m_worldValidRegion;
    Region m_worldFrontOpaque;
    Region m_worldVisibleRegion;
    Region m_worldFrontBackdrop;
    Region m_committedWorldValidRegion;
    QRect m_committedWorldBounds;
    QRect m_committedSubtreeAABB;
    bool m_committedVisible = false;


    static quint64 s_nextId;
};

class TransformNode : public Node
{
public:
    TransformNode();

    void setMatrix(const QTransform &matrix);
    void setMatrix(const QMatrix4x4 &matrix);
    QTransform matrix() const { return m_matrix; }

    void setTranslation(qreal x, qreal y);
    void setScale(qreal sx, qreal sy);
    void setRotation(qreal degrees, Qt::Axis axis = Qt::ZAxis);

private:
    friend class Node;
    friend class Tracker;
    QTransform m_matrix;
};

class GeometryNode : public Node
{
public:
    GeometryNode();

    void setBoundingRect(const QRectF &rect);
    QRectF boundingRect() const { return m_boundingRect; }

    // Content-local opaque pixels. Origin is boundingRect top-left.
    void setOpaqueRegion(const pixman_region32_t *localOpaque);
    const pixman_region32_t *opaqueRegion() const { return m_opaqueRegion.native(); }

    // Marks every pixel inside the (inner-aligned) content box as opaque.
    // Recomputed when the bounding rect changes.
    void setFullyOpaque(bool fullyOpaque);
    bool isFullyOpaque() const { return m_fullyOpaque; }

    // Content-local dirty pixels. Origin is boundingRect top-left.
    // Clipped to the content box on commit.
    void markContentDirty(const pixman_region32_t *localRegion);
    void markContentDirty(const QRect &localRect);

protected:
    explicit GeometryNode(Type type);

private:
    friend class Node;
    friend class Tracker;
    void syncFullyOpaqueRegion();

    QRectF m_boundingRect;
    Region m_opaqueRegion;
    Region m_pendingContentDamage;
    bool m_fullyOpaque = false;
};



} // namespace Gdt

Q_DECLARE_OPERATORS_FOR_FLAGS(Gdt::Node::DirtyBits)

#endif // GDTNODE_H
