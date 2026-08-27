#include "gdtregion.h"

#include <cstdlib>
#include <utility>

namespace Gdt {

static void requireRegion(pixman_bool_t ok)
{
    if (Q_UNLIKELY(!ok))
        std::abort();
}

Region::Region()
{
    pixman_region32_init(&m_region);
}

Region::Region(int x, int y, int width, int height)
{
    if (width > 0 && height > 0)
        pixman_region32_init_rect(&m_region, x, y, unsigned(width), unsigned(height));
    else
        pixman_region32_init(&m_region);
}

Region::Region(const QRect &rect)
    : Region(rect.x(), rect.y(), rect.width(), rect.height())
{
}

Region::Region(const pixman_region32_t *region)
{
    pixman_region32_init(&m_region);
    if (region)
        requireRegion(pixman_region32_copy(&m_region, region));
}

Region::Region(const Region &other)
    : Region(other.native())
{
}

Region::Region(Region &&other) noexcept
    : m_region(other.m_region)
{
    pixman_region32_init(&other.m_region);
}

Region::~Region()
{
    pixman_region32_fini(&m_region);
}

Region &Region::operator=(const Region &other)
{
    if (this != &other)
        requireRegion(pixman_region32_copy(&m_region, &other.m_region));
    return *this;
}

Region &Region::operator=(Region &&other) noexcept
{
    if (this == &other)
        return *this;
    pixman_region32_fini(&m_region);
    m_region = other.m_region;
    pixman_region32_init(&other.m_region);
    return *this;
}

bool Region::isEmpty() const
{
    return !pixman_region32_not_empty(&m_region);
}

int Region::rectCount() const
{
    return pixman_region32_n_rects(&m_region);
}

QRect Region::boundingRect() const
{
    if (isEmpty())
        return {};
    const pixman_box32_t *box = pixman_region32_extents(&m_region);
    return QRect(box->x1, box->y1, box->x2 - box->x1, box->y2 - box->y1);
}

const pixman_box32_t *Region::rectangles(int *count) const
{
    return pixman_region32_rectangles(&m_region, count);
}

void Region::clear()
{
    pixman_region32_clear(&m_region);
}

void Region::translate(int dx, int dy)
{
    pixman_region32_translate(&m_region, dx, dy);
}

Region Region::translated(int dx, int dy) const
{
    Region result(*this);
    result.translate(dx, dy);
    return result;
}


void Region::setIntersection(const pixman_region32_t *source, const QRect &rect)
{
    if (!source || rect.isEmpty()) {
        clear();
        return;
    }
    requireRegion(pixman_region32_intersect_rect(&m_region, source,
        rect.x(), rect.y(), unsigned(rect.width()), unsigned(rect.height())));
}

void Region::setIntersection(const pixman_region32_t *lhs, const pixman_region32_t *rhs)
{
    if (!lhs || !rhs) {
        clear();
        return;
    }
    requireRegion(pixman_region32_intersect(&m_region, lhs, rhs));
}
Region &Region::operator+=(const Region &other)
{
    requireRegion(pixman_region32_union(&m_region, &m_region, &other.m_region));
    return *this;
}

Region &Region::operator+=(const QRect &rect)
{
    if (!rect.isEmpty()) {
        requireRegion(pixman_region32_union_rect(&m_region, &m_region,
            rect.x(), rect.y(), unsigned(rect.width()), unsigned(rect.height())));
    }
    return *this;
}

Region &Region::operator-=(const Region &other)
{
    requireRegion(pixman_region32_subtract(&m_region, &m_region, &other.m_region));
    return *this;
}

Region &Region::operator-=(const QRect &rect)
{
    return *this -= Region(rect);
}

Region &Region::operator&=(const Region &other)
{
    requireRegion(pixman_region32_intersect(&m_region, &m_region, &other.m_region));
    return *this;
}

Region &Region::operator&=(const QRect &rect)
{
    if (rect.isEmpty()) {
        clear();
    } else {
        requireRegion(pixman_region32_intersect_rect(&m_region, &m_region,
            rect.x(), rect.y(), unsigned(rect.width()), unsigned(rect.height())));
    }
    return *this;
}

bool Region::operator==(const Region &other) const
{
    return pixman_region32_equal(&m_region, &other.m_region);
}

Region operator+(Region lhs, const Region &rhs)
{
    lhs += rhs;
    return lhs;
}

Region operator+(Region lhs, const QRect &rhs)
{
    lhs += rhs;
    return lhs;
}

Region operator-(Region lhs, const Region &rhs)
{
    lhs -= rhs;
    return lhs;
}

Region operator-(Region lhs, const QRect &rhs)
{
    lhs -= rhs;
    return lhs;
}

Region operator&(Region lhs, const Region &rhs)
{
    lhs &= rhs;
    return lhs;
}

Region operator&(Region lhs, const QRect &rhs)
{
    lhs &= rhs;
    return lhs;
}

Region mapRegionOuter(const QTransform &transform, const Region &local)
{
    if (local.isEmpty())
        return {};
    if (transform.isIdentity())
        return local;
    if (transform.type() <= QTransform::TxTranslate) {
        const qreal dx = transform.dx();
        const qreal dy = transform.dy();
        if (std::floor(dx) == dx && std::floor(dy) == dy)
            return local.translated(int(dx), int(dy));
    }
    if (!isAxisAligned(transform))
        return Region(mapOuter(transform, QRectF(local.boundingRect())));

    Region output;
    int count = 0;
    const pixman_box32_t *boxes = local.rectangles(&count);
    for (int i = 0; i < count; ++i) {
        const QRect rect(boxes[i].x1, boxes[i].y1,
                         boxes[i].x2 - boxes[i].x1,
                         boxes[i].y2 - boxes[i].y1);
        output += mapOuter(transform, QRectF(rect));
    }
    return output;
}

Region mapRegionInner(const QTransform &transform, const Region &local)
{
    if (local.isEmpty() || !isAxisAligned(transform))
        return {};
    if (transform.isIdentity())
        return local;
    if (transform.type() <= QTransform::TxTranslate) {
        const qreal dx = transform.dx();
        const qreal dy = transform.dy();
        if (std::floor(dx) == dx && std::floor(dy) == dy)
            return local.translated(int(dx), int(dy));
    }

    Region output;
    int count = 0;
    const pixman_box32_t *boxes = local.rectangles(&count);
    for (int i = 0; i < count; ++i) {
        const QRect rect(boxes[i].x1, boxes[i].y1,
                         boxes[i].x2 - boxes[i].x1,
                         boxes[i].y2 - boxes[i].y1);
        const QRect mapped = innerAligned(transform.mapRect(QRectF(rect)));
        output += mapped;
    }
    return output;
}

Region dilateRegion(const Region &region, const QMargins &margins)
{
    if (region.isEmpty())
        return {};
    if (margins.isNull())
        return region;

    Region output;
    int count = 0;
    const pixman_box32_t *boxes = region.rectangles(&count);
    for (int i = 0; i < count; ++i) {
        const QRect rect(boxes[i].x1 - margins.left(),
                         boxes[i].y1 - margins.top(),
                         boxes[i].x2 - boxes[i].x1 + margins.left() + margins.right(),
                         boxes[i].y2 - boxes[i].y1 + margins.top() + margins.bottom());
        output += rect;
    }
    return output;
}

bool regionContainsRect(const Region &region, const QRect &rect)
{
    if (rect.isEmpty())
        return true;
    const pixman_box32_t box = {
        rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height()
    };
    return pixman_region32_contains_rectangle(region.native(), &box) == PIXMAN_REGION_IN;
}

} // namespace Gdt
