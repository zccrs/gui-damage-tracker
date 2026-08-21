#include "damageoverlay.h"

#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>

static QColor interpolateColor(const QColor &newest, const QColor &oldest, qreal progress)
{
    const qreal t = qBound(0.0, progress, 1.0);
    return QColor::fromRgbF(newest.redF() + (oldest.redF() - newest.redF()) * t,
                            newest.greenF() + (oldest.greenF() - newest.greenF()) * t,
                            newest.blueF() + (oldest.blueF() - newest.blueF()) * t,
                            newest.alphaF() + (oldest.alphaF() - newest.alphaF()) * t);
}

static int refreshInterval(int refreshRate)
{
    return qMax(1, qRound(1000.0 / refreshRate));
}

DamageOverlay::DamageOverlay(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::NoButton);
    setAntialiasing(false);
    setFillColor(Qt::transparent);
    m_repaintTimer.setInterval(refreshInterval(m_refreshRate));
    connect(&m_repaintTimer, &QTimer::timeout, this, [this] {
        update();
    });
}

void DamageOverlay::setFrames(const QVariantList &frames)
{
    if (m_frames == frames)
        return;
    m_frames = frames;
    if (!m_frames.isEmpty())
        m_repaintTimer.start();
    emit framesChanged();
    update();
}

void DamageOverlay::setHistoryDuration(int duration)
{
    duration = qMax(100, duration);
    if (m_historyDuration == duration)
        return;
    m_historyDuration = duration;
    emit historyDurationChanged();
    update();
}

void DamageOverlay::setRefreshRate(int refreshRate)
{
    refreshRate = qBound(1, refreshRate, 1000);
    if (m_refreshRate == refreshRate)
        return;
    m_refreshRate = refreshRate;
    m_repaintTimer.setInterval(refreshInterval(m_refreshRate));
    emit refreshRateChanged();
}

void DamageOverlay::paintRects(QPainter *painter, const QVariantList &rects,
                               const QColor &color)
{
    if (rects.isEmpty())
        return;

    QRegion region;
    for (const QVariant &value : rects) {
        const QVariantMap rect = value.toMap();
        const QRect geometry(rect.value(QStringLiteral("x")).toInt(),
                             rect.value(QStringLiteral("y")).toInt(),
                             rect.value(QStringLiteral("w")).toInt(),
                             rect.value(QStringLiteral("h")).toInt());
        if (!geometry.isEmpty())
            region += geometry;
    }

    if (region.isEmpty())
        return;

    painter->save();
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);

    // 1. 统一填充整块 QRegion 区域
    for (const QRect &r : region)
        painter->fillRect(r, color);

    // 2. 构造整块 Region 的外轮廓路径，消解内部所有矩形拼缝，只绘制最外围轮廓
    QPainterPath path;
    path.addRegion(region);
    const QPainterPath outerContour = path.simplified();

    QColor borderColor = color;
    borderColor.setAlpha(qMin(255, color.alpha() * 2 + 50));
    painter->strokePath(outerContour, QPen(borderColor, 1.5, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));

    painter->restore();
}

void DamageOverlay::paint(QPainter *painter)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QColor newest(232, 62, 78, 110);
    const QColor oldest(45, 196, 112, 65);
    bool hasVisibleFrame = false;

    // Hue is based only on frame distance from the newest damage. Time controls
    // expiration, but never changes a frame's color.
    const qsizetype newestIndex = m_frames.size() - 1;
    constexpr qreal colorSteps = 7.0;
    for (qsizetype index = 0; index < m_frames.size(); ++index) {
        const QVariantMap frame = m_frames.at(index).toMap();
        const qint64 timestamp = frame.value(QStringLiteral("timestamp")).toLongLong();
        const qint64 age = qMax<qint64>(0, now - timestamp);
        if (age >= m_historyDuration)
            continue;
        hasVisibleFrame = true;
        const qreal progress = qMin(1.0, qreal(newestIndex - index) / colorSteps);
        paintRects(painter, frame.value(QStringLiteral("rects")).toList(),
                   interpolateColor(newest, oldest, progress));
    }

    if (!hasVisibleFrame)
        m_repaintTimer.stop();
}
