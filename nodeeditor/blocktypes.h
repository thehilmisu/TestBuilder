#ifndef NODEEDITOR_BLOCKTYPES_H
#define NODEEDITOR_BLOCKTYPES_H

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace nodeeditor {

// Mime type used when dragging a block from the palette onto the canvas.
inline const char *blockMimeType() { return "application/x-testbuilder-block"; }

struct PortSpec
{
    QString name;
};

struct ParamSpec
{
    enum Type { Text, Integer, Number, Choice, Boolean };

    QString key;
    QString label;
    Type type = Text;
    QVariant defaultValue;
    QStringList choices; // Choice only
    QString suffix;      // Integer / Number only
    int minimum = 0;     // Integer / Number only
    int maximum = 100000;
};

// Static description of one kind of block. Adding a new block to the tool
// means adding one entry to BlockLibrary::blocks() -- nothing else.
struct BlockType
{
    QString id;
    QString title;
    QString category;
    QString description;
    QColor accent;
    QVector<PortSpec> inputs;
    QVector<PortSpec> outputs;
    QVector<ParamSpec> params;
};

class BlockLibrary
{
public:
    static const QVector<BlockType> &blocks();
    static const BlockType *find(const QString &id);
    static QStringList categories();
};

} // namespace nodeeditor

#endif // NODEEDITOR_BLOCKTYPES_H
