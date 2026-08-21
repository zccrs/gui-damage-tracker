#ifndef DEMOSCENE_H
#define DEMOSCENE_H

#include "gdt.h"
#include "visualnodemodel.h"


#include <QColor>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantList>

#include <memory>

class DemoScene : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(VisualNodeModel* visualNodes READ visualNodesModel CONSTANT)
    Q_PROPERTY(QVariantList treeNodes READ treeNodes NOTIFY treeNodesChanged)
    Q_PROPERTY(QVariantList damageRects READ damageRects NOTIFY damageChanged)
    Q_PROPERTY(QVariantList damageRectsB READ damageRectsB NOTIFY damageChanged)
    Q_PROPERTY(QVariantList damageFrames READ damageFrames NOTIFY damageChanged)
    Q_PROPERTY(QRect viewportA READ viewportA NOTIFY sceneChanged)
    Q_PROPERTY(QRect viewportB READ viewportB NOTIFY sceneChanged)
    Q_PROPERTY(int selectionType READ selectionType NOTIFY selectionChanged)
    Q_PROPERTY(quint64 selectedId READ selectedId WRITE setSelectedId NOTIFY selectedIdChanged)
    Q_PROPERTY(int selectedViewportId READ selectedViewportId WRITE setSelectedViewportId NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedProps READ selectedProps NOTIFY selectedPropsChanged)
    Q_PROPERTY(QVariantMap selectedViewportProps READ selectedViewportProps NOTIFY selectedViewportPropsChanged)
    Q_PROPERTY(bool autoCommit READ autoCommit WRITE setAutoCommit NOTIFY autoCommitChanged)
    Q_PROPERTY(int refreshRate READ refreshRate WRITE setRefreshRate NOTIFY refreshRateChanged)
    Q_PROPERTY(QVariantList demoScenes READ demoScenes CONSTANT)
    Q_PROPERTY(QString demoSceneName READ demoSceneName NOTIFY demoSceneChanged)
    Q_PROPERTY(bool demoRunning READ demoRunning WRITE setDemoRunning NOTIFY demoRunningChanged)
public:
    explicit DemoScene(QObject *parent = nullptr);
    ~DemoScene() override;

    VisualNodeModel *visualNodesModel() const { return m_visualNodeModel; }
    QVariantList treeNodes() const { return m_treeNodes; }
    QVariantList damageRects() const { return m_damageRects; }
    QVariantList damageRectsB() const { return m_damageRectsB; }
    QVariantList damageFrames() const { return m_damageFrames; }
    enum class SelectionType {
        None = 0,
        Node = 1,
        Viewport = 2
    };
    Q_ENUM(SelectionType)

    QRect viewportA() const;
    QRect viewportB() const;
    int selectionType() const;
    quint64 selectedId() const { return m_selectedId; }
    int selectedViewportId() const { return m_selectedViewportId; }
    QVariantMap selectedProps() const { return m_selectedProps; }
    QVariantMap selectedViewportProps() const;
    bool autoCommit() const { return m_autoCommit; }
    int refreshRate() const { return m_refreshRate; }
    QVariantList demoScenes() const;
    QString demoSceneName() const { return m_demoSceneName; }
    bool demoRunning() const { return m_demoRunning; }

    void setSelectedId(quint64 id);
    void setSelectedViewportId(int id);
    void setAutoCommit(bool enabled);
    void setRefreshRate(int refreshRate);
    void setDemoRunning(bool running);

    Q_INVOKABLE void selectNode(quint64 id);
    Q_INVOKABLE void selectViewport(int viewportId);
    Q_INVOKABLE void setViewportRect(int x, int y, int w, int h);
    Q_INVOKABLE void setViewportScale(qreal scale);
    Q_INVOKABLE void setViewportRotation(qreal degrees);
    Q_INVOKABLE void setViewportBufferCount(int count);
    Q_INVOKABLE void setViewportSwapchainEnabled(bool enabled);
    Q_INVOKABLE void setViewportSwapchainDamageRect(int x, int y, int w, int h);
    Q_INVOKABLE void loadDemoScene(const QString &name);
    Q_INVOKABLE void stepDemoFrame();
    Q_INVOKABLE void moveNode(quint64 nodeId, quint64 newParentId,
                              quint64 beforeSiblingId = 0);
    Q_INVOKABLE void activateNode(quint64 id);

    Q_INVOKABLE void addBasic();
    Q_INVOKABLE void addTransform();
    Q_INVOKABLE void addGeometry();
    Q_INVOKABLE void addBackdrop();
    Q_INVOKABLE void addRenderer();
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void raiseSelected();
    Q_INVOKABLE void lowerSelected();
    Q_INVOKABLE void setVisibleSelected(bool visible);
    Q_INVOKABLE void setHasContentSelected(bool hasContent);
    Q_INVOKABLE void setRectSelected(qreal x, qreal y, qreal w, qreal h);
    Q_INVOKABLE void setTranslationSelected(qreal x, qreal y);
    Q_INVOKABLE void setRotationSelected(qreal degrees, int axis = 2);
    Q_INVOKABLE void setScaleSelected(qreal sx, qreal sy);
    Q_INVOKABLE void setFullyOpaqueSelected(bool opaque);
    Q_INVOKABLE void setExpansionSelected(int px);
    Q_INVOKABLE void setClipExpansionSelected(bool clip);
    Q_INVOKABLE void markSelectedContentDirty();
    Q_INVOKABLE void markSelectedContentDirtyAt(qreal x, qreal y, qreal w, qreal h);
    Q_INVOKABLE void moveSelectedBy(qreal dx, qreal dy);
    Q_INVOKABLE void finishSelectedMove();
    Q_INVOKABLE void loadPreset(const QString &name);
    Q_INVOKABLE void injectSwapchainDamage(int viewportIndex = 0, int x = 60, int y = 80,
                                           int w = 220, int h = 140);
    Q_INVOKABLE void commit();
    Q_INVOKABLE void clearTree();
signals:
    void sceneChanged();
    void treeNodesChanged();
    void damageChanged();
    void selectionChanged();
    void selectedIdChanged();
    void selectedPropsChanged();
    void selectedViewportPropsChanged();
    void autoCommitChanged();
    void refreshRateChanged();
    void demoSceneChanged();
    void demoRunningChanged();
private:
    struct Decor {
        QColor color;
    };

    Gdt::Node *findNode(quint64 id) const;
    Gdt::Node *findNodeRecursive(Gdt::Node *n, quint64 id) const;
    Gdt::Node *parentForInsert() const;
    QColor nextColor();
    void rebuildLists();
    void rebuildVisualNodes();
    void collectVisualOnly(Gdt::Node *n, QVector<QVariantMap> *visual, int *paintOrder);
    void collectVisual(Gdt::Node *n, QVector<QVariantMap> *visual, QVariantList *tree,
                       int depth, int *paintOrder);
    void refreshSelectedProps();
    void maybeCommit();
    void updateDamage(bool rebuildScene);
    void resetRoot();
    void advanceDemoFrame();
    void buildDemoScene(const QString &name);
    bool isDescendantOf(const Gdt::Node *node, const Gdt::Node *ancestor) const;

    std::unique_ptr<Gdt::Node> m_root;
    Gdt::Tracker m_tracker;
    QHash<quint64, Decor> m_decor;
    QHash<quint64, QString> m_displayNames;
    VisualNodeModel *m_visualNodeModel = nullptr;
    QVariantList m_treeNodes;
    QVariantList m_damageRects;
    QVariantList m_damageRectsB;
    QVariantList m_damageFrames;
    QRegion m_injectedBufferDamageA;
    struct ViewportConfig {
        int id = 0;
        QString name;
        QRect outputRect;
        qreal scale = 1.0;
        qreal rotation = 0.0;
        int bufferCount = 2; // 2: 双缓冲, 3: 三缓冲, 4: 四缓冲
        bool swapchainEnabled = false;
        QRect swapchainDamageRect{30, 40, 180, 140};
    };

    void refreshSelectedViewportProps();
    ViewportConfig *currentSelectedViewportConfig();

    ViewportConfig m_vpA{0, QStringLiteral("输出 A (Primary)"), QRect(0, 0, 360, 480), 1.0, 0.0, 2, false, QRect(30, 40, 180, 140)};
    ViewportConfig m_vpB{1, QStringLiteral("输出 B (Secondary)"), QRect(360, 0, 360, 480), 1.0, 0.0, 2, false, QRect(30, 40, 180, 140)};
    int m_selectedViewportId = -1;
    quint64 m_selectedId = 0;
    QVariantMap m_selectedProps;
    bool m_autoCommit = true;
    bool m_dragFramePending = false;
    bool m_demoRunning = false;
    int m_refreshRate = 60;
    int m_demoFrame = 0;
    QString m_demoSceneName;
    quint64 m_demoNodeA = 0;
    quint64 m_demoNodeB = 0;
    quint64 m_demoNodeC = 0;
    int m_colorIndex = 0;
    int m_rotationAxis = 2;
    qreal m_rotation = 0;
    qreal m_scaleX = 1;
    qreal m_scaleY = 1;
    QRegion m_injectedBufferDamageB;
    QTimer m_dragFrameTimer;
    QTimer m_demoTimer;
};

#endif
