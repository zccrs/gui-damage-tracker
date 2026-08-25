#ifndef GDT_H
#define GDT_H

#include "gdtnode.h"
#include "gdttracker.h"
#include "gdtregion.h"

namespace Gdt {
// Test-only accessor for private Node internals.
class NodeTestAccess {
public:
    static QRegion ownDamage(const Node *n) { return n->m_ownDamage; }
    static QRegion inducedDamage(const Node *n) { return n->m_inducedDamage; }
};
} // namespace Gdt

#endif // GDT_H
