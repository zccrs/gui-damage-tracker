#include "gdt.h"

#include <QMatrix4x4>
#include <QTest>

using namespace Gdt;

static QString regionStr(const QRegion &r)
{
    if (r.isEmpty())
        return QStringLiteral("<empty>");
    QString s;
    for (const QRect &rect : r) {
        if (!s.isEmpty())
            s += QLatin1Char(' ');
        s += QStringLiteral("[%1,%2 %3x%4]")
                 .arg(rect.x())
                 .arg(rect.y())
                 .arg(rect.width())
                 .arg(rect.height());
    }
    return s;
}

#define COMPARE_REGION(actual, expected)                                                       \
    do {                                                                                       \
        const QRegion _gdtActual = (actual);                                                   \
        const QRegion _gdtExpected = (expected);                                               \
        QVERIFY2(_gdtActual == _gdtExpected,                                                   \
                 qPrintable(QStringLiteral("actual=%1 expected=%2")                            \
                                .arg(regionStr(_gdtActual), regionStr(_gdtExpected))));        \
    } while (0)

#define CONTAINS_REGION(actual, expected)                                                      \
    QVERIFY2(((actual) & (expected)) == (expected),                                            \
             qPrintable(QStringLiteral("actual=%1 does not contain %2")                        \
                            .arg(regionStr(actual), regionStr(expected))))

class tst_Gdt : public QObject
{
    Q_OBJECT

private slots:
    void emptyCommit();
    void addGeometry();
    void idempotentCommit();
    void removeGeometry();
    void addRemoveSameFrame();
    void moveGeometryRect();
    void resizeGeometry();
    void partialContentDamage();
    void contentDamageClippedToBounds();
    void hideShow();
    void hideShowSameFrame();
    void hideThenRemoveWithoutCommit();
    void translateTransform();
    void nestedTransforms();
    void scaleTransform();
    void rotate90();
    void rotate45Conservative();
    void siblingOcclusionFullyCovered();
    void siblingOcclusionPartial();
    void transparentDoesNotOcclude();
    void parentGeometryBehindChildren();
    void occludedRegionReported();
    void backdropNoExpansion();
    void backdropExpansion();
    void backdropSamplesOutsideBounds();
    void backdropOwnDamageDoesNotInduce();
    void backdropClipBleed();
    void nestedBackdrop();
    void insertBetweenSiblings();
    void reparent();
    void raiseLowersZOrder();
    void viewportClip();
    void outsideViewport();
    void viewportResizeDamagesStrip();
    void viewportTransformMapsDamage();
    void viewportTransformsCullIndependently();
    void viewportTransformMapsOcclusion();
    void viewportTransformChangeDamagesOutput();
    void fullyOpaqueFlag();
    void opaqueRegionUpdate();
    void multipleDirtyRegions();
    void destroySubtree();
    void basicNodeGrouping();
    void parentHideHidesChildren();
    void fractionalTranslationOverestimates();
    void moveOpaqueRevealsBehind();
    void backdropWithTransform();
    void contentDamageUnderOpaqueSibling();
    void partiallyOccludedMoveAvoidsOpaqueFront();
    void fullyOccludedMoveProducesNoDamage();
    void zeroSizeGeometry();
    void negativeCoordinates();
    void deepTree();
    void setMatrixIdentityNoDamage();
    void geometryChildrenZOrder();
    void nonAxisAlignedDropsOpaque();
    void matrix4x4();
    void firstCommitAppearing();
    void contentDirtyOnNewNode();
    void removeUncommittedNode();
    void siblingOrderPaint();
    void backdropDoesNotSeeFrontDamageAsRequired();
    void twoViewportsIndependentDamage();
    void twoViewportsIndependentOcclusion();
    void twoViewportsSingleCommitKeepsBoth();
    void twoViewportsIdempotent();
    void twoViewportsResizeOne();
    void twoViewportsDropped();
    void twoViewportsNewOutputFullDamage();
    void nodeAccessorsFollowPrimaryViewport();
    void rendererNodeBasicDamageFunction();
    void rendererNodeDynamicInducedDamage();
    void rendererNodeMatrixPropagation();
    void viewportCarryOwnDamageOnIdleTree();
    void viewportDamageUnionsWithTreeDamage();
    void multipleViewportsIndependentBufferDamage();
};

void tst_Gdt::emptyCommit()
{
    Node root;
    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void tst_Gdt::addGeometry()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(10, 20, 30, 40));
    root.appendChild(g);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(10, 20, 30, 40));
    QCOMPARE(g->worldBounds(), QRect(10, 20, 30, 40));
    QVERIFY(!g->isFullyOccluded());
}

void tst_Gdt::idempotentCommit()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 50, 50));
    root.appendChild(g);

    Tracker tracker(&root);
    QVERIFY(!tracker.commit().isEmpty());
    COMPARE_REGION(tracker.commit(), QRegion());
    COMPARE_REGION(tracker.commit(), QRegion());
}

void tst_Gdt::removeGeometry()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(5, 5, 10, 10));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    root.removeChild(g);
    COMPARE_REGION(tracker.commit(), QRegion(5, 5, 10, 10));
    delete g;
}

void tst_Gdt::addRemoveSameFrame()
{
    Node root;
    Tracker tracker(&root);
    tracker.commit();

    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 40, 40));
    root.appendChild(g);
    root.removeChild(g);
    delete g;

    COMPARE_REGION(tracker.commit(), QRegion());
}

void tst_Gdt::moveGeometryRect()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    g->setBoundingRect(QRectF(50, 50, 20, 20));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 20, 20) + QRegion(50, 50, 20, 20));
}

void tst_Gdt::resizeGeometry()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    g->setBoundingRect(QRectF(0, 0, 30, 10));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 30, 10));
}

void tst_Gdt::partialContentDamage()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 100, 100));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    g->markContentDirty(QRect(10, 15, 4, 5));
    COMPARE_REGION(tracker.commit(), QRegion(10, 15, 4, 5));
}

void tst_Gdt::contentDamageClippedToBounds()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    g->markContentDirty(QRect(-10, -10, 15, 15));
    const QRegion d = tracker.commit();
    QVERIFY((d - QRegion(0, 0, 20, 20)).isEmpty());
    CONTAINS_REGION(d, QRegion(0, 0, 5, 5));
}

void tst_Gdt::hideShow()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(8, 8, 16, 16));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    g->setVisible(false);
    COMPARE_REGION(tracker.commit(), QRegion(8, 8, 16, 16));
    QVERIFY(g->isCulled());
    QVERIFY(!g->isFullyOccluded());

    COMPARE_REGION(tracker.commit(), QRegion());

    g->setVisible(true);
    COMPARE_REGION(tracker.commit(), QRegion(8, 8, 16, 16));
}

void tst_Gdt::hideShowSameFrame()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 12, 12));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    g->setVisible(false);
    g->setVisible(true);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void tst_Gdt::hideThenRemoveWithoutCommit()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(1, 2, 3, 4));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    g->setVisible(false);
    root.removeChild(g);
    COMPARE_REGION(tracker.commit(), QRegion(1, 2, 3, 4));
    delete g;
}

void tst_Gdt::translateTransform()
{
    Node root;
    auto *tr = new TransformNode;
    tr->setTranslation(10, 20);
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 5, 6));
    tr->appendChild(g);
    root.appendChild(tr);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(10, 20, 5, 6));

    tr->setTranslation(40, 20);
    COMPARE_REGION(tracker.commit(), QRegion(10, 20, 5, 6) + QRegion(40, 20, 5, 6));
}

void tst_Gdt::nestedTransforms()
{
    Node root;
    auto *a = new TransformNode;
    a->setTranslation(10, 0);
    auto *b = new TransformNode;
    b->setTranslation(0, 20);
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(1, 2, 3, 4));
    b->appendChild(g);
    a->appendChild(b);
    root.appendChild(a);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(11, 22, 3, 4));
}

void tst_Gdt::scaleTransform()
{
    Node root;
    auto *tr = new TransformNode;
    tr->setScale(2, 3);
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(1, 1, 4, 2));
    tr->appendChild(g);
    root.appendChild(tr);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(2, 3, 8, 6));
}

void tst_Gdt::rotate90()
{
    Node root;
    auto *tr = new TransformNode;
    QTransform t;
    t.translate(50, 50);
    t.rotate(90);
    t.translate(-50, -50);
    tr->setMatrix(t);

    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(40, 40, 20, 10));
    g->setFullyOpaque(true);
    tr->appendChild(g);
    root.appendChild(tr);

    Tracker tracker(&root);
    const QRegion d = tracker.commit();
    QVERIFY(!d.isEmpty());
    QCOMPARE(d.boundingRect().width() > 0, true);
    QVERIFY(isAxisAligned(g->worldTransform()));
    QVERIFY(!g->worldOpaqueRegion().isEmpty());
}

void tst_Gdt::rotate45Conservative()
{
    Node root;
    auto *tr = new TransformNode;
    QTransform t;
    t.translate(100, 100);
    t.rotate(45);
    t.translate(-50, -50);
    tr->setMatrix(t);

    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 100, 100));
    g->setFullyOpaque(true);
    tr->appendChild(g);
    root.appendChild(tr);

    Tracker tracker(&root);
    const QRegion d = tracker.commit();
    const QRect aabb = g->worldBounds();
    QVERIFY(d.boundingRect().contains(aabb) || d == QRegion(aabb) || (d & aabb) == QRegion(aabb));
    CONTAINS_REGION(d, QRegion(aabb));
    // Must not claim opacity under a non-axis-aligned transform.
    QVERIFY(g->worldOpaqueRegion().isEmpty());
}

void tst_Gdt::siblingOcclusionFullyCovered()
{
    Node root;
    auto *back = new GeometryNode;
    back->setName(QStringLiteral("back"));
    back->setBoundingRect(QRectF(0, 0, 100, 100));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setName(QStringLiteral("front"));
    front->setBoundingRect(QRectF(0, 0, 100, 100));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit();

    QVERIFY(back->isFullyOccluded());
    QVERIFY(!front->isFullyOccluded());

    back->markContentDirty(QRect(10, 10, 8, 8));
    tracker.commit();
    QVERIFY(back->visibleDamage().isEmpty());
    QVERIFY(back->isFullyOccluded());
}

void tst_Gdt::siblingOcclusionPartial()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 100, 100));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(25, 25, 50, 50));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit();

    QVERIFY(!back->isFullyOccluded());
    QCOMPARE(back->occludedRegion(), QRegion(25, 25, 50, 50));

    back->markContentDirty(QRect(0, 0, 10, 10));
    const QRegion d = tracker.commit();
    CONTAINS_REGION(d, QRegion(0, 0, 10, 10));
    CONTAINS_REGION(back->visibleDamage(), QRegion(0, 0, 10, 10));

    back->markContentDirty(QRect(40, 40, 10, 10));
    tracker.commit();
    QVERIFY((back->visibleDamage() & QRegion(40, 40, 10, 10)).isEmpty());
}

void tst_Gdt::transparentDoesNotOcclude()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 40, 40));
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 40, 40));
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit();
    QVERIFY(!back->isFullyOccluded());

    back->markContentDirty(QRect(2, 2, 4, 4));
    tracker.commit();
    CONTAINS_REGION(back->visibleDamage(), QRegion(2, 2, 4, 4));
}

void tst_Gdt::parentGeometryBehindChildren()
{
    Node root;
    auto *parent = new GeometryNode;
    parent->setBoundingRect(QRectF(0, 0, 80, 80));
    parent->setFullyOpaque(true);
    auto *child = new GeometryNode;
    child->setBoundingRect(QRectF(0, 0, 80, 80));
    child->setFullyOpaque(true);
    parent->appendChild(child);
    root.appendChild(parent);

    Tracker tracker(&root);
    tracker.commit();
    QVERIFY(parent->isFullyOccluded());
    QVERIFY(!child->isFullyOccluded());
}

void tst_Gdt::occludedRegionReported()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 30, 30));
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(10, 0, 30, 30));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit();
    QCOMPARE(back->occludedRegion(), QRegion(10, 0, 20, 30));
    QVERIFY(!back->isFullyOccluded());
}

void tst_Gdt::backdropNoExpansion()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 100, 100));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(20, 20, 40, 40));
    bg->setBackdropExpansion(0);
    root.appendChild(behind);
    root.appendChild(bg);

    Tracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(25, 25, 2, 2));
    tracker.commit();
    CONTAINS_REGION(bg->inducedDamage(), QRegion(25, 25, 2, 2));
    CONTAINS_REGION(bg->visibleDamage(), QRegion(25, 25, 2, 2));
}

void tst_Gdt::backdropExpansion()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 200, 200));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(0, 0, 200, 200));
    bg->setBackdropExpansion(5);
    root.appendChild(behind);
    root.appendChild(bg);

    Tracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(50, 50, 1, 1));
    tracker.commit();
    CONTAINS_REGION(bg->inducedDamage(), QRegion(45, 45, 11, 11));
}

void tst_Gdt::backdropOwnDamageDoesNotInduce()
{
    Node root;
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(0, 0, 100, 100));
    bg->setBackdropExpansion(5);
    root.appendChild(bg);

    Tracker tracker(&root);
    tracker.commit();

    bg->markContentDirty(QRect(50, 50, 1, 1));
    COMPARE_REGION(tracker.commit(), QRegion(50, 50, 1, 1));
    COMPARE_REGION(bg->inducedDamage(), QRegion());
}

void tst_Gdt::backdropSamplesOutsideBounds()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 200, 200));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(50, 50, 20, 20));
    bg->setBackdropExpansion(5);
    bg->setClipExpansion(true);
    root.appendChild(behind);
    root.appendChild(bg);

    Tracker tracker(&root);
    tracker.commit();

    // 3px left of the backdrop node, inside the 5px sample margin.
    behind->markContentDirty(QRect(47, 55, 1, 1));
    tracker.commit();
    QVERIFY(!bg->inducedDamage().isEmpty());
    QVERIFY((bg->inducedDamage() - QRegion(50, 50, 20, 20)).isEmpty());
    QVERIFY(bg->inducedDamage().intersects(QRect(50, 50, 5, 20)));
}

void tst_Gdt::backdropClipBleed()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 200, 200));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(50, 50, 20, 20));
    bg->setBackdropExpansion(4);
    bg->setClipExpansion(false);
    root.appendChild(behind);
    root.appendChild(bg);

    Tracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(50, 50, 2, 2));
    tracker.commit();
    QVERIFY(bg->inducedDamage().intersects(QRect(46, 46, 4, 4)));
}

void tst_Gdt::nestedBackdrop()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 300, 300));
    auto *inner = new BackdropNode;
    inner->setBoundingRect(QRectF(0, 0, 300, 300));
    inner->setBackdropExpansion(2);
    auto *outer = new BackdropNode;
    outer->setBoundingRect(QRectF(0, 0, 300, 300));
    outer->setBackdropExpansion(3);
    root.appendChild(behind);
    root.appendChild(inner);
    root.appendChild(outer);

    Tracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(100, 100, 1, 1));
    tracker.commit();
    CONTAINS_REGION(inner->inducedDamage(), QRegion(98, 98, 5, 5));
    // Outer sees the already-expanded inner damage and expands again.
    CONTAINS_REGION(outer->inducedDamage(), QRegion(95, 95, 11, 11));
}

void tst_Gdt::insertBetweenSiblings()
{
    Node root;
    auto *a = new GeometryNode;
    a->setBoundingRect(QRectF(0, 0, 10, 10));
    auto *c = new GeometryNode;
    c->setBoundingRect(QRectF(20, 0, 10, 10));
    root.appendChild(a);
    root.appendChild(c);

    Tracker tracker(&root);
    tracker.commit();

    auto *b = new GeometryNode;
    b->setBoundingRect(QRectF(10, 0, 10, 10));
    root.insertChildBefore(b, c);
    QCOMPARE(a->nextSibling(), b);
    QCOMPARE(b->nextSibling(), c);
    COMPARE_REGION(tracker.commit(), QRegion(10, 0, 10, 10));
}

void tst_Gdt::reparent()
{
    Node root;
    auto *t1 = new TransformNode;
    t1->setTranslation(0, 0);
    auto *t2 = new TransformNode;
    t2->setTranslation(80, 0);
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    t1->appendChild(g);
    root.appendChild(t1);
    root.appendChild(t2);

    Tracker tracker(&root);
    tracker.commit();

    t2->appendChild(g);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 10, 10) + QRegion(80, 0, 10, 10));
}

void tst_Gdt::raiseLowersZOrder()
{
    Node root;
    auto *a = new GeometryNode;
    a->setBoundingRect(QRectF(0, 0, 40, 40));
    a->setFullyOpaque(true);
    auto *b = new GeometryNode;
    b->setBoundingRect(QRectF(0, 0, 40, 40));
    b->setFullyOpaque(true);
    root.appendChild(a);
    root.appendChild(b);

    Tracker tracker(&root);
    tracker.commit();
    QVERIFY(a->isFullyOccluded());
    QVERIFY(!b->isFullyOccluded());

    root.appendChild(a); // raise a
    tracker.commit();
    QVERIFY(b->isFullyOccluded());
    QVERIFY(!a->isFullyOccluded());
}

void tst_Gdt::viewportClip()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(-20, -20, 50, 50));
    root.appendChild(g);

    Tracker tracker(&root);
    const QRegion d = tracker.commit(QRect(0, 0, 100, 100));
    QVERIFY((d - QRegion(0, 0, 100, 100)).isEmpty());
    CONTAINS_REGION(d, QRegion(0, 0, 30, 30));
}

void tst_Gdt::outsideViewport()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(500, 500, 10, 10));
    root.appendChild(g);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(QRect(0, 0, 100, 100)), QRegion());
    QVERIFY(g->isCulled());
    QVERIFY(!g->isFullyOccluded());
}

void tst_Gdt::viewportResizeDamagesStrip()
{
    Node root;
    Node spacer;
    root.appendChild(&spacer);

    Tracker tracker(&root);
    tracker.commit(QRect(0, 0, 80, 80));
    const QRegion d = tracker.commit(QRect(0, 0, 80, 100));
    CONTAINS_REGION(d, QRegion(0, 80, 80, 20));
}

void tst_Gdt::viewportTransformMapsDamage()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(10, 5, 4, 3));
    root.appendChild(g);

    Tracker tracker(&root);
    const Tracker::Viewport viewport{
        0,
        QRect(0, 0, 100, 100),
        QTransform::fromScale(2, 2),
    };

    COMPARE_REGION(tracker.commit({viewport}).value(0), QRegion(20, 10, 8, 6));
    COMPARE_REGION(tracker.visibleDamage(0, g), QRegion(20, 10, 8, 6));

    g->markContentDirty(QRect(11, 6, 1, 1));
    COMPARE_REGION(tracker.commit({viewport}).value(0), QRegion(22, 12, 2, 2));
}

void tst_Gdt::viewportTransformsCullIndependently()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(100, 0, 20, 20));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit({
        {0, QRect(0, 0, 50, 50), QTransform()},
        {1, QRect(0, 0, 50, 50), QTransform::fromTranslate(-100, 0)},
    });

    QVERIFY(tracker.isCulled(0, g));
    QVERIFY(!tracker.isFullyOccluded(0, g));
    QVERIFY(!tracker.isCulled(1, g));
    QVERIFY(!tracker.isFullyOccluded(1, g));
    COMPARE_REGION(tracker.damage(0), QRegion());
    COMPARE_REGION(tracker.damage(1), QRegion(0, 0, 20, 20));
}

void tst_Gdt::viewportTransformMapsOcclusion()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(10, 10, 10, 10));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(10, 10, 10, 10));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit({
        {0, QRect(0, 0, 100, 100), QTransform::fromScale(2, 2)},
    });

    QVERIFY(tracker.isFullyOccluded(0, back));
    QVERIFY(tracker.isCulled(0, back));
    QVERIFY(!tracker.isCulled(0, front));
    COMPARE_REGION(tracker.occludedRegion(0, back), QRegion(20, 20, 20, 20));
}

void tst_Gdt::viewportTransformChangeDamagesOutput()
{
    Node root;
    Node spacer;
    root.appendChild(&spacer);

    Tracker tracker(&root);
    tracker.commit({
        {0, QRect(0, 0, 80, 60), QTransform()},
    });
    tracker.commit({
        {0, QRect(0, 0, 80, 60), QTransform::fromTranslate(-20, 0)},
    });

    COMPARE_REGION(tracker.damage(0), QRegion(0, 0, 80, 60));
}

void tst_Gdt::fullyOpaqueFlag()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 16, 16));
    g->setFullyOpaque(true);
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();
    QCOMPARE(g->worldOpaqueRegion(), QRegion(0, 0, 16, 16));

    g->setBoundingRect(QRectF(0, 0, 32, 16));
    tracker.commit();
    QCOMPARE(g->worldOpaqueRegion(), QRegion(0, 0, 32, 16));
}

void tst_Gdt::opaqueRegionUpdate()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 40, 40));
    g->setOpaqueRegion(QRegion(0, 0, 10, 40));
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 40, 40));
    root.appendChild(back);
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();
    QCOMPARE(back->occludedRegion(), QRegion(0, 0, 10, 40));
}

void tst_Gdt::multipleDirtyRegions()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 200, 200));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    g->markContentDirty(QRect(0, 0, 2, 2));
    g->markContentDirty(QRect(50, 50, 3, 3));
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 2, 2) + QRegion(50, 50, 3, 3));
}

void tst_Gdt::destroySubtree()
{
    Node root;
    auto *tr = new TransformNode;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(3, 4, 5, 6));
    tr->appendChild(g);
    root.appendChild(tr);

    Tracker tracker(&root);
    tracker.commit();

    delete tr;
    COMPARE_REGION(tracker.commit(), QRegion(3, 4, 5, 6));
}

void tst_Gdt::basicNodeGrouping()
{
    Node root;
    auto *group = new Node;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(7, 8, 9, 10));
    group->appendChild(g);
    root.appendChild(group);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(7, 8, 9, 10));
}

void tst_Gdt::parentHideHidesChildren()
{
    Node root;
    auto *group = new Node;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    group->appendChild(g);
    root.appendChild(group);

    Tracker tracker(&root);
    tracker.commit();

    group->setVisible(false);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 20, 20));
}

void tst_Gdt::fractionalTranslationOverestimates()
{
    Node root;
    auto *tr = new TransformNode;
    QTransform t;
    t.translate(0.4, 0.4);
    tr->setMatrix(t);
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    tr->appendChild(g);
    root.appendChild(tr);

    Tracker tracker(&root);
    const QRegion d = tracker.commit();
    CONTAINS_REGION(d, QRegion(0, 0, 10, 10));
    QVERIFY(d.rectCount() >= 1);
    QVERIFY(d.boundingRect().width() >= 10);
}

void tst_Gdt::moveOpaqueRevealsBehind()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 80, 80));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 80, 80));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit();
    QVERIFY(back->isFullyOccluded());

    front->setBoundingRect(QRectF(100, 0, 80, 80));
    const QRegion d = tracker.commit();
    CONTAINS_REGION(d, QRegion(0, 0, 80, 80));
    CONTAINS_REGION(d, QRegion(100, 0, 80, 80));
    CONTAINS_REGION(back->visibleDamage(), QRegion(0, 0, 80, 80));
}

void tst_Gdt::backdropWithTransform()
{
    Node root;
    auto *tr = new TransformNode;
    tr->setTranslation(30, 40);
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 50, 50));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(0, 0, 50, 50));
    bg->setBackdropExpansion(2);
    tr->appendChild(behind);
    tr->appendChild(bg);
    root.appendChild(tr);

    Tracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(10, 10, 1, 1));
    tracker.commit();
    CONTAINS_REGION(bg->inducedDamage(), QRegion(38, 48, 5, 5));
}

void tst_Gdt::contentDamageUnderOpaqueSibling()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 60, 60));
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(20, 20, 20, 20));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit();

    back->markContentDirty(QRect(22, 22, 4, 4));
    tracker.commit();
    QVERIFY((back->visibleDamage() & QRegion(22, 22, 4, 4)).isEmpty());
}

void tst_Gdt::fullyOccludedMoveProducesNoDamage()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(20, 20, 20, 20));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 100, 100));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit();
    QVERIFY(back->isFullyOccluded());

    back->setBoundingRect(QRectF(60, 20, 20, 20));
    COMPARE_REGION(tracker.commit(), QRegion());
    QVERIFY(back->isFullyOccluded());
}

void tst_Gdt::partiallyOccludedMoveAvoidsOpaqueFront()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(80, 80, 260, 200));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(160, 140, 220, 180));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit();

    back->setBoundingRect(QRectF(100, 100, 260, 200));
    const QRegion damage = tracker.commit();
    QVERIFY((damage & QRegion(front->worldBounds())).isEmpty());
}

void tst_Gdt::zeroSizeGeometry()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(10, 10, 0, 10));
    root.appendChild(g);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void tst_Gdt::negativeCoordinates()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(-30, -10, 20, 20));
    root.appendChild(g);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(-30, -10, 20, 20));
}

void tst_Gdt::deepTree()
{
    Node root;
    Node *cur = &root;
    for (int i = 0; i < 32; ++i) {
        auto *t = new TransformNode;
        t->setTranslation(1, 0);
        cur->appendChild(t);
        cur = t;
    }
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 2, 2));
    cur->appendChild(g);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(32, 0, 2, 2));
}

void tst_Gdt::setMatrixIdentityNoDamage()
{
    Node root;
    auto *tr = new TransformNode;
    tr->setTranslation(0, 0);
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 8, 8));
    tr->appendChild(g);
    root.appendChild(tr);

    Tracker tracker(&root);
    tracker.commit();

    tr->setTranslation(0, 0);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void tst_Gdt::geometryChildrenZOrder()
{
    Node root;
    auto *parent = new GeometryNode;
    parent->setBoundingRect(QRectF(0, 0, 50, 50));
    auto *child = new GeometryNode;
    child->setBoundingRect(QRectF(10, 10, 10, 10));
    child->setFullyOpaque(true);
    parent->appendChild(child);
    root.appendChild(parent);

    Tracker tracker(&root);
    tracker.commit();

    parent->markContentDirty(QRect(10, 10, 10, 10));
    tracker.commit();
    QVERIFY((parent->visibleDamage() & QRegion(10, 10, 10, 10)).isEmpty());
}

void tst_Gdt::nonAxisAlignedDropsOpaque()
{
    Node root;
    auto *tr = new TransformNode;
    QTransform t;
    t.rotate(33);
    tr->setMatrix(t);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 80, 80));
    front->setFullyOpaque(true);
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 80, 80));
    tr->appendChild(back);
    tr->appendChild(front);
    root.appendChild(tr);

    Tracker tracker(&root);
    tracker.commit();
    QVERIFY(front->worldOpaqueRegion().isEmpty());
    QVERIFY(!back->isFullyOccluded());
}

void tst_Gdt::matrix4x4()
{
    Node root;
    auto *tr = new TransformNode;
    QMatrix4x4 m;
    m.translate(15, 25);
    tr->setMatrix(m);
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 4, 4));
    tr->appendChild(g);
    root.appendChild(tr);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(15, 25, 4, 4));
}

void tst_Gdt::firstCommitAppearing()
{
    Node root;
    auto *g1 = new GeometryNode;
    g1->setBoundingRect(QRectF(0, 0, 5, 5));
    auto *g2 = new GeometryNode;
    g2->setBoundingRect(QRectF(10, 0, 5, 5));
    root.appendChild(g1);
    root.appendChild(g2);

    Tracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 5, 5) + QRegion(10, 0, 5, 5));
}

void tst_Gdt::contentDirtyOnNewNode()
{
    Node root;
    Tracker tracker(&root);
    tracker.commit();

    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    g->markContentDirty(QRect(1, 1, 1, 1));
    root.appendChild(g);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 20, 20));
}

void tst_Gdt::removeUncommittedNode()
{
    Node root;
    auto *keep = new GeometryNode;
    keep->setBoundingRect(QRectF(0, 0, 5, 5));
    root.appendChild(keep);

    Tracker tracker(&root);
    tracker.commit();

    auto *tmp = new GeometryNode;
    tmp->setBoundingRect(QRectF(100, 100, 5, 5));
    root.appendChild(tmp);
    root.removeChild(tmp);
    delete tmp;
    COMPARE_REGION(tracker.commit(), QRegion());
}

void tst_Gdt::siblingOrderPaint()
{
    Node root;
    auto *first = new GeometryNode;
    first->setBoundingRect(QRectF(0, 0, 10, 10));
    first->setFullyOpaque(true);
    auto *second = new GeometryNode;
    second->setBoundingRect(QRectF(0, 0, 10, 10));
    second->setFullyOpaque(true);
    root.prependChild(second);
    root.prependChild(first);
    QCOMPARE(root.firstChild(), first);

    Tracker tracker(&root);
    tracker.commit();
    QVERIFY(first->isFullyOccluded());
    QVERIFY(!second->isFullyOccluded());
}

void tst_Gdt::backdropDoesNotSeeFrontDamageAsRequired()
{
    // A change strictly in front of a backdrop must not be required to expand
    // the backdrop; expansion is allowed (over-damage) but we verify that a
    // change *behind* always expands.
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 80, 80));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(0, 0, 80, 80));
    bg->setBackdropExpansion(6);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 80, 80));
    root.appendChild(behind);
    root.appendChild(bg);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(20, 20, 2, 2));
    tracker.commit();
    CONTAINS_REGION(bg->inducedDamage(), QRegion(14, 14, 14, 14));
}

void tst_Gdt::twoViewportsIndependentDamage()
{
    Node root;
    auto *left = new GeometryNode;
    left->setBoundingRect(QRectF(0, 0, 40, 40));
    auto *right = new GeometryNode;
    right->setBoundingRect(QRectF(100, 0, 40, 40));
    root.appendChild(left);
    root.appendChild(right);

    Tracker tracker(&root);
    const QVector<Tracker::Viewport> vps{
        {0, QRect(0, 0, 80, 80)},
        {1, QRect(80, 0, 80, 80)},
    };

    tracker.commit(vps);
    COMPARE_REGION(tracker.damage(0), QRegion(0, 0, 40, 40));
    COMPARE_REGION(tracker.damage(1), QRegion(100, 0, 40, 40));

    left->markContentDirty(QRect(1, 1, 2, 2));
    tracker.commit(vps);
    COMPARE_REGION(tracker.damage(0), QRegion(1, 1, 2, 2));
    COMPARE_REGION(tracker.damage(1), QRegion());
}

void tst_Gdt::twoViewportsIndependentOcclusion()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 200, 80));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 80, 80));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    Tracker tracker(&root);
    tracker.commit({
        {0, QRect(0, 0, 80, 80)},
        {1, QRect(80, 0, 120, 80)},
    });

    QVERIFY(tracker.isFullyOccluded(0, back));
    QVERIFY(!tracker.isFullyOccluded(1, back));
    QVERIFY(!tracker.isFullyOccluded(0, front));
}

void tst_Gdt::twoViewportsSingleCommitKeepsBoth()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(50, 0, 60, 20));
    root.appendChild(g);

    Tracker tracker(&root);
    const auto results = tracker.commit({
        {0, QRect(0, 0, 80, 40)},
        {1, QRect(80, 0, 80, 40)},
    });

    CONTAINS_REGION(results.value(0), QRegion(50, 0, 30, 20));
    CONTAINS_REGION(results.value(1), QRegion(80, 0, 30, 20));
    QCOMPARE(tracker.lastDamage(), tracker.damage(0));
}

void tst_Gdt::twoViewportsIdempotent()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    Tracker tracker(&root);
    const QVector<Tracker::Viewport> vps{
        {0, QRect(0, 0, 50, 50)},
        {1, QRect(50, 0, 50, 50)},
    };
    QVERIFY(!tracker.commit(vps).value(0).isEmpty());
    QCOMPARE(tracker.commit(vps).value(0), QRegion());
    COMPARE_REGION(tracker.damage(1), QRegion());
}

void tst_Gdt::twoViewportsResizeOne()
{
    Node root;
    Node spacer;
    root.appendChild(&spacer);

    Tracker tracker(&root);
    tracker.commit({
        {0, QRect(0, 0, 40, 40)},
        {1, QRect(40, 0, 40, 40)},
    });
    tracker.commit({
        {0, QRect(0, 0, 40, 40)},
        {1, QRect(40, 0, 40, 60)},
    });
    COMPARE_REGION(tracker.damage(0), QRegion());
    CONTAINS_REGION(tracker.damage(1), QRegion(40, 40, 40, 20));
}

void tst_Gdt::twoViewportsDropped()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit({
        {0, QRect(0, 0, 20, 20)},
        {1, QRect(100, 100, 20, 20)},
    });
    tracker.commit({{0, QRect(0, 0, 20, 20)}});
    COMPARE_REGION(tracker.damage(1), QRegion());
}

void tst_Gdt::twoViewportsNewOutputFullDamage()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit({{0, QRect(0, 0, 40, 40)}});
    tracker.commit({
        {0, QRect(0, 0, 40, 40)},
        {1, QRect(40, 0, 40, 40)},
    });
    COMPARE_REGION(tracker.damage(0), QRegion());
    COMPARE_REGION(tracker.damage(1), QRegion(40, 0, 40, 40));
}

void tst_Gdt::nodeAccessorsFollowPrimaryViewport()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit({
        {0, QRect(50, 50, 10, 10)},
        {1, QRect(0, 0, 20, 20)},
    });

    QVERIFY(g->isCulled());
    QVERIFY(!g->isFullyOccluded());
    QVERIFY(tracker.isCulled(0, g));
    QVERIFY(!tracker.isFullyOccluded(0, g));
    QVERIFY(!tracker.isCulled(1, g));
    QVERIFY(!tracker.isFullyOccluded(1, g));
    CONTAINS_REGION(tracker.visibleDamage(1, g), QRegion(0, 0, 10, 10));
}

void tst_Gdt::rendererNodeBasicDamageFunction()
{
    Node root;
    auto *rnd = new RendererNode;
    rnd->setBoundingRect(QRectF(10, 20, 100, 80));

    bool called = false;
    QRectF capturedBoundingRect;
    QRect capturedWorldBounds;
    rnd->setDamageFunction([&](const RenderContext &ctx) -> QRegion {
        called = true;
        capturedBoundingRect = ctx.boundingRect;
        capturedWorldBounds = ctx.worldBounds;
        // Return extra 10px margin around worldBounds as custom damage
        return ctx.worldBounds.adjusted(-10, -10, 10, 10);
    });

    root.appendChild(rnd);
    Tracker tracker(&root);

    // First commit: node appearing (ownDamage: 10,20,100,80) + inducedDamage: 0,10,120,100
    const QRegion damage = tracker.commit();
    QVERIFY(called);
    QCOMPARE(capturedBoundingRect, QRectF(10, 20, 100, 80));
    QCOMPARE(capturedWorldBounds, QRect(10, 20, 100, 80));
    COMPARE_REGION(damage, QRegion(0, 10, 120, 100));
    COMPARE_REGION(rnd->inducedDamage(), QRegion(0, 10, 120, 100));
}
void tst_Gdt::rendererNodeDynamicInducedDamage()
{
    Node root;
    auto *bg = new GeometryNode;
    bg->setBoundingRect(QRectF(0, 0, 50, 50));
    root.appendChild(bg);

    auto *rnd = new RendererNode;
    rnd->setBoundingRect(QRectF(20, 20, 100, 100));

    QRegion receivedOverall;
    rnd->setDamageFunction([&receivedOverall](const RenderContext &ctx) -> QRegion {
        receivedOverall = ctx.overallDamage;
        if (ctx.overallDamage.intersects(ctx.worldBounds)) {
            // If background changed inside our bounds, induce full node repaint
            return ctx.worldBounds;
        }
        return {};
    });
    root.appendChild(rnd);

    Tracker tracker(&root);
    tracker.commit(); // initial commit

    // Now bg changes content in intersecting area (30,30,10,10)
    bg->markContentDirty(QRect(30, 30, 10, 10));
    const QRegion damage = tracker.commit();

    // rnd should have seen the background damage in its overallDamage context
    CONTAINS_REGION(receivedOverall, QRegion(30, 30, 10, 10));
    // And rnd should have induced its full 100x100 bounds
    CONTAINS_REGION(damage, QRegion(20, 20, 100, 100));
}

void tst_Gdt::rendererNodeMatrixPropagation()
{
    Node root;
    auto *trans = new TransformNode;
    QTransform t;
    t.translate(50, 30);
    t.scale(2.0, 2.0);
    trans->setMatrix(t);
    root.appendChild(trans);

    auto *rnd = new RendererNode;
    rnd->setBoundingRect(QRectF(10, 10, 20, 20));

    QTransform capturedWorldTransform;
    QTransform capturedRenderMatrix;
    rnd->setDamageFunction([&](const RenderContext &ctx) -> QRegion {
        capturedWorldTransform = ctx.worldTransform;
        capturedRenderMatrix = ctx.renderMatrix;
        return ctx.worldBounds;
    });
    trans->appendChild(rnd);

    Tracker tracker(&root);
    tracker.commit();

    // local (10,10,20,20) * scale(2,2) + translate(50,30) => (70, 50, 40, 40)
    QCOMPARE(rnd->worldBounds(), QRect(70, 50, 40, 40));
    QCOMPARE(capturedWorldTransform.dx(), 50.0);
    QCOMPARE(capturedWorldTransform.dy(), 30.0);
    QCOMPARE(capturedWorldTransform.m11(), 2.0);
    QCOMPARE(capturedWorldTransform.m22(), 2.0);
}

void tst_Gdt::viewportCarryOwnDamageOnIdleTree()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(10, 10, 100, 100));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit(); // initial commit

    // Idle tree (no changes), but viewport has swapchain/buffer damage (e.g. from buffer age)
    Tracker::Viewport vp;
    vp.id = 0;
    vp.outputRect = QRect(0, 0, 500, 500);
    vp.damage = QRegion(50, 50, 40, 40);

    const auto result = tracker.commit({vp});
    COMPARE_REGION(result.value(0), QRegion(50, 50, 40, 40));
    COMPARE_REGION(tracker.damage(0), QRegion(50, 50, 40, 40));
}

void tst_Gdt::viewportDamageUnionsWithTreeDamage()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 200, 200));
    root.appendChild(g);

    Tracker tracker(&root);
    tracker.commit();

    // Tree has content damage in (10, 10, 30, 30), and viewport carries buffer damage in (100, 100, 50, 50)
    g->markContentDirty(QRect(10, 10, 30, 30));

    Tracker::Viewport vp;
    vp.id = 0;
    vp.outputRect = QRect(0, 0, 400, 400);
    vp.damage = QRegion(100, 100, 50, 50);

    const auto result = tracker.commit({vp});
    const QRegion expected = QRegion(10, 10, 30, 30) + QRegion(100, 100, 50, 50);
    COMPARE_REGION(result.value(0), expected);
}

void tst_Gdt::multipleViewportsIndependentBufferDamage()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 400, 400));
    root.appendChild(g);
    Tracker tracker(&root);

    Tracker::Viewport initVp0;
    initVp0.id = 0;
    initVp0.outputRect = QRect(0, 0, 800, 600);

    Tracker::Viewport initVp1;
    initVp1.id = 1;
    initVp1.outputRect = QRect(0, 0, 800, 600);

    tracker.commit({initVp0, initVp1}); // initial commit registers both viewports

    // Next frame: dual monitor outputs with independent swapchain buffer damage
    Tracker::Viewport vp0;
    vp0.id = 0;
    vp0.outputRect = QRect(0, 0, 800, 600);
    vp0.damage = QRegion(20, 20, 30, 30);

    Tracker::Viewport vp1;
    vp1.id = 1;
    vp1.outputRect = QRect(0, 0, 800, 600);
    vp1.damage = QRegion(60, 60, 40, 40);

    const auto result = tracker.commit({vp0, vp1});
    COMPARE_REGION(result.value(0), QRegion(20, 20, 30, 30));
    COMPARE_REGION(result.value(1), QRegion(60, 60, 40, 40));
}
QTEST_MAIN(tst_Gdt)
#include "tst_gdt.moc"
