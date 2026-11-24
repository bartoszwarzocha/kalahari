// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_node_dialog.cpp
/// @brief Node Properties: the name, the kind, the note, the rows, the attributes and the
/// picture of a node of a mind map

#include <kalahari/mindmap/mind_map_node_dialog.h>

#include <kalahari/core/art_provider.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/mindmap/mind_map_content.h>
#include <kalahari/mindmap/mind_map_look.h>

#include <QComboBox>
#include <QCompleter>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace kalahari::mindmap {

namespace {

constexpr int NOTE_LINES = 5;
constexpr int ROW_LINES = 7;
constexpr int ID_ROLE = Qt::UserRole + 1;

}  // namespace

MindMapNodeDialog::MindMapNodeDialog(const MindMap& map, const MindMapNode& node,
                                     const MindMapContent& content, const QString& mapTitle,
                                     const core::ProjectManager* book, QWidget* parent)
    : KalahariDialog(parent), m_node(node), m_attributeNames(map.attributeNames()) {
    const bool root = node.id == map.root();
    MindMapNode without = node;
    without.text.clear();
    m_titleWithout = content.titleOf(map, without, mapTitle);
    const QString title = content.titleOf(map, node, mapTitle);
    //: The window of a node of a mind map. %1: its name. In Polish: Właściwości węzła / Węzeł
    //: „%1”: nazwa, notatka, wiersze z książki, własne atrybuty i obraz.
    setHeading(tr("Node Properties"),
               tr("“%1”: its name, its note, the rows of the book, its own attributes and its "
                  "picture.")
                   .arg(title));
    setHeadingIcon(QString::fromLatin1(MindMapIcons::PROPERTIES));

    m_name = new QLineEdit(this);
    m_name->setText(title);
    m_name->setPlaceholderText(m_titleWithout);
    //: In Polish: Nazwa
    addField(tr("Name"), m_name);

    if (node.isOwn(root) && !root) {
        m_kind = new QComboBox(this);
        m_kind->addItem(MindMapContent::ownKindName(QString::fromLatin1(MindMapNode::IDEA_KIND)),
                        QString());
        for (const char* kind : {MindMapNode::QUESTION_KIND, MindMapNode::NOTE_KIND}) {
            m_kind->addItem(MindMapContent::ownKindName(QString::fromLatin1(kind)),
                            QString::fromLatin1(kind));
        }
        const int index = m_kind->findData(
            node.kind == QLatin1String(MindMapNode::IDEA_KIND) ? QString() : node.kind);
        m_kind->setCurrentIndex(std::max(0, index));
        //: In Polish: Rodzaj
        addField(tr("Kind"), m_kind);
    }

    m_note = new QPlainTextEdit(this);
    m_note->setPlainText(node.note);
    m_note->setTabChangesFocus(true);
    m_note->setMinimumHeight(m_note->fontMetrics().lineSpacing() * NOTE_LINES);
    //: In Polish: Notatka
    addField(tr("Note"), m_note);

    // The rows of the book
    const QList<RowChoice> choices = content.rowChoices(node);
    if (!choices.isEmpty()) {
        m_defaultRows = content.defaultRows(node);
        auto* rows = new QWidget(this);
        auto* rowLine = new QHBoxLayout(rows);
        rowLine->setContentsMargins(0, 0, 0, 0);
        m_rows = new QListWidget(rows);
        m_rows->setMinimumHeight(m_rows->fontMetrics().lineSpacing() * ROW_LINES);
        //: The accessible name of the list of the rows of a node. In Polish: Wiersze węzła
        m_rows->setAccessibleName(tr("Rows of the node"));
        for (const RowChoice& choice : choices) {
            auto* item = new QListWidgetItem(choice.label, m_rows);
            item->setData(ID_ROLE, choice.id);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
        rowLine->addWidget(m_rows, 1);
        auto* buttons = new QVBoxLayout;
        //: In Polish: W górę
        auto* up = new QPushButton(tr("Up"), rows);
        //: In Polish: W dół
        auto* down = new QPushButton(tr("Down"), rows);
        //: The rows a node shows at first. In Polish: Jak na początku
        auto* first = new QPushButton(tr("As at First"), rows);
        buttons->addWidget(up);
        buttons->addWidget(down);
        buttons->addWidget(first);
        buttons->addStretch(1);
        rowLine->addLayout(buttons);
        connect(up, &QPushButton::clicked, this, [this]() { moveRow(-1); });
        connect(down, &QPushButton::clicked, this, [this]() { moveRow(1); });
        connect(first, &QPushButton::clicked, this, [this]() { fillRows(m_defaultRows); });
        fillRows(node.rows.value_or(m_defaultRows));
        //: The rows of the book a node shows. In Polish: Wiersze
        addField(tr("Rows"), rows);
    }

    // The writer's own attributes
    auto* attributes = new QWidget(this);
    auto* attributeColumn = new QVBoxLayout(attributes);
    attributeColumn->setContentsMargins(0, 0, 0, 0);
    m_attributeRows = new QVBoxLayout;
    attributeColumn->addLayout(m_attributeRows);
    //: In Polish: Dodaj atrybut
    m_addAttribute = new QPushButton(tr("Add Attribute"), attributes);
    m_addAttribute->setIcon(
        core::ArtProvider::getInstance().getIcon(QString::fromLatin1(MindMapIcons::ADD)));
    auto* addLine = new QHBoxLayout;
    addLine->addWidget(m_addAttribute);
    addLine->addStretch(1);
    attributeColumn->addLayout(addLine);
    connect(m_addAttribute, &QPushButton::clicked, this, [this]() {
        addAttribute(QString(), QString());
        m_attributes.last().first->setFocus();
    });
    for (const NodeAttribute& attribute : node.attributes) {
        addAttribute(attribute.name, attribute.value);
    }
    //: The writer's own attributes of a node: a name and a value. In Polish: Atrybuty
    addField(tr("Attributes"), attributes);

    // The picture
    if (book != nullptr) {
        m_image = new QComboBox(this);
        //: The picture of a node: as Settings > Mind Maps say (the portraits of the cards). In
        //: Polish: jak w ustawieniach
        m_image->addItem(tr("as the settings say"), QString());
        //: In Polish: bez obrazu
        m_image->addItem(tr("none"), QString::fromLatin1(MindMapNode::NO_IMAGE));
        const core::ProjectElement* element = book->findElement(node.link);
        if (element != nullptr && book->isCard(*element) &&
            node.kind == QLatin1String(MindMapNode::ELEMENT_KIND)) {
            //: In Polish: główny portret karty
            m_image->addItem(tr("the main portrait of the card"),
                             QString::fromLatin1(MindMapNode::PORTRAIT));
        }
        for (const core::ProjectElement* picture : book->pictureMaterials()) {
            m_image->addItem(picture->title, picture->id);
        }
        m_image->setCurrentIndex(std::max(0, m_image->findData(node.image)));
        //: In Polish: Obraz
        addField(tr("Picture"), m_image);
    }

    //: In Polish: Zapisz
    setAcceptText(tr("Save"));
    m_name->selectAll();
    m_name->setFocus();
}

void MindMapNodeDialog::fillRows(const QStringList& shown) {
    // The rows shown first, in their order, then the others
    QList<QListWidgetItem*> items;
    while (m_rows->count() > 0) {
        items.append(m_rows->takeItem(0));
    }
    for (const QString& id : shown) {
        for (qsizetype i = 0; i < items.size(); ++i) {
            if (items.at(i)->data(ID_ROLE).toString() == id) {
                QListWidgetItem* item = items.takeAt(i);
                item->setCheckState(Qt::Checked);
                m_rows->addItem(item);
                break;
            }
        }
    }
    for (QListWidgetItem* item : std::as_const(items)) {
        item->setCheckState(Qt::Unchecked);
        m_rows->addItem(item);
    }
    m_rows->setCurrentRow(0);
}

void MindMapNodeDialog::moveRow(int by) {
    const int row = m_rows->currentRow();
    const int to = row + by;
    if (row < 0 || to < 0 || to >= m_rows->count()) {
        return;
    }
    QListWidgetItem* item = m_rows->takeItem(row);
    m_rows->insertItem(to, item);
    m_rows->setCurrentRow(to);
}

void MindMapNodeDialog::addAttribute(const QString& name, const QString& value) {
    auto* line = new QWidget(this);
    auto* row = new QHBoxLayout(line);
    row->setContentsMargins(0, 0, 0, 0);
    auto* nameEdit = new QLineEdit(name, line);
    //: The name of an attribute of a node. In Polish: Nazwa, np. Motyw
    nameEdit->setPlaceholderText(tr("Name, e.g. Motive"));
    auto* completer = new QCompleter(m_attributeNames, nameEdit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    nameEdit->setCompleter(completer);
    auto* valueEdit = new QLineEdit(value, line);
    //: In Polish: Wartość
    valueEdit->setPlaceholderText(tr("Value"));
    auto* remove = new QToolButton(line);
    remove->setAutoRaise(true);
    remove->setIcon(
        core::ArtProvider::getInstance().getIcon(QString::fromLatin1(MindMapIcons::REMOVE)));
    //: In Polish: Usuń atrybut
    remove->setToolTip(tr("Delete the Attribute"));
    remove->setAccessibleName(remove->toolTip());
    row->addWidget(nameEdit, 2);
    row->addWidget(valueEdit, 3);
    row->addWidget(remove);
    m_attributeRows->addWidget(line);
    m_attributes.append(std::pair<QLineEdit*, QLineEdit*>(nameEdit, valueEdit));
    connect(remove, &QToolButton::clicked, this, [this, line, nameEdit]() {
        m_attributes.removeIf([nameEdit](const std::pair<QLineEdit*, QLineEdit*>& each) {
            return each.first == nameEdit;
        });
        line->deleteLater();
    });
}

QList<NodeAttribute> MindMapNodeDialog::attributes() const {
    QList<NodeAttribute> written;
    for (const auto& [name, value] : m_attributes) {
        if (!name->text().trimmed().isEmpty()) {
            NodeAttribute attribute;
            attribute.name = name->text().trimmed();
            attribute.value = value->text().trimmed();
            // What a later version wrote of it stays
            for (const NodeAttribute& before : m_node.attributes) {
                if (before.name == attribute.name) {
                    attribute.extra = before.extra;
                    break;
                }
            }
            written.append(attribute);
        }
    }
    return written;
}

MindMapNode MindMapNodeDialog::node() const {
    MindMapNode changed = m_node;
    const QString name = m_name->text().trimmed();
    changed.text = name == m_titleWithout ? QString() : name;
    if (m_kind != nullptr) {
        changed.kind = m_kind->currentData().toString();
    }
    changed.note = m_note->toPlainText().trimmed();
    if (m_rows != nullptr) {
        QStringList shown;
        for (int i = 0; i < m_rows->count(); ++i) {
            if (m_rows->item(i)->checkState() == Qt::Checked) {
                shown.append(m_rows->item(i)->data(ID_ROLE).toString());
            }
        }
        changed.rows = shown == m_defaultRows ? std::nullopt : std::optional<QStringList>(shown);
    }
    changed.attributes = attributes();
    if (m_image != nullptr) {
        changed.image = m_image->currentData().toString();
    }
    return changed;
}

}  // namespace kalahari::mindmap
