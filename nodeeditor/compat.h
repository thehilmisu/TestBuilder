#ifndef NODEEDITOR_COMPAT_H
#define NODEEDITOR_COMPAT_H

#include <QPoint>
#include <QtGlobal>

namespace nodeeditor {

// QMouseEvent/QDropEvent expose the local position under different names in
// Qt5 and Qt6; this keeps the call sites free of #if blocks.
template <class Event>
inline QPoint eventPos(const Event *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

} // namespace nodeeditor

#endif // NODEEDITOR_COMPAT_H
