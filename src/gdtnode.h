#ifndef GDTNODE_H
#define GDTNODE_H

#include "gdtregion.h"

#include <QFlags>
#include <QMatrix4x4>
#include <QRect>
#include <QRectF>
#include <QRegion>
#include <QString>
#include <QTransform>

namespace Gdt {

class Node;
class Tracker;
struct Viewport;
struct ViewportOcclusionState;
class TransformNode;
class GeometryNode;
class BackdropNode;
class RendererNode;
// Viewport-attached data owned by Viewport, linked onto Node.
struct NodeViewportData {
    QRegion visibleDamage;
    QRegion occludedRegion;
    bool fullyOccluded = false;
    bool culled = false;

    const Viewport *viewport = nullptr;
    Node *node = nullptr;
    NodeViewportData *nextOnNode = nullptr;
    NodeViewportData *prevOnNode = nullptr;
};
using NodeView = NodeViewportData;

// Context passed to RendererNode's custom damage calculation function.
struct RenderContext {
    const Viewport *viewport = nullptr;
    QRegion overallDamage;              // Damage accumulated before this node paints
    QTransform worldTransform;          // Node's transform in world space
    QTransform renderMatrix;            // Final composite matrix (worldToOutput * worldTransform)
    QRectF boundingRect;                // Node's local bounding rectangle
    QRect worldBounds;                  // Node's world bounding rectangle
    QRect outputBounds;                 // Node's output bounding rectangle in current viewport
    RendererNode *node = nullptr;       // Pointer to the node being evaluated
};

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
        Basic = 0,     // grouping only
        Transform,     // local 2D matrix applied to descendants
        Geometry,      // visible content
        Backdrop,      // visible; expands damage from content behind it
        Renderer       // visible; dynamically computes damage via custom function
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
    BackdropNode *toBackdrop();
    RendererNode *toRenderer();
    const TransformNode *toTransform() const;
    const GeometryNode *toGeometry() const;
    const BackdropNode *toBackdrop() const;
    const RendererNode *toRenderer() const;

    void setName(const QString &name) { m_name = name; }
    QString name() const { return m_name; }
    quint64 id() const { return m_id; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);

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
    // Per-viewport regions use output coordinates. The unqualified viewport
    // accessors mirror id 0 when present, otherwise the first viewport.
    QTransform worldTransform() const { return m_worldTransform; }
    QRect worldBounds() const { return m_worldBounds; }
    QRect subtreeBounds() const { return m_subtreeAABB; }
    QRegion worldOpaqueRegion() const { return m_worldOpaque; }
    QRegion ownDamage() const { return m_ownDamage; }
    QRegion inducedDamage() const { return m_inducedDamage; }
    // Per-viewport attached data accessors (O(1) direct pointer or short list)
    const NodeViewportData *viewportData(const Viewport *viewport = nullptr) const;
    NodeViewportData *viewportData(const Viewport *viewport = nullptr);

    QRegion visibleDamage(const Viewport *viewport = nullptr) const;
    QRegion occludedRegion(const Viewport *viewport = nullptr) const;
    bool isFullyOccluded(const Viewport *viewport = nullptr) const;
    bool isCulled(const Viewport *viewport = nullptr) const;
    DirtyBits dirty() const { return m_dirty; }
    bool isDirty() const { return m_dirty != 0; }

    QString dumpTree() const;

protected:
    void markDirty(DirtyBits bits);
    bool m_hasContent = false;
private:
    friend class Tracker;

    void attach(Node *child, Node *prev, Node *next);
    void unlink(Node *child);
    void adopt(Node *child);
    void clearFrameDamageRecursive();
    void updateWorld(const QTransform &parentWorld, bool parentWorldChanged);
    void collectBackdrop(QRegion &acc);
    void applyOcclusion(QRegion &frontOpaque, QRegion &remaining, QRegion &exposed,
                        QRegion &screen, const QTransform &worldToOutput,
                        const QRect &outputRect, const Viewport *viewport,
                        const std::function<NodeViewportData *(Node *)> &dataFactory);
    void applyOcclusionMulti(ViewportOcclusionState *states, int count,
                             const QRegion &worldDamage,
                             const std::function<NodeViewportData *(Node *, Viewport *)> &dataFactory);
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
    DirtyBits m_dirty = DirtyAdded;

    QTransform m_worldTransform;
    QRect m_worldBounds;
    QRect m_subtreeAABB;
    QRegion m_worldOpaque;
    QRegion m_ownDamage;
    QRegion m_inducedDamage;
    QRegion m_visibleDamage;
    QRegion m_occludedRegion;
    QRegion m_pendingRemovedDamage;
    bool m_fullyOccluded = false;
    bool m_culled = false;

    QRect m_committedWorldBounds;
    QRect m_committedSubtreeAABB;
    bool m_committedVisible = false;
    NodeViewportData *m_viewportDataHead = nullptr;

    friend struct Viewport;
    friend struct NodeViewportData;
    void attachViewportData(NodeViewportData *data);
    void detachViewportData(NodeViewportData *data);

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

    void setOpaqueRegion(const QRegion &localOpaque);
    QRegion opaqueRegion() const { return m_opaqueRegion; }

    // Marks every pixel inside the (inner-aligned) bounding rect as opaque.
    // Recomputed when the bounding rect changes.
    void setFullyOpaque(bool fullyOpaque);
    bool isFullyOpaque() const { return m_fullyOpaque; }

    // Local-space dirty pixels of this node's content. Clipped to bounding rect.
    void markContentDirty(const QRegion &localRegion);
    void markContentDirty(const QRect &localRect);

protected:
    explicit GeometryNode(Type type);

private:
    friend class Node;
    friend class Tracker;
    void syncFullyOpaqueRegion();

    QRectF m_boundingRect;
    QRegion m_opaqueRegion;
    QRegion m_pendingContentDamage;
    bool m_fullyOpaque = false;
};

// Displayable node that samples content behind it (blur, backdrop-filter,
// WRenderBufferNode-style buffer). Backdrop damage intersecting the sample
// area is dilated by backdropExpansion; clipping is optional.
class BackdropNode : public GeometryNode
{
public:
    BackdropNode();

    void setBackdropExpansion(const QMargins &margins);
    void setBackdropExpansion(int px);
    QMargins backdropExpansion() const { return m_expansion; }

    // When true (default), expanded damage is clipped to world bounds.
    // When false, expansion may bleed outside the node (e.g. a halo).
    void setClipExpansion(bool clip);
    bool clipExpansion() const { return m_clipExpansion; }

private:
    friend class Node;
    friend class Tracker;
    QMargins m_expansion;
    bool m_clipExpansion = true;
};

// Displayable node that executes a custom damage calculation function.
// When reached during damage propagation, it inspects the accumulated overall
// damage and render matrices to compute the damage region it will induce.
class RendererNode : public GeometryNode
{
public:
    using DamageFunction = std::function<QRegion(const RenderContext &context)>;

    RendererNode();

    void setDamageFunction(DamageFunction func);
    DamageFunction damageFunction() const { return m_damageFunc; }

private:
    friend class Node;
    friend class Tracker;
    DamageFunction m_damageFunc;
};

} // namespace Gdt

Q_DECLARE_OPERATORS_FOR_FLAGS(Gdt::Node::DirtyBits)

#endif // GDTNODE_H
