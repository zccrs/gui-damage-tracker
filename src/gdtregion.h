#ifndef GDTREGION_H
#define GDTREGION_H

#include <QMargins>
#include <QRect>
#include <QRectF>
#include <QRegion>
#include <QTransform>
#include <QtMath>

#include <cmath>

namespace Gdt {

// Pixel coverage of a continuous rect: over-estimate (damage) and under-estimate (opaque).
inline QRect outerAligned(const QRectF &r)
{
    if (!r.isValid() || r.width() <= 0.0 || r.height() <= 0.0)
        return {};
    const int x1 = int(std::floor(r.left()));
    const int y1 = int(std::floor(r.top()));
    const int x2 = int(std::ceil(r.right()));
    const int y2 = int(std::ceil(r.bottom()));
    if (x2 <= x1 || y2 <= y1)
        return {};
    return QRect(x1, y1, x2 - x1, y2 - y1);
}

inline QRect innerAligned(const QRectF &r)
{
    if (!r.isValid() || r.width() <= 0.0 || r.height() <= 0.0)
        return {};
    const int x1 = int(std::ceil(r.left()));
    const int y1 = int(std::ceil(r.top()));
    const int x2 = int(std::floor(r.right()));
    const int y2 = int(std::floor(r.bottom()));
    if (x2 <= x1 || y2 <= y1)
        return {};
    return QRect(x1, y1, x2 - x1, y2 - y1);
}

// Axis-aligned = translation / scale / 90°-multiple rotation. No shear or arbitrary rotation.
inline bool isAxisAligned(const QTransform &t)
{
    if (!t.isAffine())
        return false;
    const bool noOffAxis = qFuzzyIsNull(t.m12()) && qFuzzyIsNull(t.m21());
    const bool rot90 = qFuzzyIsNull(t.m11()) && qFuzzyIsNull(t.m22());
    return noOffAxis || rot90;
}

inline QRect mapOuter(const QTransform &t, const QRectF &local)
{
    if (!local.isValid() || local.width() <= 0.0 || local.height() <= 0.0)
        return {};
    if (t.isIdentity())
        return outerAligned(local);
    return outerAligned(t.mapRect(local));
}

inline QRect mapInner(const QTransform &t, const QRectF &local)
{
    if (!local.isValid() || local.width() <= 0.0 || local.height() <= 0.0)
        return {};
    if (!isAxisAligned(t))
        return {};
    if (t.isIdentity())
        return innerAligned(local);
    return innerAligned(t.mapRect(local));
}

// Damage mapping: never smaller than the true covered pixels.
inline QRegion mapRegionOuter(const QTransform &t, const QRegion &local)
{
    if (local.isEmpty())
        return {};
    if (t.isIdentity())
        return local;
    if (t.type() <= QTransform::TxTranslate) {
        const qreal dx = t.dx();
        const qreal dy = t.dy();
        if (std::floor(dx) == dx && std::floor(dy) == dy)
            return local.translated(int(dx), int(dy));
    }
    if (!isAxisAligned(t))
        return QRegion(mapOuter(t, QRectF(local.boundingRect())));
    if (local.rectCount() == 1)
        return QRegion(mapOuter(t, QRectF(local.boundingRect())));
    QRegion out;
    for (const QRect &r : local)
        out += mapOuter(t, QRectF(r));
    return out;
}

// Opaque mapping: never larger than the true fully-opaque pixels.
// Non-axis-aligned transforms contribute nothing (cannot prove coverage cheaply).
inline QRegion mapRegionInner(const QTransform &t, const QRegion &local)
{
    if (local.isEmpty())
        return {};
    if (!isAxisAligned(t))
        return {};
    if (t.isIdentity())
        return local;
    if (t.type() <= QTransform::TxTranslate) {
        const qreal dx = t.dx();
        const qreal dy = t.dy();
        if (std::floor(dx) == dx && std::floor(dy) == dy)
            return local.translated(int(dx), int(dy));
    }
    if (local.rectCount() == 1) {
        const QRect ir = innerAligned(t.mapRect(QRectF(local.boundingRect())));
        return ir.isEmpty() ? QRegion() : QRegion(ir);
    }
    QRegion out;
    for (const QRect &r : local) {
        const QRect ir = innerAligned(t.mapRect(QRectF(r)));
        if (!ir.isEmpty())
            out += ir;
    }
    return out;
}

inline QRegion dilateRegion(const QRegion &r, const QMargins &m)
{
    if (r.isEmpty())
        return {};
    if (m.isNull())
        return r;
    if (r.rectCount() == 1)
        return QRegion(r.boundingRect().marginsAdded(m));
    QRegion out;
    for (const QRect &rect : r)
        out += rect.marginsAdded(m);
    return out;
}

inline bool regionContainsRect(const QRegion &region, const QRect &rect)
{
    if (rect.isEmpty())
        return true;
    if (region.isEmpty())
        return false;
    return (QRegion(rect) - region).isEmpty();
}

} // namespace Gdt

#endif // GDTREGION_H
