#ifndef GDTREGION_H
#define GDTREGION_H

#include <QMargins>
#include <QRect>
#include <QRectF>
#include <QTransform>
#include <QtCore/qglobal.h>

#include <pixman.h>

#include <cmath>

namespace Gdt {

class Region
{
public:
    Region();
    Region(int x, int y, int width, int height);
    explicit Region(const QRect &rect);
    explicit Region(const pixman_region32_t *region);
    Region(const Region &other);
    Region(Region &&other) noexcept;
    ~Region();

    Region &operator=(const Region &other);
    Region &operator=(Region &&other) noexcept;

    const pixman_region32_t *native() const { return &m_region; }
    pixman_region32_t *native() { return &m_region; }

    bool isEmpty() const;
    int rectCount() const;
    QRect boundingRect() const;
    const pixman_box32_t *rectangles(int *count) const;

    void clear();
    void translate(int dx, int dy);
    Region translated(int dx, int dy) const;
    void setIntersection(const pixman_region32_t *source, const QRect &rect);
    void setIntersection(const pixman_region32_t *lhs, const pixman_region32_t *rhs);

    Region &operator+=(const Region &other);
    Region &operator+=(const QRect &rect);
    Region &operator-=(const Region &other);
    Region &operator-=(const QRect &rect);
    Region &operator&=(const Region &other);
    Region &operator&=(const QRect &rect);

    bool operator==(const Region &other) const;
    bool operator!=(const Region &other) const { return !(*this == other); }

private:
    pixman_region32_t m_region;
};

Region operator+(Region lhs, const Region &rhs);
Region operator+(Region lhs, const QRect &rhs);
Region operator-(Region lhs, const Region &rhs);
Region operator-(Region lhs, const QRect &rhs);
Region operator&(Region lhs, const Region &rhs);
Region operator&(Region lhs, const QRect &rhs);

// Pixel coverage of a continuous rect: over-estimate (damage) and under-estimate (opaque).
inline QRect outerAligned(const QRectF &r)
{
    if (Q_UNLIKELY(!r.isValid() || r.width() <= 0.0 || r.height() <= 0.0))
        return {};
    const int x1 = int(std::floor(r.left()));
    const int y1 = int(std::floor(r.top()));
    const int x2 = int(std::ceil(r.right()));
    const int y2 = int(std::ceil(r.bottom()));
    if (Q_UNLIKELY(x2 <= x1 || y2 <= y1))
        return {};
    return QRect(x1, y1, x2 - x1, y2 - y1);
}

inline QRect innerAligned(const QRectF &r)
{
    if (Q_UNLIKELY(!r.isValid() || r.width() <= 0.0 || r.height() <= 0.0))
        return {};
    const int x1 = int(std::ceil(r.left()));
    const int y1 = int(std::ceil(r.top()));
    const int x2 = int(std::floor(r.right()));
    const int y2 = int(std::floor(r.bottom()));
    if (Q_UNLIKELY(x2 <= x1 || y2 <= y1))
        return {};
    return QRect(x1, y1, x2 - x1, y2 - y1);
}

// Axis-aligned = translation / scale / 90-degree rotation. No shear/arbitrary rotation.
inline bool isAxisAligned(const QTransform &t)
{
    if (Q_UNLIKELY(!t.isAffine()))
        return false;
    const bool noOffAxis = qFuzzyIsNull(t.m12()) && qFuzzyIsNull(t.m21());
    const bool rot90 = qFuzzyIsNull(t.m11()) && qFuzzyIsNull(t.m22());
    return noOffAxis || rot90;
}

inline QRect mapOuter(const QTransform &t, const QRectF &local)
{
    if (Q_UNLIKELY(!local.isValid() || local.width() <= 0.0 || local.height() <= 0.0))
        return {};
    if (Q_LIKELY(t.isIdentity()))
        return outerAligned(local);
    return outerAligned(t.mapRect(local));
}

inline QRect mapInner(const QTransform &t, const QRectF &local)
{
    if (Q_UNLIKELY(!local.isValid() || local.width() <= 0.0 || local.height() <= 0.0))
        return {};
    if (Q_UNLIKELY(!isAxisAligned(t)))
        return {};
    if (Q_LIKELY(t.isIdentity()))
        return innerAligned(local);
    return innerAligned(t.mapRect(local));
}

Region mapRegionOuter(const QTransform &transform, const Region &local);
Region mapRegionInner(const QTransform &transform, const Region &local);
Region dilateRegion(const Region &region, const QMargins &margins);
bool regionContainsRect(const Region &region, const QRect &rect);

} // namespace Gdt

#endif // GDTREGION_H
