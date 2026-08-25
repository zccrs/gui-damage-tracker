#ifndef DAMAGEOVERLAY_H
#define DAMAGEOVERLAY_H

#include <QQmlEngine>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantList>

class DamageOverlay : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList frames READ frames WRITE setFrames NOTIFY framesChanged)
    Q_PROPERTY(QVariantList flushFrames READ flushFrames WRITE setFlushFrames NOTIFY flushFramesChanged)
    Q_PROPERTY(int displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)
    Q_PROPERTY(int historyDuration READ historyDuration WRITE setHistoryDuration
               NOTIFY historyDurationChanged)
    Q_PROPERTY(int refreshRate READ refreshRate WRITE setRefreshRate NOTIFY refreshRateChanged)
    Q_PROPERTY(bool holdCurrent READ holdCurrent WRITE setHoldCurrent NOTIFY holdCurrentChanged)

public:
    explicit DamageOverlay(QQuickItem *parent = nullptr);

    QVariantList frames() const { return m_frames; }
    void setFrames(const QVariantList &frames);

    QVariantList flushFrames() const { return m_flushFrames; }
    void setFlushFrames(const QVariantList &frames);

    int displayMode() const { return m_displayMode; }
    void setDisplayMode(int mode);

    int historyDuration() const { return m_historyDuration; }
    void setHistoryDuration(int duration);

    int refreshRate() const { return m_refreshRate; }
    void setRefreshRate(int refreshRate);

    bool holdCurrent() const { return m_holdCurrent; }
    void setHoldCurrent(bool hold);

    void paint(QPainter *painter) override;

signals:
    void framesChanged();
    void flushFramesChanged();
    void displayModeChanged();
    void historyDurationChanged();
    void refreshRateChanged();
    void holdCurrentChanged();

private:
    void paintRects(QPainter *painter, const QVariantList &rects, const QColor &color);
    bool paintSeries(QPainter *painter, const QVariantList &frames,
                     const QColor &newest, const QColor &oldest, qint64 now);

    QVariantList m_frames;
    QVariantList m_flushFrames;
    QTimer m_repaintTimer;
    int m_historyDuration = 200;
    int m_refreshRate = 60;
    bool m_holdCurrent = false;
    int m_displayMode = 0;
};

#endif
