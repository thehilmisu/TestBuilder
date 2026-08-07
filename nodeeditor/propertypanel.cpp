#include "propertypanel.h"

#include "blocktypes.h"
#include "nodeitem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace nodeeditor {

PropertyPanel::PropertyPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    m_placeholder = new QLabel(tr("Select a block to edit its parameters."), this);
    m_placeholder->setWordWrap(true);
    m_placeholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_placeholder->setEnabled(false);
    layout->addWidget(m_placeholder);

    m_editorArea = new QWidget(this);
    auto *editorLayout = new QVBoxLayout(m_editorArea);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(8);

    m_typeLabel = new QLabel(m_editorArea);
    QFont typeFont = m_typeLabel->font();
    typeFont.setBold(true);
    m_typeLabel->setFont(typeFont);
    editorLayout->addWidget(m_typeLabel);

    m_descriptionLabel = new QLabel(m_editorArea);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setEnabled(false);
    editorLayout->addWidget(m_descriptionLabel);

    m_form = new QFormLayout;
    m_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_titleEdit = new QLineEdit(m_editorArea);
    connect(m_titleEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (m_node)
            m_node->setTitle(text);
    });
    m_form->addRow(tr("Label"), m_titleEdit);

    editorLayout->addLayout(m_form);
    editorLayout->addStretch(1);

    layout->addWidget(m_editorArea);
    layout->addStretch(1);

    m_editorArea->hide();
}

void PropertyPanel::setNode(NodeItem *node)
{
    if (m_node == node) {
        refresh();
        return;
    }
    m_node = node;
    buildForm();
}

void PropertyPanel::refresh()
{
    buildForm();
}

void PropertyPanel::forgetNode(NodeItem *node)
{
    if (m_node == node)
        setNode(nullptr);
}

void PropertyPanel::clearForm()
{
    // Row 0 is the label editor, which is reused across selections.
    while (m_form->rowCount() > 1)
        m_form->removeRow(m_form->rowCount() - 1);
}

void PropertyPanel::buildForm()
{
    clearForm();

    if (!m_node) {
        m_editorArea->hide();
        m_placeholder->show();
        return;
    }

    m_placeholder->hide();
    m_editorArea->show();

    const BlockType *type = m_node->blockType();
    m_typeLabel->setText(type->title);
    m_descriptionLabel->setText(type->description);
    m_titleEdit->setText(m_node->title());

    for (const ParamSpec &spec : type->params) {
        const QVariant value = m_node->param(spec.key);
        const QString key = spec.key;
        QWidget *editor = nullptr;

        switch (spec.type) {
        case ParamSpec::Text: {
            auto *edit = new QLineEdit(value.toString(), m_editorArea);
            connect(edit, &QLineEdit::textEdited, this, [this, key](const QString &text) {
                if (m_node)
                    m_node->setParam(key, text);
            });
            editor = edit;
            break;
        }
        case ParamSpec::Integer: {
            auto *spin = new QSpinBox(m_editorArea);
            spin->setRange(spec.minimum, spec.maximum);
            spin->setSuffix(spec.suffix);
            spin->setValue(value.toInt());
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, key](int v) {
                if (m_node)
                    m_node->setParam(key, v);
            });
            editor = spin;
            break;
        }
        case ParamSpec::Number: {
            auto *spin = new QDoubleSpinBox(m_editorArea);
            spin->setRange(spec.minimum, spec.maximum);
            spin->setSuffix(spec.suffix);
            spin->setValue(value.toDouble());
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                    [this, key](double v) {
                        if (m_node)
                            m_node->setParam(key, v);
                    });
            editor = spin;
            break;
        }
        case ParamSpec::Choice: {
            auto *combo = new QComboBox(m_editorArea);
            combo->addItems(spec.choices);
            combo->setCurrentText(value.toString());
            connect(combo, &QComboBox::currentTextChanged, this, [this, key](const QString &text) {
                if (m_node)
                    m_node->setParam(key, text);
            });
            editor = combo;
            break;
        }
        case ParamSpec::Boolean: {
            auto *check = new QCheckBox(m_editorArea);
            check->setChecked(value.toBool());
            connect(check, &QCheckBox::toggled, this, [this, key](bool on) {
                if (m_node)
                    m_node->setParam(key, on);
            });
            editor = check;
            break;
        }
        }

        if (editor)
            m_form->addRow(spec.label, editor);
    }
}

} // namespace nodeeditor
