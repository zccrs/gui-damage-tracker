#include "demoscene.h"
#include "damageoverlay.h"

#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <QTest>

static QRegion regionFromRects(const QVariantList &rects)
{
    QRegion region;
    for (const QVariant &value : rects) {
        const QVariantMap rect = value.toMap();
        region += QRect(rect.value(QStringLiteral("x")).toInt(),
                        rect.value(QStringLiteral("y")).toInt(),
                        rect.value(QStringLiteral("w")).toInt(),
                        rect.value(QStringLiteral("h")).toInt());
    }
    return region;
}

class tst_Demo : public QObject
{
    Q_OBJECT

private slots:
    void paintOrderNames();
    void addedChildIsNumberedAndSelected();
    void moveActionsFollowTreeDirection();
    void activatingNodeRaisesItToTop();
    void treeMoveReparentsAndReorders();
    void activatingDemoNodeRaisesItToTop();
    void rotationAndScaleScenesAnimate();
    void builtInScenesAnimateAndAllowEditing();
    void selectingTreeNodeDoesNotRaiseIt();
    void activateNodeDoesNotSelect();
    void moveNodeByDoesNotSelect();
    void opacityChangesFixedName();
    void dragUpdatesCoordinatesInRealTime();
    void refreshRateThrottlesDragFrames();
    void damageHistoryAccumulatesFrames();
    void damageHistoryColorsOldGreenNewRed();
    void damageHistoryDurationIsConfigurable();
    void refreshRateIsConfigurable();
    void autoCommitOffMoveDoesNotCommit();
    void holdCurrentKeepsLatestDamageFrame();
    void backdropSceneUsesBackdropLabel();
    void rendererCoveredBehindDirty();
    void rendererBufferDoesNotEnlargePresent();
    void rendererExpansionDilatesPresent();
    void disablingBackdropDropsCoveredDamage();
    void simulatedRendererProvidesFlush();
    void rotationAxisPersistsAfterCommit();
    void transformInspectorExposesMatrix();
};

void tst_Demo::paintOrderNames()
{
    DemoScene scene;
    const QVariantList tree = scene.treeNodes();

    QCOMPARE(tree.size(), 3);
    const QString n0 = tree.at(0).toMap().value(QStringLiteral("name")).toString();
    const QString n1 = tree.at(1).toMap().value(QStringLiteral("name")).toString();
    const QString n2 = tree.at(2).toMap().value(QStringLiteral("name")).toString();
    QVERIFY(n0.startsWith(QStringLiteral("0:根节点 #")));
    QVERIFY(n1.startsWith(QStringLiteral("1:不透明几何 #")));
    QVERIFY(n1.contains(QStringLiteral("[260x200]")));
    QVERIFY(n2.startsWith(QStringLiteral("2:不透明几何 #")));
    QVERIFY(n2.contains(QStringLiteral("[220x180]")));
}
void tst_Demo::addedChildIsNumberedAndSelected()
{
    DemoScene scene;
    scene.addGeometry();

    const QVariantList tree = scene.treeNodes();
    QCOMPARE(tree.size(), 4);
    const QString n2 = tree.at(2).toMap().value(QStringLiteral("name")).toString();
    const QString n3 = tree.at(3).toMap().value(QStringLiteral("name")).toString();
    QVERIFY(n2.startsWith(QStringLiteral("2:不透明几何 #")));
    QVERIFY(n3.startsWith(QStringLiteral("3:不透明几何 #")));
    QCOMPARE(scene.selectedProps().value(QStringLiteral("name")).toString(), n2);
}

void tst_Demo::moveActionsFollowTreeDirection()
{
    DemoScene scene;
    const quint64 selected = scene.selectedId();
    scene.raiseSelected();
    QVariantList tree = scene.treeNodes();
    QCOMPARE(tree.at(2).toMap().value(QStringLiteral("id")).toULongLong(), selected);
    QVERIFY(scene.selectedProps().value(QStringLiteral("name")).toString()
                .startsWith(QStringLiteral("2:不透明几何 #")));

    scene.lowerSelected();
    tree = scene.treeNodes();
    QCOMPARE(tree.at(1).toMap().value(QStringLiteral("id")).toULongLong(), selected);
    QVERIFY(scene.selectedProps().value(QStringLiteral("name")).toString()
                .startsWith(QStringLiteral("1:不透明几何 #")));
}

void tst_Demo::activatingNodeRaisesItToTop()
{
    DemoScene scene;
    const quint64 back = scene.treeNodes().at(1).toMap()
                             .value(QStringLiteral("id")).toULongLong();
    const quint64 front = scene.treeNodes().at(2).toMap()
                              .value(QStringLiteral("id")).toULongLong();

    scene.setSelectedId(back);
    QCOMPARE(scene.treeNodes().at(1).toMap().value(QStringLiteral("id")).toULongLong(), back);
    QCOMPARE(scene.treeNodes().at(2).toMap().value(QStringLiteral("id")).toULongLong(), front);

    scene.activateNode(back);

    const QVariantList tree = scene.treeNodes();
    QCOMPARE(tree.at(2).toMap().value(QStringLiteral("id")).toULongLong(), back);
    QVERIFY(scene.selectedProps().value(QStringLiteral("name")).toString()
                .startsWith(QStringLiteral("2:不透明几何 #")));
}

void tst_Demo::activatingDemoNodeRaisesItToTop()
{
    DemoScene scene;
    scene.loadDemoScene(QStringLiteral("occlusion"));
    const quint64 back = scene.treeNodes().at(1).toMap()
                             .value(QStringLiteral("id")).toULongLong();
    scene.activateNode(back);
    QCOMPARE(scene.treeNodes().at(2).toMap().value(QStringLiteral("id")).toULongLong(), back);
}
void tst_Demo::treeMoveReparentsAndReorders()
{
    DemoScene scene;
    const QVariantList initial = scene.treeNodes();
    const quint64 rootId = initial.at(0).toMap().value(QStringLiteral("id")).toULongLong();
    const quint64 backId = initial.at(1).toMap().value(QStringLiteral("id")).toULongLong();
    const quint64 frontId = initial.at(2).toMap().value(QStringLiteral("id")).toULongLong();

    scene.moveNode(frontId, backId);
    QCOMPARE(scene.treeNodes().at(2).toMap().value(QStringLiteral("parentId")).toULongLong(),
             backId);

    scene.moveNode(frontId, rootId, backId);
    QCOMPARE(scene.treeNodes().at(1).toMap().value(QStringLiteral("id")).toULongLong(),
             frontId);
    QCOMPARE(scene.treeNodes().at(1).toMap().value(QStringLiteral("parentId")).toULongLong(),
             rootId);
}

void tst_Demo::builtInScenesAnimateAndAllowEditing()
{
    DemoScene scene;
    scene.loadDemoScene(QStringLiteral("content"));
    QCOMPARE(scene.demoScenes().size(), 7);
    QCOMPARE(scene.demoSceneName(), QStringLiteral("content"));
    QVERIFY(!scene.selectedProps().isEmpty());
    const qsizetype initialFrames = scene.renderFrames().size();
    scene.setDemoRunning(false);
    scene.setDemoRunning(true);
    scene.stepDemoFrame();
    QVERIFY(scene.renderFrames().size() > initialFrames);
    scene.setDemoRunning(false);
    const qsizetype pausedFrames = scene.renderFrames().size();
    scene.stepDemoFrame();
    QCOMPARE(scene.renderFrames().size(), pausedFrames);

    const int count = scene.treeNodes().size();
    scene.addGeometry();
    QCOMPARE(scene.treeNodes().size(), count + 1);
}
void tst_Demo::rotationAndScaleScenesAnimate()
{
    DemoScene scene;
    for (const QString &name : {QStringLiteral("rotation"), QStringLiteral("scale")}) {
        scene.loadDemoScene(name);
        QCOMPARE(scene.selectedProps().value(QStringLiteral("type")).toString(),
                 QStringLiteral("Transform"));
        const qsizetype initialFrames = scene.renderFrames().size();
        scene.stepDemoFrame();
        QVERIFY(scene.renderFrames().size() > initialFrames);
    }
}

void tst_Demo::selectingTreeNodeDoesNotRaiseIt()
{
    DemoScene scene;
    scene.raiseSelected();
    const quint64 lowerNode = scene.treeNodes().at(1).toMap()
                                  .value(QStringLiteral("id")).toULongLong();

    scene.setSelectedId(lowerNode);

    QCOMPARE(scene.treeNodes().at(1).toMap()
                 .value(QStringLiteral("id")).toULongLong(), lowerNode);
}

void tst_Demo::activateNodeDoesNotSelect()
{
    DemoScene scene;
    const quint64 back = scene.treeNodes().at(1).toMap()
                             .value(QStringLiteral("id")).toULongLong();
    const quint64 front = scene.treeNodes().at(2).toMap()
                              .value(QStringLiteral("id")).toULongLong();
    scene.setSelectedId(front);
    scene.activateNode(back);
    QCOMPARE(scene.selectedId(), front);
    QCOMPARE(scene.treeNodes().at(2).toMap().value(QStringLiteral("id")).toULongLong(), back);
}

void tst_Demo::moveNodeByDoesNotSelect()
{
    DemoScene scene;
    const quint64 selected = scene.selectedId();
    const quint64 other = scene.treeNodes().at(2).toMap()
                              .value(QStringLiteral("id")).toULongLong();
    QVERIFY(other != selected);
    scene.moveNodeBy(other, 5, 7);
    QCOMPARE(scene.selectedId(), selected);
}


void tst_Demo::opacityChangesFixedName()
{
    DemoScene scene;
    scene.setFullyOpaqueSelected(false);

    const QString name = scene.selectedProps().value(QStringLiteral("name")).toString();
    QVERIFY(name.startsWith(QStringLiteral("1:透明几何 #")));
    QCOMPARE(scene.treeNodes().at(1).toMap().value(QStringLiteral("name")).toString(),
             name);
}
void tst_Demo::dragUpdatesCoordinatesInRealTime()
{
    DemoScene scene;
    scene.moveSelectedBy(5, 7);

    QCOMPARE(scene.selectedProps().value(QStringLiteral("x")).toReal(), 85.0);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("y")).toReal(), 87.0);
    QCOMPARE(scene.visualNodesModel()->getRow(0).value(QStringLiteral("x")).toReal(), 85.0);
    QCOMPARE(scene.visualNodesModel()->getRow(0).value(QStringLiteral("y")).toReal(), 87.0);
    scene.finishSelectedMove();
    QCOMPARE(scene.visualNodesModel()->getRow(0).value(QStringLiteral("x")).toReal(), 85.0);
    QCOMPARE(scene.visualNodesModel()->getRow(0).value(QStringLiteral("y")).toReal(), 87.0);
}

void tst_Demo::refreshRateThrottlesDragFrames()
{
    DemoScene scene;
    scene.setRefreshRate(1);
    const qsizetype initialFrames = scene.renderFrames().size();

    scene.moveSelectedBy(1, 0);
    QCOMPARE(scene.renderFrames().size(), initialFrames + 1);

    scene.moveSelectedBy(1, 0);
    QCOMPARE(scene.renderFrames().size(), initialFrames + 1);

    scene.finishSelectedMove();
    QCOMPARE(scene.renderFrames().size(), initialFrames + 2);
}

void tst_Demo::damageHistoryAccumulatesFrames()
{
    DemoScene scene;
    const qsizetype initialFrames = scene.renderFrames().size();

    scene.moveSelectedBy(2, 0);
    scene.moveSelectedBy(3, 0);
    scene.finishSelectedMove();

    const QVariantList frames = scene.renderFrames();
    QCOMPARE(frames.size(), initialFrames + 2);
    const QVariantMap previous = frames.at(frames.size() - 2).toMap();
    const QVariantMap newest = frames.constLast().toMap();
    QVERIFY(!previous.value(QStringLiteral("rects")).toList().isEmpty());
    QVERIFY(!newest.value(QStringLiteral("rects")).toList().isEmpty());
    QVERIFY(previous.value(QStringLiteral("timestamp")).toLongLong()
            <= newest.value(QStringLiteral("timestamp")).toLongLong());
}

void tst_Demo::damageHistoryColorsOldGreenNewRed()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto frame = [](qint64 timestamp, const QRect &rect) {
        QVariantMap geometry;
        geometry.insert(QStringLiteral("x"), rect.x());
        geometry.insert(QStringLiteral("y"), rect.y());
        geometry.insert(QStringLiteral("w"), rect.width());
        geometry.insert(QStringLiteral("h"), rect.height());
        QVariantMap damageFrame;
        damageFrame.insert(QStringLiteral("timestamp"), timestamp);
        damageFrame.insert(QStringLiteral("rects"), QVariantList{geometry});
        return damageFrame;
    };

    QVariantList frames;
    for (int i = 0; i < 8; ++i)
        frames.append(frame(now, QRect(i * 10, 0, 10, 10)));

    DamageOverlay overlay;
    overlay.setHistoryDuration(1800);
    overlay.setRenderFrames(frames);

    QImage image(82, 12, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    overlay.paint(&painter);
    painter.end();

    const QColor oldDamage = image.pixelColor(5, 5);
    const QColor newDamage = image.pixelColor(75, 5);
    QVERIFY(oldDamage.green() > oldDamage.red());
    QVERIFY(newDamage.red() > newDamage.green());
    QVERIFY(oldDamage.alpha() < 150);
    QVERIFY(newDamage.alpha() < 150);

    QVariantList overlapping{
        frame(now - 100, QRect(0, 0, 10, 10)),
        frame(now, QRect(0, 0, 10, 10)),
    };
    overlay.setRenderFrames(overlapping);
    image.fill(Qt::transparent);
    QPainter overlapPainter(&image);
    overlay.paint(&overlapPainter);
    overlapPainter.end();
    const QColor overlapDamage = image.pixelColor(5, 5);
    QVERIFY(overlapDamage.red() > overlapDamage.green());
    QVERIFY(overlapDamage.alpha() < 200);
    // Time controls expiration only. A lone frame stays red for its lifetime.
    overlay.setRenderFrames({frame(now - 1500, QRect(0, 0, 10, 10))});
    image.fill(Qt::transparent);
    QPainter singlePainter(&image);
    overlay.paint(&singlePainter);
    singlePainter.end();
    const QColor singleDamage = image.pixelColor(5, 5);
    QVERIFY(singleDamage.red() > singleDamage.green());
}

void tst_Demo::damageHistoryDurationIsConfigurable()
{
    DamageOverlay overlay;
    overlay.setHistoryDuration(3200);
    QCOMPARE(overlay.historyDuration(), 3200);

    overlay.setHistoryDuration(50);
    QCOMPARE(overlay.historyDuration(), 100);
}

void tst_Demo::refreshRateIsConfigurable()
{
    DemoScene scene;
    scene.setRefreshRate(144);
    QCOMPARE(scene.refreshRate(), 144);
    scene.setRefreshRate(0);
    QCOMPARE(scene.refreshRate(), 1);

    DamageOverlay overlay;
    overlay.setRefreshRate(144);
    QCOMPARE(overlay.refreshRate(), 144);
    overlay.setRefreshRate(0);
    QCOMPARE(overlay.refreshRate(), 1);
}

void tst_Demo::autoCommitOffMoveDoesNotCommit()
{
    DemoScene scene;
    scene.setAutoCommit(false);
    const qsizetype frames = scene.renderFrames().size();
    scene.moveSelectedBy(5, 7);
    scene.finishSelectedMove();
    QCOMPARE(scene.renderFrames().size(), frames);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("x")).toReal(), 85.0);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("y")).toReal(), 87.0);
}

void tst_Demo::holdCurrentKeepsLatestDamageFrame()
{
    DamageOverlay overlay;
    overlay.setHoldCurrent(true);
    QVERIFY(overlay.holdCurrent());
    overlay.setDisplayMode(2);
    QCOMPARE(overlay.displayMode(), 2);
    overlay.setDisplayMode(1);
    QCOMPARE(overlay.displayMode(), 1);
    overlay.setHoldCurrent(false);
    QVERIFY(!overlay.holdCurrent());
}

void tst_Demo::backdropSceneUsesBackdropLabel()
{
    DemoScene scene;
    scene.loadDemoScene(QStringLiteral("backdrop"));
    bool found = false;
    for (const QVariant &row : scene.treeNodes()) {
        const QVariantMap m = row.toMap();
        if (m.value(QStringLiteral("isBackdrop")).toBool()) {
            found = true;
            QVERIFY(m.value(QStringLiteral("name")).toString().contains(QStringLiteral("背景采样")));
        }
    }
    QVERIFY(found);
}

void tst_Demo::simulatedRendererProvidesFlush()
{
    DemoScene scene;
    scene.loadDemoScene(QStringLiteral("backdrop-cover"));
    scene.setDemoRunning(false);

    const QRegion cover(40, 40, 160, 160);
    const QRegion backdropBounds(100, 100, 140, 140);
    const QRegion initial = cover + (backdropBounds - cover);
    QCOMPARE(regionFromRects(scene.renderRects()), initial);
    QCOMPARE(regionFromRects(scene.presentRects()), initial);
}

void tst_Demo::rendererCoveredBehindDirty()
{
    DemoScene scene;
    scene.loadDemoScene(QStringLiteral("backdrop-cover"));
    scene.setDemoRunning(false);

    scene.markSelectedContentDirtyAt(60, 60, 20, 20);
    const QRegion coveredDirty(180, 180, 20, 20);
    QCOMPARE(regionFromRects(scene.renderRects()), coveredDirty);
    QCOMPARE(regionFromRects(scene.presentRects()), QRegion());
}

void tst_Demo::rendererBufferDoesNotEnlargePresent()
{
    DemoScene scene;
    scene.loadDemoScene(QStringLiteral("backdrop-cover"));
    scene.setDemoRunning(false);
    scene.markSelectedContentDirtyAt(60, 60, 20, 20);

    scene.injectSwapchainDamage(0, 120, 120, 20, 20);
    QCOMPARE(regionFromRects(scene.renderRects()), QRegion(120, 120, 20, 20));
    QCOMPARE(regionFromRects(scene.presentRects()), QRegion());
}

void tst_Demo::rendererExpansionDilatesPresent()
{
    DemoScene scene;
    scene.loadDemoScene(QStringLiteral("backdrop"));
    scene.setDemoRunning(false);

    quint64 backdropId = 0;
    quint64 wallId = 0;
    for (const QVariant &row : scene.treeNodes()) {
        const QVariantMap m = row.toMap();
        const quint64 id = m.value(QStringLiteral("id")).toULongLong();
        if (m.value(QStringLiteral("isBackdrop")).toBool())
            backdropId = id;
        else if (wallId == 0 && m.value(QStringLiteral("type")).toString() == QLatin1String("Geometry"))
            wallId = id;
    }
    QVERIFY(backdropId != 0);
    QVERIFY(wallId != 0);

    scene.setSelectedId(backdropId);
    scene.setExpansionSelected(16);
    scene.setSelectedId(wallId);
    scene.markSelectedContentDirtyAt(140, 90, 10, 10);

    const QRegion present = regionFromRects(scene.presentRects());
    QVERIFY(present.contains(QRect(160, 110, 10, 10)));
    QVERIFY(present.contains(QRect(150, 100, 1, 1)));
    QVERIFY(!present.contains(QRect(143, 93, 1, 1)));
}

void tst_Demo::disablingBackdropDropsCoveredDamage()
{
    DemoScene scene;
    scene.loadDemoScene(QStringLiteral("backdrop-cover"));
    scene.setDemoRunning(false);

    quint64 backdropId = 0;
    const quint64 dirtyId = scene.selectedId();
    for (const QVariant &row : scene.treeNodes()) {
        const QVariantMap m = row.toMap();
        if (m.value(QStringLiteral("isBackdrop")).toBool())
            backdropId = m.value(QStringLiteral("id")).toULongLong();
    }
    QVERIFY(backdropId != 0);

    scene.setSelectedId(backdropId);
    scene.setNeedsBackdropSelected(false);
    scene.setSelectedId(dirtyId);
    scene.markSelectedContentDirtyAt(60, 60, 20, 20);
    QCOMPARE(regionFromRects(scene.renderRects()), QRegion());
    QCOMPARE(regionFromRects(scene.presentRects()), QRegion());
}

void tst_Demo::rotationAxisPersistsAfterCommit()
{
    DemoScene scene;
    scene.loadPreset(QStringLiteral("transform"));

    // Set rotation to X axis with 30 degrees.
    scene.setRotationSelected(30, 0);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("rotationAxis")).toInt(), 0);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("rotation")).toReal() - 30.0) < 0.1);

    // After a scale change, rotation axis and angle should still be X/30.
    scene.setScaleSelected(2, 3);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("rotationAxis")).toInt(), 0);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("rotation")).toReal() - 30.0) < 0.1);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("sx")).toReal() - 2.0) < 0.1);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("sy")).toReal() - 3.0) < 0.1);

    // Set to Y axis with 60 degrees — scale should be preserved.
    scene.setRotationSelected(60, 1);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("rotationAxis")).toInt(), 1);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("rotation")).toReal() - 60.0) < 0.1);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("sx")).toReal() - 2.0) < 0.1);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("sy")).toReal() - 3.0) < 0.1);

    // Change rotation angle again — scale should not change.
    scene.setRotationSelected(45, 1);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("rotation")).toReal() - 45.0) < 0.1);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("sx")).toReal() - 2.0) < 0.1);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("sy")).toReal() - 3.0) < 0.1);

    // Switch to Z axis.
    scene.setRotationSelected(90, 2);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("rotationAxis")).toInt(), 2);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("rotation")).toReal() - 90.0) < 0.1);

    // Visual nodes should include perspective components.
    scene.loadDemoScene(QStringLiteral("rotation"));
    const QVariantMap visual = scene.visualNodesModel()->getRow(0);
    QVERIFY(visual.contains(QStringLiteral("m13")));
    QVERIFY(visual.contains(QStringLiteral("m23")));
    QVERIFY(visual.contains(QStringLiteral("m33")));
}
void tst_Demo::transformInspectorExposesMatrix()
{
    DemoScene scene;
    scene.loadPreset(QStringLiteral("transform"));
    QVERIFY(scene.selectedProps().value(QStringLiteral("isTransform")).toBool());
    QCOMPARE(scene.selectedProps().value(QStringLiteral("tx")).toReal(), 40.0);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("ty")).toReal(), 30.0);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("rotation")).toReal()) < 0.01);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("sx")).toReal(), 1.0);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("sy")).toReal(), 1.0);

    scene.setRotationSelected(45, 2);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("rotation")).toReal() - 45.0) < 0.1);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("rotationAxis")).toInt(), 2);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("tx")).toReal(), 40.0);
    QCOMPARE(scene.selectedProps().value(QStringLiteral("ty")).toReal(), 30.0);
    scene.setScaleSelected(2, 3);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("sx")).toReal() - 2.0) < 0.1);
    QVERIFY(qAbs(scene.selectedProps().value(QStringLiteral("sy")).toReal() - 3.0) < 0.1);
    QVERIFY(!scene.renderFrames().isEmpty());

    scene.loadDemoScene(QStringLiteral("rotation"));
    const QVariantMap rotatedVisual = scene.visualNodesModel()->getRow(0);
    QVERIFY(rotatedVisual.contains(QStringLiteral("localWidth")));
    QVERIFY(rotatedVisual.contains(QStringLiteral("m11")));
    QVERIFY(rotatedVisual.contains(QStringLiteral("m22")));
}

QTEST_MAIN(tst_Demo)
#include "tst_demo.moc"
