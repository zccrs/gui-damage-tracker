#ifndef GDT_H
#define GDT_H

#include "gdtnode.h"
#include "gdttracker.h"
#include "gdtregion.h"

namespace Gdt {
// Test-only accessor for private Node internals.
class NodeTestAccess {
public:
    static const pixman_region32_t *ownDamage(const Node *n) { return n->m_ownDamage.native(); }
};
} // namespace Gdt

#endif // GDT_H
