/****************************************************************************
** Meta object code from reading C++ file 'nodescene.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../nodeeditor/nodescene.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'nodescene.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10nodeeditor9NodeSceneE_t {};
} // unnamed namespace

template <> constexpr inline auto nodeeditor::NodeScene::qt_create_metaobjectdata<qt_meta_tag_ZN10nodeeditor9NodeSceneE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "nodeeditor::NodeScene",
        "graphChanged",
        "",
        "nodeSelected",
        "nodeeditor::NodeItem*",
        "node",
        "nodeAboutToBeRemoved"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'graphChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'nodeSelected'
        QtMocHelpers::SignalData<void(nodeeditor::NodeItem *)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
        // Signal 'nodeAboutToBeRemoved'
        QtMocHelpers::SignalData<void(nodeeditor::NodeItem *)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<NodeScene, qt_meta_tag_ZN10nodeeditor9NodeSceneE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject nodeeditor::NodeScene::staticMetaObject = { {
    QMetaObject::SuperData::link<QGraphicsScene::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10nodeeditor9NodeSceneE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10nodeeditor9NodeSceneE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10nodeeditor9NodeSceneE_t>.metaTypes,
    nullptr
} };

void nodeeditor::NodeScene::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<NodeScene *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->graphChanged(); break;
        case 1: _t->nodeSelected((*reinterpret_cast<std::add_pointer_t<nodeeditor::NodeItem*>>(_a[1]))); break;
        case 2: _t->nodeAboutToBeRemoved((*reinterpret_cast<std::add_pointer_t<nodeeditor::NodeItem*>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (NodeScene::*)()>(_a, &NodeScene::graphChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (NodeScene::*)(nodeeditor::NodeItem * )>(_a, &NodeScene::nodeSelected, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (NodeScene::*)(nodeeditor::NodeItem * )>(_a, &NodeScene::nodeAboutToBeRemoved, 2))
            return;
    }
}

const QMetaObject *nodeeditor::NodeScene::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *nodeeditor::NodeScene::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10nodeeditor9NodeSceneE_t>.strings))
        return static_cast<void*>(this);
    return QGraphicsScene::qt_metacast(_clname);
}

int nodeeditor::NodeScene::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QGraphicsScene::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void nodeeditor::NodeScene::graphChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void nodeeditor::NodeScene::nodeSelected(nodeeditor::NodeItem * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void nodeeditor::NodeScene::nodeAboutToBeRemoved(nodeeditor::NodeItem * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
