#ifndef NODEEDITOR_PROPERTYPANEL_H
#define NODEEDITOR_PROPERTYPANEL_H

#include <QWidget>

class QFormLayout;
class QLabel;
class QLineEdit;
class QVBoxLayout;

namespace nodeeditor {

class NodeItem;

// Editor for the selected block: renames it and edits the parameters declared
// by its BlockType. The form is rebuilt from the spec on every selection.
class PropertyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget *parent = nullptr);

public slots:
    void setNode(nodeeditor::NodeItem *node);
    void refresh();

    // Called before a node is destroyed so the panel never keeps a stale pointer.
    void forgetNode(nodeeditor::NodeItem *node);

private:
    void clearForm();
    void buildForm();

    NodeItem *m_node = nullptr;
    QLabel *m_typeLabel;
    QLabel *m_descriptionLabel;
    QLineEdit *m_titleEdit;
    QFormLayout *m_form;
    QLabel *m_placeholder;
    QWidget *m_editorArea;
};

} // namespace nodeeditor

#endif // NODEEDITOR_PROPERTYPANEL_H
