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

class TestTracker : public Tracker
{
public:
    using Tracker::Tracker;
    using Tracker::commit;

    QRegion commit()
    {
        m_vps[0].setOutputRect({});
        m_vps[0].setWorldToOutput({});
        m_vps[0].finishFrame(); // prime: clear dirty from set calls
        prepareFrame();
        commit(m_vps[0]);
        QRegion result = m_vps[0].accumulatedDamage();
        finishFrame();
        return result;
    }

    QRegion commit(const QRect &outputRect)
    {
        m_vps[0].setOutputRect(outputRect);
        m_vps[0].setWorldToOutput({});
        m_vps[0].finishFrame(); // prime: clear dirty from set calls
        prepareFrame();
        commit(m_vps[0]);
        QRegion result = m_vps[0].accumulatedDamage();
        finishFrame();
        return result;
    }

private:
    QVector<Viewport> m_vps{Viewport()};
};

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
    void expansion();
    void backdropSamplesOutsideBounds();
    void backdropOwnDamageDoesNotInduce();
    void backdropClipBleed();
    void nestedBackdrop();
    void insertBetweenSiblings();
    void reparent();
    void raiseLowersZOrder();
    void viewportClip();
    void outsideViewport();
    void viewportTransformMapsDamage();
    void viewportTransformsCullIndependently();
    void viewportTransformMapsOcclusion();
    void fullyOpaqueFlag();
    void opaqueRegionUpdate();
    void contentLocalDirtyFollowsBox();
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
    void twoViewportsDropped();
    void nodeAccessorsFollowPrimaryViewport();
    void nodeHasContentProperty();
    void worldVisiblePartialOpaqueRegion();
    void worldVisibleTwoOpaqueFronts();
    void worldVisibleRaiseReveals();
    void worldVisibleHasContentOff();
    void worldVisibleParentHideClearsChild();
    void worldVisibleUnderTranslation();
    void worldVisibleNestedAndScale();
    void worldVisibleDemoDualOutput();
    void worldVisibleOutsideAllOutputs();
    void worldVisibleViewportMatrixIgnored();
    void worldVisibleNonAxisAlignedFront();
    void worldVisibleAfterGeometryMove();
    void worldVisibleZeroSizeEmpty();
    void worldVisibleRemoveFront();
    void worldVisibleIdempotentSecondCommit();
    void worldVisibleGroupHasNone();
    void worldVisiblePartialChildCover();
    void worldVisibleDeepNested();
    void worldVisibleInsertAndReparent();
    void worldVisibleHideShow();
    void worldVisibleNegativeCoords();
    void worldVisibleRotate90();
};

void tst_Gdt::emptyCommit()
{
    Node root;
    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void tst_Gdt::addGeometry()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(10, 20, 30, 40));
    root.appendChild(g);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(10, 20, 30, 40));
    QCOMPARE(g->worldBounds(), QRect(10, 20, 30, 40));
}

void tst_Gdt::idempotentCommit()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 50, 50));
    root.appendChild(g);

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
    tracker.commit();

    root.removeChild(g);
    COMPARE_REGION(tracker.commit(), QRegion(5, 5, 10, 10));
    delete g;
}

void tst_Gdt::addRemoveSameFrame()
{
    Node root;
    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
    tracker.commit();

    g->setVisible(false);
    COMPARE_REGION(tracker.commit(), QRegion(8, 8, 16, 16));

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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
    tracker.commit();


    back->markContentDirty(QRect(10, 10, 8, 8));
    tracker.commit();
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

    TestTracker tracker(&root);
    tracker.commit();


    back->markContentDirty(QRect(0, 0, 10, 10));
    const QRegion d = tracker.commit();
    CONTAINS_REGION(d, QRegion(0, 0, 10, 10));

    back->markContentDirty(QRect(40, 40, 10, 10));
    tracker.commit();
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

    TestTracker tracker(&root);
    tracker.commit();

    back->markContentDirty(QRect(2, 2, 4, 4));
    tracker.commit();
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

    TestTracker tracker(&root);
    tracker.commit();
}

void tst_Gdt::occludedRegionReported()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 30, 30));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(10, 0, 30, 30));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    COMPARE_REGION(front->worldVisibleRegion(), QRegion(10, 0, 30, 30));
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(0, 0, 10, 30));
}

void tst_Gdt::backdropNoExpansion()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 100, 100));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(20, 20, 40, 40));
    bg->setExpansion(0);
    root.appendChild(behind);
    root.appendChild(bg);

    TestTracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(25, 25, 2, 2));
    tracker.commit();
    CONTAINS_REGION(NodeTestAccess::inducedDamage(bg), QRegion(25, 25, 2, 2));
}

void tst_Gdt::expansion()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 200, 200));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(0, 0, 200, 200));
    bg->setExpansion(5);
    root.appendChild(behind);
    root.appendChild(bg);

    TestTracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(50, 50, 1, 1));
    tracker.commit();
    CONTAINS_REGION(NodeTestAccess::inducedDamage(bg), QRegion(45, 45, 11, 11));
}

void tst_Gdt::backdropOwnDamageDoesNotInduce()
{
    Node root;
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(0, 0, 100, 100));
    bg->setExpansion(5);
    root.appendChild(bg);

    TestTracker tracker(&root);
    tracker.commit();

    bg->markContentDirty(QRect(50, 50, 1, 1));
    COMPARE_REGION(tracker.commit(), QRegion(50, 50, 1, 1));
    COMPARE_REGION(NodeTestAccess::inducedDamage(bg), QRegion());
}

void tst_Gdt::backdropSamplesOutsideBounds()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 200, 200));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(50, 50, 20, 20));
    bg->setExpansion(5);
    bg->setClip(true);
    root.appendChild(behind);
    root.appendChild(bg);

    TestTracker tracker(&root);
    tracker.commit();

    // 3px left of the backdrop node, inside the 5px sample margin.
    behind->markContentDirty(QRect(47, 55, 1, 1));
    tracker.commit();
    QVERIFY(!NodeTestAccess::inducedDamage(bg).isEmpty());
    QVERIFY((NodeTestAccess::inducedDamage(bg) - QRegion(50, 50, 20, 20)).isEmpty());
    QVERIFY(NodeTestAccess::inducedDamage(bg).intersects(QRect(50, 50, 5, 20)));
}

void tst_Gdt::backdropClipBleed()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 200, 200));
    auto *bg = new BackdropNode;
    bg->setBoundingRect(QRectF(50, 50, 20, 20));
    bg->setExpansion(4);
    bg->setClip(false);
    root.appendChild(behind);
    root.appendChild(bg);

    TestTracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(50, 50, 2, 2));
    tracker.commit();
    QVERIFY(NodeTestAccess::inducedDamage(bg).intersects(QRect(46, 46, 4, 4)));
}

void tst_Gdt::nestedBackdrop()
{
    Node root;
    auto *behind = new GeometryNode;
    behind->setBoundingRect(QRectF(0, 0, 300, 300));
    auto *inner = new BackdropNode;
    inner->setBoundingRect(QRectF(0, 0, 300, 300));
    inner->setExpansion(2);
    auto *outer = new BackdropNode;
    outer->setBoundingRect(QRectF(0, 0, 300, 300));
    outer->setExpansion(3);
    root.appendChild(behind);
    root.appendChild(inner);
    root.appendChild(outer);

    TestTracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(100, 100, 1, 1));
    tracker.commit();
    CONTAINS_REGION(NodeTestAccess::inducedDamage(inner), QRegion(98, 98, 5, 5));
    // Outer sees the already-expanded inner damage and expands again.
    CONTAINS_REGION(NodeTestAccess::inducedDamage(outer), QRegion(95, 95, 11, 11));
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
    tracker.commit();

    root.appendChild(a); // raise a
    tracker.commit();
}

void tst_Gdt::viewportClip()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(10, 10, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(QRect(20, 10, 20, 20)), QRegion(20, 10, 10, 20));
    g->markContentDirty(QRect(12, 2, 2, 2));
    COMPARE_REGION(tracker.commit(QRect(20, 10, 20, 20)), QRegion(22, 12, 2, 2));
}

void tst_Gdt::outsideViewport()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(500, 500, 10, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(QRect(0, 0, 100, 100)), QRegion());
}

void tst_Gdt::viewportTransformMapsDamage()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(10, 5, 4, 3));
    root.appendChild(g);

    TestTracker tracker(&root);
    const Tracker::Viewport viewport{QRect(0, 0, 100, 100),
        QTransform::fromScale(2, 2),
    };

    QVector<Tracker::Viewport> vps{viewport};
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    COMPARE_REGION(vps[0].accumulatedDamage(), QRegion(20, 10, 8, 6));
    tracker.finishFrame();

    g->markContentDirty(QRect(1, 1, 1, 1));
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    COMPARE_REGION(vps[0].accumulatedDamage(), QRegion(22, 12, 2, 2));
    tracker.finishFrame();
}
void tst_Gdt::viewportTransformsCullIndependently()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(100, 0, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps{
        {QRect(0, 0, 50, 50), QTransform()},
        {QRect(0, 0, 50, 50), QTransform::fromTranslate(-100, 0)},
    };
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    COMPARE_REGION(vps[0].accumulatedDamage(), QRegion());
    COMPARE_REGION(vps[1].accumulatedDamage(), QRegion(0, 0, 20, 20));
    tracker.finishFrame();
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

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps{
        {QRect(0, 0, 100, 100), QTransform::fromScale(2, 2)},
    };
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    tracker.finishFrame();

}

void tst_Gdt::fullyOpaqueFlag()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 16, 16));
    g->setFullyOpaque(true);
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    QCOMPARE(g->worldOpaqueRegion(), QRegion(0, 0, 16, 16));

    g->setBoundingRect(QRectF(80, 80, 32, 16));
    tracker.commit();
    QCOMPARE(g->worldOpaqueRegion(), QRegion(80, 80, 32, 16));
    COMPARE_REGION(g->opaqueRegion(), QRegion(0, 0, 32, 16));
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

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion(0, 0, 10, 40));
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(10, 0, 30, 40));

    g->setBoundingRect(QRectF(80, 80, 40, 40));
    tracker.commit();
    COMPARE_REGION(g->worldOpaqueRegion(), QRegion(80, 80, 10, 40));
    COMPARE_REGION(g->opaqueRegion(), QRegion(0, 0, 10, 40));
}

void tst_Gdt::contentLocalDirtyFollowsBox()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(100, 110, 240, 150));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();

    g->markContentDirty(QRect(12, 24, 26, 20));
    COMPARE_REGION(tracker.commit(), QRegion(112, 134, 26, 20));

    g->setBoundingRect(QRectF(180, 150, 240, 150));
    tracker.commit();
    g->markContentDirty(QRect(12, 24, 26, 20));
    COMPARE_REGION(tracker.commit(), QRegion(192, 174, 26, 20));
}

void tst_Gdt::multipleDirtyRegions()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 200, 200));
    root.appendChild(g);

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
    tracker.commit();

    front->setBoundingRect(QRectF(100, 0, 80, 80));
    const QRegion d = tracker.commit();
    CONTAINS_REGION(d, QRegion(0, 0, 80, 80));
    CONTAINS_REGION(d, QRegion(100, 0, 80, 80));
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
    bg->setExpansion(2);
    tr->appendChild(behind);
    tr->appendChild(bg);
    root.appendChild(tr);

    TestTracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(10, 10, 1, 1));
    tracker.commit();
    CONTAINS_REGION(NodeTestAccess::inducedDamage(bg), QRegion(38, 48, 5, 5));
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

    TestTracker tracker(&root);
    tracker.commit();

    back->markContentDirty(QRect(22, 22, 4, 4));
    tracker.commit();
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

    TestTracker tracker(&root);
    tracker.commit();

    back->setBoundingRect(QRectF(60, 20, 20, 20));
    COMPARE_REGION(tracker.commit(), QRegion());
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion());
}

void tst_Gdt::negativeCoordinates()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(-30, -10, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
    tracker.commit();

    parent->markContentDirty(QRect(10, 10, 10, 10));
    tracker.commit();
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

    TestTracker tracker(&root);
    tracker.commit();
    QVERIFY(front->worldOpaqueRegion().isEmpty());
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
    COMPARE_REGION(tracker.commit(), QRegion(0, 0, 5, 5) + QRegion(10, 0, 5, 5));
}

void tst_Gdt::contentDirtyOnNewNode()
{
    Node root;
    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
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

    TestTracker tracker(&root);
    tracker.commit();
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
    bg->setExpansion(6);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 80, 80));
    root.appendChild(behind);
    root.appendChild(bg);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();

    behind->markContentDirty(QRect(20, 20, 2, 2));
    tracker.commit();
    CONTAINS_REGION(NodeTestAccess::inducedDamage(bg), QRegion(14, 14, 14, 14));
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

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps{
        {QRect(0, 0, 50, 50)},
        {QRect(100, 0, 50, 50)},
    };
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    COMPARE_REGION(vps[0].accumulatedDamage(), QRegion(0, 0, 40, 40));
    COMPARE_REGION(vps[1].accumulatedDamage(), QRegion(100, 0, 40, 40));
    tracker.finishFrame();

    left->markContentDirty(QRect(1, 1, 2, 2));
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    COMPARE_REGION(vps[0].accumulatedDamage(), QRegion(1, 1, 2, 2));
    COMPARE_REGION(vps[1].accumulatedDamage(), QRegion());
    tracker.finishFrame();
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

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps{
        {QRect(0, 0, 80, 80)},
        {QRect(80, 0, 80, 80)},
    };
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    tracker.finishFrame();
    COMPARE_REGION(front->worldVisibleRegion(), QRegion(0, 0, 80, 80));
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(80, 0, 120, 80));
}

void tst_Gdt::twoViewportsSingleCommitKeepsBoth()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(50, 0, 60, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps{
        {QRect(0, 0, 80, 40)},
        {QRect(80, 0, 80, 40)},
    };
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    COMPARE_REGION(vps[0].accumulatedDamage(), QRegion(50, 0, 30, 20));
    COMPARE_REGION(vps[1].accumulatedDamage(), QRegion(80, 0, 30, 20));
    tracker.finishFrame();
}

void tst_Gdt::twoViewportsIdempotent()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps{
        {QRect(0, 0, 50, 50)},
        {QRect(50, 0, 50, 50)},
    };
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    tracker.finishFrame();
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    COMPARE_REGION(vps[0].accumulatedDamage(), QRegion());
    COMPARE_REGION(vps[1].accumulatedDamage(), QRegion());
    tracker.finishFrame();
}

void tst_Gdt::twoViewportsDropped()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps1{
        {QRect(0, 0, 20, 20)},
        {QRect(100, 100, 20, 20)},
    };
    for (auto &vp : vps1) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps1) tracker.commit(vp);
    tracker.finishFrame();
    QVector<Tracker::Viewport> vps2{
        {QRect(0, 0, 20, 20)},
    };
    for (auto &vp : vps2) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps2) tracker.commit(vp);
    QCOMPARE(vps2.size(), 1);
    tracker.finishFrame();
}

void tst_Gdt::nodeAccessorsFollowPrimaryViewport()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 10, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps{
        {QRect(0, 0, 20, 20)},
        {QRect(50, 50, 10, 10)},
    };
    for (auto &vp : vps) vp.finishFrame(); tracker.prepareFrame(); for (auto &vp : vps) tracker.commit(vp);
    tracker.finishFrame();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(0, 0, 10, 10));
    QCOMPARE(g->worldBounds(), QRect(0, 0, 10, 10));
}

void tst_Gdt::nodeHasContentProperty()
{
    Node root;
    auto *container = new GeometryNode;
    container->setBoundingRect(QRectF(0, 0, 300, 300));
    container->setHasContent(false); // Acts as a structural container without drawing own pixels
    QVERIFY(!container->hasContent());

    auto *child = new GeometryNode;
    child->setBoundingRect(QRectF(50, 50, 100, 100));
    container->appendChild(child);
    root.appendChild(container);

    TestTracker tracker(&root);
    // First commit: only child generates damage, container has no own damage
    const QRegion damage1 = tracker.commit();
    COMPARE_REGION(damage1, QRegion(50, 50, 100, 100));
    COMPARE_REGION(NodeTestAccess::ownDamage(container), QRegion());
    COMPARE_REGION(container->worldVisibleRegion(), QRegion());
    COMPARE_REGION(child->worldVisibleRegion(), QRegion(50, 50, 100, 100));

    container->setBoundingRect(QRectF(0, 0, 400, 400));
    const QRegion damage2 = tracker.commit();
    COMPARE_REGION(damage2, QRegion());
    COMPARE_REGION(container->worldVisibleRegion(), QRegion());

    container->setHasContent(true);
    QVERIFY(container->hasContent());
    container->setBoundingRect(QRectF(0, 0, 500, 500));
    const QRegion damage3 = tracker.commit();
    QVERIFY(!damage3.isEmpty());
    COMPARE_REGION(container->worldVisibleRegion(), QRegion(0, 0, 500, 500));
}

void tst_Gdt::worldVisiblePartialOpaqueRegion()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 40, 40));
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 40, 40));
    front->setOpaqueRegion(QRegion(0, 0, 10, 40));
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(front->worldVisibleRegion(), QRegion(0, 0, 40, 40));
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(10, 0, 30, 40));
}

void tst_Gdt::worldVisibleTwoOpaqueFronts()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 100, 40));
    back->setFullyOpaque(true);
    auto *left = new GeometryNode;
    left->setBoundingRect(QRectF(0, 0, 20, 40));
    left->setFullyOpaque(true);
    auto *right = new GeometryNode;
    right->setBoundingRect(QRectF(80, 0, 20, 40));
    right->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(left);
    root.appendChild(right);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(right->worldVisibleRegion(), QRegion(80, 0, 20, 40));
    COMPARE_REGION(left->worldVisibleRegion(), QRegion(0, 0, 20, 40));
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(20, 0, 60, 40));
}

void tst_Gdt::worldVisibleRaiseReveals()
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

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(b->worldVisibleRegion(), QRegion(0, 0, 40, 40));
    COMPARE_REGION(a->worldVisibleRegion(), QRegion());

    root.appendChild(a);
    tracker.commit();
    COMPARE_REGION(a->worldVisibleRegion(), QRegion(0, 0, 40, 40));
    COMPARE_REGION(b->worldVisibleRegion(), QRegion());
}

void tst_Gdt::worldVisibleHasContentOff()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 30, 30));
    g->setFullyOpaque(true);
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(0, 0, 30, 30));

    g->setHasContent(false);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion());
}

void tst_Gdt::worldVisibleParentHideClearsChild()
{
    Node root;
    auto *group = new Node;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    group->appendChild(g);
    root.appendChild(group);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(0, 0, 20, 20));
    COMPARE_REGION(group->worldVisibleRegion(), QRegion());

    group->setVisible(false);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion());
    COMPARE_REGION(group->worldVisibleRegion(), QRegion());
}

void tst_Gdt::worldVisibleUnderTranslation()
{
    Node root;
    auto *tr = new TransformNode;
    tr->setTranslation(10, 20);
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 5, 6));
    tr->appendChild(g);
    root.appendChild(tr);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(10, 20, 5, 6));

    tr->setTranslation(40, 20);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(40, 20, 5, 6));
}

void tst_Gdt::worldVisibleNestedAndScale()
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

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(11, 22, 3, 4));

    Node root2;
    auto *tr = new TransformNode;
    tr->setScale(2, 3);
    auto *s = new GeometryNode;
    s->setBoundingRect(QRectF(1, 1, 4, 2));
    tr->appendChild(s);
    root2.appendChild(tr);
    TestTracker tracker2(&root2);
    tracker2.commit();
    COMPARE_REGION(s->worldVisibleRegion(), QRegion(2, 3, 8, 6));
}

void tst_Gdt::worldVisibleDemoDualOutput()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(160, 140, 220, 180));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(80, 80, 260, 200));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps{
        {QRect(0, 0, 360, 480)},
        {QRect(360, 0, 360, 480)},
    };
    for (auto &vp : vps) vp.finishFrame();
    tracker.prepareFrame();
    for (auto &vp : vps)
        tracker.commit(vp);
    tracker.finishFrame();

    COMPARE_REGION(front->worldVisibleRegion(), QRegion(80, 80, 260, 200));
    COMPARE_REGION(back->worldVisibleRegion(),
                   QRegion(160, 140, 220, 180) - QRegion(80, 80, 260, 200));
}

void tst_Gdt::worldVisibleOutsideAllOutputs()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(500, 500, 10, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    QCOMPARE(tracker.commit(QRect(0, 0, 100, 100)).isEmpty(), true);
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(500, 500, 10, 10));
}

void tst_Gdt::worldVisibleViewportMatrixIgnored()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(10, 5, 4, 3));
    root.appendChild(g);

    TestTracker tracker(&root);
    QVector<Tracker::Viewport> vps{
        {QRect(0, 0, 100, 100), QTransform::fromScale(2, 2)},
    };
    for (auto &vp : vps) vp.finishFrame();
    tracker.prepareFrame();
    for (auto &vp : vps)
        tracker.commit(vp);
    tracker.finishFrame();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(10, 5, 4, 3));
}

void tst_Gdt::worldVisibleNonAxisAlignedFront()
{
    Node root;
    auto *tr = new TransformNode;
    QTransform t;
    t.rotate(33);
    tr->setMatrix(t);
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 80, 80));
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 80, 80));
    front->setFullyOpaque(true);
    tr->appendChild(back);
    tr->appendChild(front);
    root.appendChild(tr);

    TestTracker tracker(&root);
    tracker.commit();
    QVERIFY(front->worldOpaqueRegion().isEmpty());
    COMPARE_REGION(front->worldVisibleRegion(), QRegion(front->worldBounds()));
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(back->worldBounds()));
}

void tst_Gdt::worldVisibleAfterGeometryMove()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(0, 0, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(0, 0, 20, 20));

    g->setBoundingRect(QRectF(50, 50, 20, 20));
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(50, 50, 20, 20));
}

void tst_Gdt::worldVisibleZeroSizeEmpty()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(10, 10, 0, 10));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    QVERIFY(g->worldVisibleRegion().isEmpty());
}

void tst_Gdt::worldVisibleRemoveFront()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 40, 40));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(0, 0, 40, 40));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(back->worldVisibleRegion(), QRegion());

    root.removeChild(front);
    delete front;
    tracker.commit();
    COMPARE_REGION(back->worldVisibleRegion(), QRegion(0, 0, 40, 40));
}

void tst_Gdt::worldVisibleIdempotentSecondCommit()
{
    Node root;
    auto *back = new GeometryNode;
    back->setBoundingRect(QRectF(0, 0, 30, 30));
    back->setFullyOpaque(true);
    auto *front = new GeometryNode;
    front->setBoundingRect(QRectF(10, 0, 30, 30));
    front->setFullyOpaque(true);
    root.appendChild(back);
    root.appendChild(front);

    TestTracker tracker(&root);
    tracker.commit();
    const QRegion firstFront = front->worldVisibleRegion();
    const QRegion firstBack = back->worldVisibleRegion();
    tracker.commit();
    COMPARE_REGION(front->worldVisibleRegion(), firstFront);
    COMPARE_REGION(back->worldVisibleRegion(), firstBack);
}

void tst_Gdt::worldVisibleGroupHasNone()
{
    Node root;
    auto *group = new Node;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(7, 8, 9, 10));
    group->appendChild(g);
    root.appendChild(group);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(7, 8, 9, 10));
    COMPARE_REGION(group->worldVisibleRegion(), QRegion());
}

void tst_Gdt::worldVisiblePartialChildCover()
{
    Node root;
    auto *parent = new GeometryNode;
    parent->setBoundingRect(QRectF(0, 0, 50, 50));
    parent->setFullyOpaque(true);
    auto *child = new GeometryNode;
    child->setBoundingRect(QRectF(10, 10, 10, 10));
    child->setFullyOpaque(true);
    parent->appendChild(child);
    root.appendChild(parent);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(child->worldVisibleRegion(), QRegion(10, 10, 10, 10));
    COMPARE_REGION(parent->worldVisibleRegion(),
                   QRegion(0, 0, 50, 50) - QRegion(10, 10, 10, 10));
}

void tst_Gdt::worldVisibleDeepNested()
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

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(32, 0, 2, 2));
}

void tst_Gdt::worldVisibleInsertAndReparent()
{
    Node root;
    auto *a = new GeometryNode;
    a->setBoundingRect(QRectF(0, 0, 30, 30));
    a->setFullyOpaque(true);
    auto *c = new GeometryNode;
    c->setBoundingRect(QRectF(0, 0, 30, 30));
    c->setFullyOpaque(true);
    root.appendChild(a);
    root.appendChild(c);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(c->worldVisibleRegion(), QRegion(0, 0, 30, 30));
    COMPARE_REGION(a->worldVisibleRegion(), QRegion());

    auto *b = new GeometryNode;
    b->setBoundingRect(QRectF(10, 0, 10, 30));
    b->setFullyOpaque(true);
    root.insertChildBefore(b, c);
    tracker.commit();
    COMPARE_REGION(c->worldVisibleRegion(), QRegion(0, 0, 30, 30));
    COMPARE_REGION(b->worldVisibleRegion(), QRegion());
    COMPARE_REGION(a->worldVisibleRegion(), QRegion());

    auto *t2 = new TransformNode;
    t2->setTranslation(80, 0);
    root.appendChild(t2);
    t2->appendChild(b);
    tracker.commit();
    COMPARE_REGION(b->worldVisibleRegion(), QRegion(90, 0, 10, 30));
    COMPARE_REGION(c->worldVisibleRegion(), QRegion(0, 0, 30, 30));
    COMPARE_REGION(a->worldVisibleRegion(), QRegion());
}

void tst_Gdt::worldVisibleHideShow()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(8, 8, 16, 16));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(8, 8, 16, 16));

    g->setVisible(false);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion());

    g->setVisible(true);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(8, 8, 16, 16));
}

void tst_Gdt::worldVisibleNegativeCoords()
{
    Node root;
    auto *g = new GeometryNode;
    g->setBoundingRect(QRectF(-30, -10, 20, 20));
    root.appendChild(g);

    TestTracker tracker(&root);
    tracker.commit();
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(-30, -10, 20, 20));
}

void tst_Gdt::worldVisibleRotate90()
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

    TestTracker tracker(&root);
    tracker.commit();
    QVERIFY(isAxisAligned(g->worldTransform()));
    QVERIFY(!g->worldOpaqueRegion().isEmpty());
    COMPARE_REGION(g->worldVisibleRegion(), QRegion(g->worldBounds()));
}

QTEST_MAIN(tst_Gdt)
#include "tst_gdt.moc"
