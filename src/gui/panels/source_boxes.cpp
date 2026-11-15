// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file source_boxes.cpp
/// @brief The groups of the card of a material that is a source, and of a quote

#include "kalahari/gui/panels/source_boxes.h"

#include "kalahari/core/bibliography.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/source_names.h"

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <utility>

namespace kalahari::gui {

namespace {

using card::buttonRow;
using card::fitRows;
using card::hintLabel;
using card::isListKey;
using card::isOpenKey;
using card::keepIcon;
using card::listButton;
using core::Bibliography;
using core::SourceDescription;

/// The columns of the people of a source
constexpr int NAME_COLUMN = 0;
constexpr int ROLE_COLUMN = 1;

/// The columns of the bookmarks and of the quotes
constexpr int PAGE_COLUMN = 0;
constexpr int NOTE_COLUMN = 1;

/// What a person did (SourceDescription::AUTHOR...; empty: the author), in the role's cell
constexpr int ROLE_ROLE = Qt::UserRole;
/// The fields of a person or a bookmark this version does not know, in the first cell
constexpr int EXTRA_ROLE = Qt::UserRole + 1;
/// The quote of a row of the quotes
constexpr int ID_ROLE = Qt::UserRole + 2;

/// The last page a bookmark can have
constexpr int MAXIMUM_PAGE = 99999;

/// The fields of the descriptions before the fields of the kind: the kind and the people
constexpr int FIXED_ROWS = 2;

/// A table of the card: one row selected, its cells edited with F2 or by typing
QTableWidget* cardTable(QWidget* parent, const char* name, const QString& accessibleName,
                        const QStringList& columns, bool editable) {
    auto* table = new QTableWidget(0, static_cast<int>(columns.size()), parent);
    table->setObjectName(QString::fromLatin1(name));
    table->setAccessibleName(accessibleName);
    table->setHorizontalHeaderLabels(columns);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(editable ? QAbstractItemView::SelectItems
                                         : QAbstractItemView::SelectRows);
    table->setEditTriggers(editable ? QAbstractItemView::DoubleClicked |
                                          QAbstractItemView::EditKeyPressed |
                                          QAbstractItemView::AnyKeyPressed
                                    : QAbstractItemView::NoEditTriggers);
    table->setWordWrap(false);
    return table;
}

/// The keys of a table that edits: none in a book open read-only
QAbstractItemView::EditTriggers editTriggers(bool readOnly) {
    return readOnly ? QAbstractItemView::NoEditTriggers
                    : QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                          QAbstractItemView::AnyKeyPressed;
}

/// Whether a cell of @p view is being edited: its editor has the keys
bool isEditing(const QAbstractItemView* view) {
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && focus != view && view->isAncestorOf(focus);
}

/// A group of the card, with its margins
QVBoxLayout* groupLayout(QGroupBox* group) {
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(11, 6, 11, 11);
    layout->setSpacing(6);
    return layout;
}

/// A form of a group: the labels at the left of their fields
QFormLayout* groupForm() {
    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
}

/// The role of a person: a list of what a person of a source can have done
class RoleDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        if (index.column() != ROLE_COLUMN) {
            return QStyledItemDelegate::createEditor(parent, option, index);
        }
        auto* box = new QComboBox(parent);
        box->setObjectName(QStringLiteral("sourceRoleEditor"));
        for (const QString& role : SourceDescription::roles()) {
            box->addItem(SourceNames::roleName(role), role);
        }
        return box;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto* box = qobject_cast<QComboBox*>(editor);
        if (box == nullptr) {
            QStyledItemDelegate::setEditorData(editor, index);
            return;
        }
        const QString role = roleOf(index);
        int at = box->findData(role);
        if (at < 0) {
            box->addItem(SourceNames::roleName(role), role);
            at = box->count() - 1;
        }
        box->setCurrentIndex(at);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override {
        const auto* box = qobject_cast<QComboBox*>(editor);
        if (box == nullptr) {
            QStyledItemDelegate::setModelData(editor, model, index);
            return;
        }
        const QString role = box->currentData().toString();
        if (role == roleOf(index)) {
            return;
        }
        model->setItemData(index, {{Qt::DisplayRole, SourceNames::roleName(role)},
                                   {ROLE_ROLE, role}});
    }

private:
    /// The role of the person of @p index: the author for none
    static QString roleOf(const QModelIndex& index) {
        const QString role = index.data(ROLE_ROLE).toString();
        return role.isEmpty() ? QString::fromLatin1(SourceDescription::AUTHOR) : role;
    }
};

/// The page of a bookmark: from 1 to MAXIMUM_PAGE
class PageDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
            spin->setRange(1, MAXIMUM_PAGE);
        }
        return editor;
    }
};

/// A cell that shows @p text and is not edited
QTableWidgetItem* shownItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return item;
}

/// A cell of a page, at the right
QTableWidgetItem* pageItem(int page) {
    auto* item = new QTableWidgetItem();
    item->setData(Qt::EditRole, page);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

/// The title of @p element as a source: that of its description, else its own
QString sourceTitle(const core::ProjectElement& element) {
    const QString own = element.source.value(SourceDescription::TITLE);
    return own.isEmpty() ? element.title : own;
}

}  // namespace

// =============================================================================
// SourceDescriptionBox
// =============================================================================

SourceDescriptionBox::SourceDescriptionBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_book(book) {
    setObjectName(QStringLiteral("sourceDescription"));
    //: The group of the card of a material with its author, title, publisher... In Polish:
    //: Opis bibliograficzny
    setTitle(tr("Bibliographic Description"));
    QVBoxLayout* layout = groupLayout(this);
    m_form = groupForm();
    layout->addLayout(m_form);

    m_kindBox = new QComboBox(this);
    m_kindBox->setObjectName(QStringLiteral("sourceKindBox"));
    //: In Polish: Rodzaj źródła:
    m_form->addRow(tr("Kind of source:"), m_kindBox);

    // The people: a name and what each did
    auto* people = new QWidget(this);
    auto* peopleLayout = new QVBoxLayout(people);
    peopleLayout->setContentsMargins(0, 0, 0, 0);
    peopleLayout->setSpacing(4);
    //: In Polish: Dodaj osobę (Insert)
    m_addPersonButton = listButton(people, "common.add", tr("Add Person (Insert)"),
                                   "sourceAddPerson");
    //: In Polish: Usuń osobę (Delete)
    m_removePersonButton = listButton(people, "common.remove", tr("Remove Person (Delete)"),
                                      "sourceRemovePerson");
    peopleLayout->addLayout(buttonRow(m_addPersonButton, m_removePersonButton));
    m_peopleTable = cardTable(people, "sourcePeopleTable",
                              //: In Polish: Osoby źródła
                              tr("People of the source"),
                              //: The columns of the people of a source. In Polish: Imię i
                              //: nazwisko
                              {tr("Name"),
                               //: What a person of a source did. In Polish: Rola
                               tr("Role")},
                              true);
    m_peopleTable->horizontalHeader()->setSectionResizeMode(NAME_COLUMN, QHeaderView::Stretch);
    m_peopleTable->horizontalHeader()->setSectionResizeMode(ROLE_COLUMN,
                                                            QHeaderView::ResizeToContents);
    m_peopleTable->setItemDelegate(new RoleDelegate(m_peopleTable));
    //: In Polish: Nazwisko z imieniem („Anna Wiśniewska” albo „Wiśniewska, Anna”) albo
    //: nazwa instytucji
    m_peopleTable->setToolTip(tr("A name (\"Anna Wiśniewska\" or \"Wiśniewska, Anna\") or the "
                                 "name of an institution"));
    m_peopleTable->installEventFilter(this);
    peopleLayout->addWidget(m_peopleTable);
    //: In Polish: Brak osób. „+” albo Insert dodaje autora, redakcję, tłumaczenie...
    m_noPeopleLabel = hintLabel(people,
                                tr("No people. \"+\" or Insert adds an author, an editor, a "
                                   "translator..."),
                                "sourceNoPeople");
    peopleLayout->addWidget(m_noPeopleLabel);
    //: The authors, editors, translators of a source. In Polish: Osoby:
    m_form->addRow(tr("People:"), people);

    // The fields of every kind; showKind() puts those of the kind chosen in the form
    for (const QString& field : SourceDescription::allFields()) {
        auto* edit = new QLineEdit(this);
        edit->setObjectName(QStringLiteral("sourceField_") + field);
        auto* label = new QLabel(this);
        label->setBuddy(edit);
        m_edits.insert(field, edit);
        m_labels.insert(field, label);
        edit->hide();
        label->hide();
        connect(edit, &QLineEdit::editingFinished, this, &SourceDescriptionBox::save);
    }

    //: In Polish: Wklej opis (BibTeX, RIS)
    m_pasteButton = new QPushButton(tr("Paste Description (BibTeX, RIS)"), this);
    m_pasteButton->setObjectName(QStringLiteral("sourcePasteButton"));
    keepIcon(m_pasteButton, QStringLiteral("edit.paste"), core::IconContext::Button);
    //: In Polish: Wkleja opis skopiowany z katalogu biblioteki albo z programu do bibliografii
    m_pasteButton->setToolTip(tr("Pastes a description copied from a library catalog or from a "
                                 "bibliography manager"));
    auto* pasteRow = new QHBoxLayout();
    pasteRow->addWidget(m_pasteButton);
    pasteRow->addStretch(1);
    layout->addLayout(pasteRow);

    connect(m_kindBox, &QComboBox::activated, this, [this]() {
        showKind();
        save();
    });
    connect(m_addPersonButton, &QToolButton::clicked, this, &SourceDescriptionBox::addPerson);
    connect(m_removePersonButton, &QToolButton::clicked, this,
            &SourceDescriptionBox::removePerson);
    connect(m_pasteButton, &QPushButton::clicked, this, &SourceDescriptionBox::pasteDescription);
    connect(m_peopleTable, &QTableWidget::itemChanged, this, [this]() {
        if (!m_updating) {
            save();
        }
    });
    connect(m_peopleTable, &QTableWidget::currentCellChanged, this,
            &SourceDescriptionBox::updateButtons);
    // A new person left without a name goes
    connect(m_peopleTable->itemDelegate(), &QAbstractItemDelegate::closeEditor, this,
            [this](QWidget*, QAbstractItemDelegate::EndEditHint hint) {
                if (hint == QAbstractItemDelegate::EditNextItem ||
                    hint == QAbstractItemDelegate::EditPreviousItem) {
                    return;
                }
                const int row = m_peopleTable->currentRow();
                const QTableWidgetItem* name =
                    row >= 0 ? m_peopleTable->item(row, NAME_COLUMN) : nullptr;
                if (row >= 0 && (name == nullptr || name->text().trimmed().isEmpty())) {
                    m_peopleTable->removeRow(row);
                    fitRows(m_peopleTable);
                    save();
                    updateButtons();
                }
            });
    // The description changed elsewhere: on the other card of the material, or pasted
    connect(&m_book, &core::ProjectManager::sourcesChanged, this, [this]() {
        if (const core::ProjectElement* element = m_book.findElement(m_elementId)) {
            showMaterial(*element);
        }
    });
    showKind();
    updateButtons();
}

void SourceDescriptionBox::showMaterial(const core::ProjectElement& element) {
    const bool other = element.id != m_elementId;
    m_elementId = element.id;
    m_materialKind = SourceNames::baseKindOf(m_book, element);
    m_source = element.source;
    m_updating = true;

    // The kinds, with one this version does not know as it is written
    QString kind = m_source.type;
    if (kind.isEmpty()) {
        kind = m_source.isEmpty() ? SourceDescription::kindFor(m_materialKind)
                                  : QString::fromLatin1(SourceDescription::BOOK);
    }
    {
        const QSignalBlocker blocker(m_kindBox);
        m_kindBox->clear();
        for (const QString& each : SourceDescription::kinds()) {
            m_kindBox->addItem(SourceNames::kindName(each), each);
        }
        if (m_kindBox->findData(kind) < 0) {
            m_kindBox->addItem(SourceNames::kindName(kind), kind);
        }
        m_kindBox->setCurrentIndex(m_kindBox->findData(kind));
    }

    // The people, unless the table shows them as they are or edits one
    if (other || (!isEditing(m_peopleTable) && shownSource().people != m_source.people)) {
        m_peopleTable->setRowCount(0);
        for (const core::SourcePerson& person : m_source.people) {
            const int row = m_peopleTable->rowCount();
            m_peopleTable->insertRow(row);
            auto* name = new QTableWidgetItem(person.name);
            name->setData(EXTRA_ROLE, person.extra);
            m_peopleTable->setItem(row, NAME_COLUMN, name);
            auto* role = new QTableWidgetItem(SourceNames::roleName(person.role));
            role->setData(ROLE_ROLE, person.role);
            m_peopleTable->setItem(row, ROLE_COLUMN, role);
        }
    }
    fitRows(m_peopleTable);

    for (auto it = m_edits.cbegin(); it != m_edits.cend(); ++it) {
        if (other || !it.value()->hasFocus()) {
            it.value()->setText(m_source.values.value(it.key()));
        }
    }
    // An empty title is the title of the material
    m_edits.value(QString::fromLatin1(SourceDescription::TITLE))->setPlaceholderText(element.title);

    const bool readOnly = m_book.isReadOnly();
    m_kindBox->setEnabled(!readOnly);
    m_peopleTable->setEditTriggers(editTriggers(readOnly));
    for (QLineEdit* edit : std::as_const(m_edits)) {
        edit->setReadOnly(readOnly);
    }
    showKind();
    m_updating = false;
    updateButtons();
}

core::SourceDescription SourceDescriptionBox::shownSource() const {
    SourceDescription source = m_source;
    source.type = m_kindBox->currentData().toString();
    source.people.clear();
    for (int row = 0; row < m_peopleTable->rowCount(); ++row) {
        const QTableWidgetItem* name = m_peopleTable->item(row, NAME_COLUMN);
        const QTableWidgetItem* role = m_peopleTable->item(row, ROLE_COLUMN);
        if (name == nullptr || name->text().trimmed().isEmpty()) {
            continue;
        }
        source.people << core::SourcePerson{name->text().trimmed(),
                                            role != nullptr ? role->data(ROLE_ROLE).toString()
                                                            : QString(),
                                            name->data(EXTRA_ROLE).toJsonObject()};
    }
    for (auto it = m_edits.cbegin(); it != m_edits.cend(); ++it) {
        source.setValue(it.key(), it.value()->text());
    }
    // The kind alone makes no source
    if (source.people.isEmpty() && source.values.isEmpty() && source.bibliography.isEmpty() &&
        source.extra.isEmpty()) {
        source.type.clear();
    }
    return source;
}

bool SourceDescriptionBox::isFieldShown(const QString& field) const {
    const QLineEdit* edit = m_edits.value(field);
    return edit != nullptr && m_form->indexOf(edit) >= 0;
}

void SourceDescriptionBox::showKind() {
    const QString kind = m_kindBox->currentData().toString();
    if (kind == m_shownKind && m_form->rowCount() > FIXED_ROWS) {
        return;
    }
    m_shownKind = kind;
    const QStringList fields = SourceDescription::fieldsOf(kind);
    // The rows of the fields go and come back in the order of the kind
    while (m_form->rowCount() > FIXED_ROWS) {
        const QFormLayout::TakeRowResult row = m_form->takeRow(FIXED_ROWS);
        for (QLayoutItem* item : {row.labelItem, row.fieldItem}) {
            if (item != nullptr && item->widget() != nullptr) {
                item->widget()->hide();
            }
            delete item;
        }
    }
    QWidget* previous = m_peopleTable;
    for (const QString& field : fields) {
        QLineEdit* edit = m_edits.value(field);
        QLabel* label = m_labels.value(field);
        if (edit == nullptr || label == nullptr) {
            continue;
        }
        label->setText(SourceNames::fieldLabel(field, kind));
        m_form->addRow(label, edit);
        label->show();
        edit->show();
        setTabOrder(previous, edit);
        previous = edit;
    }
    setTabOrder(previous, m_pasteButton);
}

void SourceDescriptionBox::updateButtons() {
    const bool editable = !m_elementId.isEmpty() && !m_book.isReadOnly();
    m_addPersonButton->setEnabled(editable);
    m_removePersonButton->setEnabled(editable && m_peopleTable->currentRow() >= 0 &&
                                     m_peopleTable->rowCount() > 0);
    m_pasteButton->setEnabled(editable);
    const bool none = m_peopleTable->rowCount() == 0;
    m_peopleTable->setVisible(!none);
    m_noPeopleLabel->setVisible(none);
}

void SourceDescriptionBox::addPerson() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const bool wasUpdating = m_updating;
    m_updating = true;
    const int row = m_peopleTable->rowCount();
    m_peopleTable->insertRow(row);
    m_peopleTable->setItem(row, NAME_COLUMN, new QTableWidgetItem());
    auto* role = new QTableWidgetItem(SourceNames::roleName(QString()));
    role->setData(ROLE_ROLE, QString());
    m_peopleTable->setItem(row, ROLE_COLUMN, role);
    m_updating = wasUpdating;
    fitRows(m_peopleTable);
    updateButtons();
    m_peopleTable->setFocus(Qt::OtherFocusReason);
    m_peopleTable->setCurrentCell(row, NAME_COLUMN);
    m_peopleTable->editItem(m_peopleTable->item(row, NAME_COLUMN));
}

void SourceDescriptionBox::removePerson() {
    const int row = m_peopleTable->currentRow();
    if (row < 0 || row >= m_peopleTable->rowCount() || m_book.isReadOnly()) {
        return;
    }
    const QTableWidgetItem* name = m_peopleTable->item(row, NAME_COLUMN);
    if (name != nullptr && !name->text().trimmed().isEmpty() &&
        //: In Polish: Usuń osobę
        !dialogs::MessageDialog::confirm(this, tr("Remove Person"),
                                         //: %1: the name. In Polish: Usunąć „%1” z opisu?
                                         tr("Remove \"%1\" from the description?")
                                             .arg(name->text().trimmed()),
                                         //: In Polish: &Usuń
                                         tr("&Remove"))) {
        return;
    }
    const bool wasUpdating = m_updating;
    m_updating = true;
    m_peopleTable->removeRow(row);
    m_updating = wasUpdating;
    fitRows(m_peopleTable);
    save();
    updateButtons();
    if (m_peopleTable->rowCount() > 0) {
        m_peopleTable->setCurrentCell(std::min(row, m_peopleTable->rowCount() - 1), NAME_COLUMN);
        m_peopleTable->setFocus(Qt::OtherFocusReason);
    }
}

void SourceDescriptionBox::pasteDescription() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const QClipboard* clipboard = QGuiApplication::clipboard();
    const std::optional<SourceDescription> pasted =
        Bibliography::parse(clipboard != nullptr ? clipboard->text() : QString());
    //: In Polish: Wklej opis
    const QString title = tr("Paste Description");
    if (!pasted) {
        dialogs::MessageDialog::information(
            this, title,
            //: In Polish: Schowek nie ma opisu w formacie BibTeX ani RIS. Skopiuj opis źródła z
            //: katalogu biblioteki (eksport BibTeX albo RIS) albo z programu do bibliografii.
            tr("The clipboard has no description in BibTeX or RIS. Copy the description of the "
               "source from a library catalog (export as BibTeX or RIS) or from a bibliography "
               "manager."));
        return;
    }
    const SourceDescription shown = shownSource();
    if (!shown.isEmpty() &&
        !dialogs::MessageDialog::confirm(
            this, title,
            //: In Polish: Zastąpić opis bibliograficzny opisem ze schowka?
            tr("Replace the bibliographic description with the one from the clipboard?"),
            //: In Polish: &Zastąp
            tr("&Replace"))) {
        return;
    }
    // Whether the bibliography has it, and what this version does not know, stay
    SourceDescription source = *pasted;
    source.bibliography = shown.bibliography;
    source.extra = shown.extra;
    if (!m_book.setSource(m_elementId, source)) {
        //: In Polish: Nie zapisano opisu bibliograficznego
        dialogs::MessageDialog::warning(this, tr("Bibliographic Description Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    if (const core::ProjectElement* element = m_book.findElement(m_elementId)) {
        showMaterial(*element);
    }
    emit changed(m_elementId);
    //: In Polish: Wklejono opis bibliograficzny
    emit message(tr("Bibliographic description pasted"));
}

void SourceDescriptionBox::save() {
    if (m_updating || m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const SourceDescription source = shownSource();
    if (source == m_source) {
        return;
    }
    if (!m_book.setSource(m_elementId, source)) {
        dialogs::MessageDialog::warning(this, tr("Bibliographic Description Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    m_source = source;
    emit changed(m_elementId);
}

bool SourceDescriptionBox::eventFilter(QObject* watched, QEvent* event) {
    // Insert and Delete of the people; the same keys of the window's commands wait while the
    // table has the keys. A cell being edited has them itself.
    if (watched == m_peopleTable &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        const bool insert = isListKey(key, Qt::Key_Insert);
        const bool remove = isListKey(key, Qt::Key_Delete);
        if (insert || remove) {
            if (event->type() == QEvent::ShortcutOverride) {
                key->accept();
                return true;
            }
            insert ? addPerson() : removePerson();
            return true;
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

// =============================================================================
// BibliographyEntryBox
// =============================================================================

BibliographyEntryBox::BibliographyEntryBox(core::ProjectManager& book, QWidget* parent,
                                           bool compact)
    : QGroupBox(parent), m_book(book), m_compact(compact) {
    setObjectName(compact ? QStringLiteral("bibliographyEntryShort")
                          : QStringLiteral("bibliographyEntry"));
    //: The group with the entry of a source in the bibliography. In Polish: Tak wygląda w
    //: bibliografii
    setTitle(tr("As the Bibliography Shows It"));
    QVBoxLayout* layout = groupLayout(this);

    auto* styleForm = groupForm();
    m_styleBox = new QComboBox(this);
    m_styleBox->setObjectName(QStringLiteral("bibliographyStyleBox"));
    //: In Polish: Wybór pokazuje zapis w innej normie; normę książki zmienisz we
    //: Właściwościach książki.
    m_styleBox->setToolTip(tr("The choice shows the entry in another style; the style of the "
                              "book is in the Properties of the book."));
    //: The style of a bibliography: PN-ISO 690, APA... In Polish: Norma:
    styleForm->addRow(tr("Style:"), m_styleBox);
    layout->addLayout(styleForm);

    m_entryLabel = new QLabel(this);
    m_entryLabel->setObjectName(QStringLiteral("bibliographyEntryLabel"));
    m_entryLabel->setWordWrap(true);
    m_entryLabel->setTextFormat(Qt::RichText);
    m_entryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                          Qt::TextSelectableByKeyboard);
    m_entryLabel->setFrameShape(QFrame::StyledPanel);
    m_entryLabel->setMargin(6);
    m_entryLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    //: In Polish: Zapis w bibliografii
    m_entryLabel->setAccessibleName(tr("Entry in the bibliography"));
    layout->addWidget(m_entryLabel);

    auto* inclusionForm = groupForm();
    m_inclusionBox = new QComboBox(this);
    m_inclusionBox->setObjectName(QStringLiteral("bibliographyInclusionBox"));
    for (const char* inclusion : {SourceDescription::LINKED, SourceDescription::ALWAYS,
                                  SourceDescription::NEVER}) {
        m_inclusionBox->addItem(SourceNames::inclusionName(QString::fromLatin1(inclusion)),
                                QString::fromLatin1(inclusion));
    }
    //: Whether the bibliography of the book has a source. In Polish: W bibliografii:
    inclusionForm->addRow(tr("In the bibliography:"), m_inclusionBox);
    layout->addLayout(inclusionForm);

    //: In Polish: Kopiuj zapis
    m_copyButton = new QPushButton(tr("Copy Entry"), this);
    m_copyButton->setObjectName(QStringLiteral("bibliographyCopyButton"));
    keepIcon(m_copyButton, QStringLiteral("edit.copy"), core::IconContext::Button);
    //: Opens Settings > Materials. In Polish: Ustawienia źródeł
    m_settingsButton = listButton(this, "dock.settings", tr("Source Settings"),
                                  "bibliographySettingsButton");
    auto* copyRow = new QHBoxLayout();
    copyRow->addWidget(m_copyButton);
    copyRow->addStretch(1);
    copyRow->addWidget(m_settingsButton);
    layout->addLayout(copyRow);

    // The short card shows the entry in the style of the book alone
    if (compact) {
        m_styleBox->hide();
        styleForm->setRowVisible(m_styleBox, false);
        inclusionForm->setRowVisible(m_inclusionBox, false);
    }

    connect(m_styleBox, &QComboBox::activated, this, &BibliographyEntryBox::updateEntry);
    connect(m_copyButton, &QPushButton::clicked, this, &BibliographyEntryBox::copyEntry);
    connect(m_settingsButton, &QToolButton::clicked, this,
            &BibliographyEntryBox::requestSettings);
    connect(m_inclusionBox, &QComboBox::activated, this, [this]() {
        if (m_elementId.isEmpty() || m_book.isReadOnly() || m_source.isEmpty()) {
            return;
        }
        SourceDescription source = m_source;
        const QString inclusion = m_inclusionBox->currentData().toString();
        source.bibliography =
            inclusion == QLatin1String(SourceDescription::LINKED) ? QString() : inclusion;
        if (source == m_source) {
            return;
        }
        if (!m_book.setSource(m_elementId, source)) {
            dialogs::MessageDialog::warning(this, tr("Bibliographic Description Not Saved"),
                                            tr("Failed to save changes."));
            return;
        }
        m_source = source;
        emit changed(m_elementId);
    });
    // The description or the style of the book changed
    connect(&m_book, &core::ProjectManager::sourcesChanged, this, [this]() {
        if (const core::ProjectElement* element = m_book.findElement(m_elementId)) {
            showMaterial(*element);
        }
    });
}

void BibliographyEntryBox::showMaterial(const core::ProjectElement& element) {
    const bool other = element.id != m_elementId;
    m_elementId = element.id;
    m_title = element.title;
    m_source = element.source;
    fillStyles(other || m_bookStyle != m_book.bibliographyStyle());
    {
        const QSignalBlocker blocker(m_inclusionBox);
        const int at = m_inclusionBox->findData(m_source.bibliography);
        m_inclusionBox->setCurrentIndex(std::max(at, 0));
    }
    m_inclusionBox->setEnabled(!m_book.isReadOnly() && !m_source.isEmpty());
    updateEntry();
}

QString BibliographyEntryBox::style() const {
    const QString chosen = m_styleBox->currentData().toString();
    return chosen.isEmpty() ? m_book.bibliographyStyle() : chosen;
}

QString BibliographyEntryBox::entryText() const {
    return Bibliography::entry(m_source, m_title, style(), m_book.bookLanguage(), false);
}

void BibliographyEntryBox::fillStyles(bool chooseBook) {
    const QString previous = m_styleBox->currentData().toString();
    m_bookStyle = m_book.bibliographyStyle();
    const QSignalBlocker blocker(m_styleBox);
    m_styleBox->clear();
    for (const QString& each : Bibliography::styles()) {
        const QString name = Bibliography::styleName(each);
        //: The style of the book among the styles of a bibliography: %1 its name. In Polish:
        //: %1 (domyślna książki)
        m_styleBox->addItem(each == m_bookStyle ? tr("%1 (the book's)").arg(name) : name, each);
    }
    const int at = m_styleBox->findData(chooseBook || previous.isEmpty() ? m_bookStyle : previous);
    m_styleBox->setCurrentIndex(std::max(at, 0));
    if (m_compact) {
        //: The short card: %1 the style of the book. In Polish: W bibliografii · %1
        setTitle(tr("In the Bibliography · %1").arg(Bibliography::styleName(m_bookStyle)));
    }
}

void BibliographyEntryBox::updateEntry() {
    const bool empty = m_source.isEmpty();
    if (empty) {
        m_entryLabel->setTextFormat(Qt::PlainText);
        //: In Polish: Uzupełnij opis bibliograficzny, a tu pojawi się zapis w bibliografii.
        m_entryLabel->setText(tr("Fill in the bibliographic description, and its entry in the "
                                 "bibliography shows here."));
        m_entryLabel->setForegroundRole(QPalette::PlaceholderText);
    } else {
        m_entryLabel->setTextFormat(Qt::RichText);
        m_entryLabel->setText(
            Bibliography::entry(m_source, m_title, style(), m_book.bookLanguage(), true));
        m_entryLabel->setForegroundRole(QPalette::WindowText);
    }
    m_copyButton->setEnabled(!empty);
}

void BibliographyEntryBox::copyEntry() {
    if (m_source.isEmpty()) {
        return;
    }
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        return;
    }
    auto* mime = new QMimeData();
    mime->setHtml(Bibliography::entry(m_source, m_title, style(), m_book.bookLanguage(), true));
    mime->setText(entryText());
    clipboard->setMimeData(mime);
    //: %1: the style. In Polish: Skopiowano zapis (%1)
    emit message(tr("Entry copied (%1)").arg(Bibliography::styleName(style())));
}

// =============================================================================
// BookmarksBox
// =============================================================================

BookmarksBox::BookmarksBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_book(book) {
    setObjectName(QStringLiteral("sourceBookmarks"));
    //: Places in the file of a material: a page and a note each. In Polish: Zakładki
    setTitle(tr("Bookmarks"));
    QVBoxLayout* layout = groupLayout(this);
    //: In Polish: Dodaj zakładkę (Insert)
    m_addButton = listButton(this, "common.add", tr("Add Bookmark (Insert)"), "bookmarksAdd");
    //: In Polish: Usuń zakładkę (Delete)
    m_removeButton = listButton(this, "common.remove", tr("Delete Bookmark (Delete)"),
                                "bookmarksRemove");
    layout->addLayout(buttonRow(m_addButton, m_removeButton));
    m_table = cardTable(this, "bookmarksTable",
                        //: In Polish: Zakładki pliku
                        tr("Bookmarks of the file"),
                        //: The columns of the bookmarks. In Polish: Strona
                        {tr("Page"),
                         //: In Polish: Notatka
                         tr("Note")},
                        true);
    m_table->horizontalHeader()->setSectionResizeMode(PAGE_COLUMN, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(NOTE_COLUMN, QHeaderView::Stretch);
    m_table->setItemDelegate(new PageDelegate(m_table));
    m_table->installEventFilter(this);
    layout->addWidget(m_table);
    //: In Polish: Brak zakładek. „+” albo Insert dodaje zakładkę.
    m_emptyLabel = hintLabel(this, tr("No bookmarks. \"+\" or Insert adds a bookmark."),
                             "bookmarksEmpty");
    layout->addWidget(m_emptyLabel);
    connect(m_addButton, &QToolButton::clicked, this, &BookmarksBox::addBookmark);
    connect(m_removeButton, &QToolButton::clicked, this, &BookmarksBox::removeBookmark);
    connect(m_table, &QTableWidget::itemChanged, this, [this]() {
        if (!m_updating) {
            save();
        }
    });
    connect(m_table, &QTableWidget::currentCellChanged, this, &BookmarksBox::updateButtons);
}

void BookmarksBox::showMaterial(const core::ProjectElement& element) {
    const bool other = element.id != m_elementId;
    m_elementId = element.id;
    m_lastPage = element.lastPage;
    if (other || (!isEditing(m_table) && element.bookmarks != m_bookmarks)) {
        m_bookmarks = element.bookmarks;
        m_updating = true;
        m_table->setRowCount(0);
        for (const core::PageBookmark& bookmark : m_bookmarks) {
            const int row = m_table->rowCount();
            m_table->insertRow(row);
            QTableWidgetItem* page = pageItem(bookmark.page);
            page->setData(EXTRA_ROLE, bookmark.extra);
            m_table->setItem(row, PAGE_COLUMN, page);
            m_table->setItem(row, NOTE_COLUMN, new QTableWidgetItem(bookmark.note));
        }
        m_updating = false;
    }
    m_table->setEditTriggers(editTriggers(m_book.isReadOnly()));
    fitRows(m_table);
    updateButtons();
}

void BookmarksBox::updateButtons() {
    const bool editable = !m_elementId.isEmpty() && !m_book.isReadOnly();
    m_addButton->setEnabled(editable);
    m_removeButton->setEnabled(editable && m_table->currentRow() >= 0 &&
                               m_table->rowCount() > 0);
    const bool none = m_table->rowCount() == 0;
    m_table->setVisible(!none);
    m_emptyLabel->setVisible(none);
}

void BookmarksBox::addBookmark() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    m_updating = true;
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, PAGE_COLUMN, pageItem(std::max(m_lastPage, 1)));
    m_table->setItem(row, NOTE_COLUMN, new QTableWidgetItem());
    m_updating = false;
    save();
    fitRows(m_table);
    updateButtons();
    m_table->setFocus(Qt::OtherFocusReason);
    m_table->setCurrentCell(row, PAGE_COLUMN);
    m_table->editItem(m_table->item(row, PAGE_COLUMN));
}

void BookmarksBox::removeBookmark() {
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount() || m_book.isReadOnly()) {
        return;
    }
    const QTableWidgetItem* page = m_table->item(row, PAGE_COLUMN);
    const QTableWidgetItem* note = m_table->item(row, NOTE_COLUMN);
    if (note != nullptr && !note->text().trimmed().isEmpty() &&
        //: In Polish: Usuń zakładkę
        !dialogs::MessageDialog::confirm(
            this, tr("Delete Bookmark"),
            //: %1: the page. In Polish: Usunąć zakładkę strony %1 razem z notatką?
            tr("Delete the bookmark of page %1 with its note?")
                .arg(page != nullptr ? page->text() : QString()),
            //: In Polish: &Usuń
            tr("&Delete"))) {
        return;
    }
    m_updating = true;
    m_table->removeRow(row);
    m_updating = false;
    save();
    fitRows(m_table);
    updateButtons();
    if (m_table->rowCount() > 0) {
        m_table->setCurrentCell(std::min(row, m_table->rowCount() - 1), PAGE_COLUMN);
        m_table->setFocus(Qt::OtherFocusReason);
    }
}

void BookmarksBox::save() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    QList<core::PageBookmark> bookmarks;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem* page = m_table->item(row, PAGE_COLUMN);
        const QTableWidgetItem* note = m_table->item(row, NOTE_COLUMN);
        core::PageBookmark bookmark;
        if (page != nullptr) {
            bookmark.page = std::clamp(page->data(Qt::EditRole).toInt(), 1, MAXIMUM_PAGE);
            bookmark.extra = page->data(EXTRA_ROLE).toJsonObject();
        }
        bookmark.note = note != nullptr ? note->text().trimmed() : QString();
        bookmarks << bookmark;
    }
    if (bookmarks == m_bookmarks) {
        return;
    }
    if (!m_book.setBookmarks(m_elementId, bookmarks)) {
        //: In Polish: Nie zapisano zakładek
        dialogs::MessageDialog::warning(this, tr("Bookmarks Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    m_bookmarks = bookmarks;
    emit changed(m_elementId);
}

bool BookmarksBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_table &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        const bool insert = isListKey(key, Qt::Key_Insert);
        const bool remove = isListKey(key, Qt::Key_Delete);
        if (insert || remove) {
            if (event->type() == QEvent::ShortcutOverride) {
                key->accept();
                return true;
            }
            insert ? addBookmark() : removeBookmark();
            return true;
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

// =============================================================================
// SourceQuotesBox
// =============================================================================

SourceQuotesBox::SourceQuotesBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_book(book) {
    setObjectName(QStringLiteral("sourceQuotes"));
    //: In Polish: Cytaty z tego źródła
    setTitle(tr("Quotes from This Source"));
    QVBoxLayout* layout = groupLayout(this);
    //: In Polish: Nowy cytat z tego źródła... (Insert)
    m_addButton = listButton(this, "common.add", tr("New Quote from This Source... (Insert)"),
                             "sourceQuotesAdd");
    layout->addLayout(buttonRow(m_addButton, nullptr));
    m_table = cardTable(this, "sourceQuotesTable",
                        //: In Polish: Cytaty z tego źródła
                        tr("Quotes from this source"),
                        //: The columns of the quotes of a source. In Polish: Strona
                        {tr("Page"),
                         //: In Polish: Cytat
                         tr("Quote")},
                        false);
    m_table->horizontalHeader()->setSectionResizeMode(PAGE_COLUMN, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(NOTE_COLUMN, QHeaderView::Stretch);
    //: In Polish: Dwuklik albo Enter otwiera cytat
    m_table->setToolTip(tr("A double-click or Enter opens the quote"));
    m_table->installEventFilter(this);
    layout->addWidget(m_table);
    //: In Polish: Brak cytatów. „+” albo Insert dodaje cytat z tego źródła.
    m_emptyLabel = hintLabel(this,
                             tr("No quotes. \"+\" or Insert adds a quote from this source."),
                             "sourceQuotesEmpty");
    layout->addWidget(m_emptyLabel);
    connect(m_addButton, &QToolButton::clicked, this, [this]() {
        if (!m_elementId.isEmpty() && !m_book.isReadOnly()) {
            emit requestNewQuote(m_elementId);
        }
    });
    connect(m_table, &QTableWidget::itemActivated, this, [this](const QTableWidgetItem* item) {
        const QTableWidgetItem* first = m_table->item(item->row(), PAGE_COLUMN);
        if (first != nullptr) {
            emit requestOpenQuote(first->data(ID_ROLE).toString());
        }
    });
    // A quote got this source or lost it
    connect(&m_book, &core::ProjectManager::sourcesChanged, this, [this]() {
        if (const core::ProjectElement* element = m_book.findElement(m_elementId)) {
            showMaterial(*element);
        }
    });
}

void SourceQuotesBox::showMaterial(const core::ProjectElement& element) {
    m_elementId = element.id;
    const QTableWidgetItem* currentItem =
        m_table->currentRow() >= 0 ? m_table->item(m_table->currentRow(), PAGE_COLUMN) : nullptr;
    const QString current = currentItem != nullptr ? currentItem->data(ID_ROLE).toString()
                                                   : QString();
    m_table->setRowCount(0);
    for (const core::ProjectElement* quote : m_book.quotesOf(m_elementId)) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        QTableWidgetItem* page = shownItem(quote->quote.page);
        page->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        page->setData(ID_ROLE, quote->id);
        m_table->setItem(row, PAGE_COLUMN, page);
        const QString words = quote->description.simplified();
        QTableWidgetItem* text = shownItem(words.isEmpty() ? quote->title : words);
        text->setToolTip(quote->description.isEmpty() ? quote->title : quote->description);
        m_table->setItem(row, NOTE_COLUMN, text);
        if (quote->id == current) {
            m_table->setCurrentCell(row, PAGE_COLUMN);
        }
    }
    fitRows(m_table);
    const bool none = m_table->rowCount() == 0;
    m_table->setVisible(!none);
    m_emptyLabel->setVisible(none);
    m_addButton->setEnabled(!m_book.isReadOnly());
}

bool SourceQuotesBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_table &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (event->type() == QEvent::KeyPress && isOpenKey(key)) {
            const int row = m_table->currentRow();
            if (const QTableWidgetItem* first =
                    row >= 0 ? m_table->item(row, PAGE_COLUMN) : nullptr) {
                emit requestOpenQuote(first->data(ID_ROLE).toString());
            }
            return true;
        }
        if (isListKey(key, Qt::Key_Insert)) {
            if (event->type() == QEvent::ShortcutOverride) {
                key->accept();
                return true;
            }
            if (!m_book.isReadOnly()) {
                emit requestNewQuote(m_elementId);
            }
            return true;
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

// =============================================================================
// QuoteSourceBox
// =============================================================================

QuoteSourceBox::QuoteSourceBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_book(book) {
    setObjectName(QStringLiteral("quoteSource"));
    //: Where a quote comes from. In Polish: Skąd
    setTitle(tr("Where It Comes From"));
    QVBoxLayout* layout = groupLayout(this);
    m_form = groupForm();
    layout->addLayout(m_form);

    auto* sourceRow = new QWidget(this);
    auto* sourceLayout = new QHBoxLayout(sourceRow);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    sourceLayout->setSpacing(6);
    m_sourceBox = new QComboBox(sourceRow);
    m_sourceBox->setObjectName(QStringLiteral("quoteSourceBox"));
    //: In Polish: Materiały z opisem bibliograficznym
    m_sourceBox->setToolTip(tr("The materials with a bibliographic description"));
    m_sourceBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_sourceBox->setMinimumContentsLength(16);
    //: In Polish: Otwórz źródło
    m_openSourceButton = new QPushButton(tr("Open Source"), sourceRow);
    m_openSourceButton->setObjectName(QStringLiteral("quoteOpenSourceButton"));
    keepIcon(m_openSourceButton, QStringLiteral("common.openExternal"), core::IconContext::Button);
    sourceLayout->addWidget(m_sourceBox, 1);
    sourceLayout->addWidget(m_openSourceButton);
    //: In Polish: Źródło:
    m_form->addRow(tr("Source:"), sourceRow);

    m_pageEdit = new QLineEdit(this);
    m_pageEdit->setObjectName(QStringLiteral("quotePageEdit"));
    //: The page of a quote, as the writer puts it. In Polish: 45, xii albo 41–42
    m_pageEdit->setPlaceholderText(tr("45, xii or 41–42"));
    //: In Polish: Strona:
    m_form->addRow(tr("Page:"), m_pageEdit);
    m_authorEdit = new QLineEdit(this);
    m_authorEdit->setObjectName(QStringLiteral("quoteAuthorEdit"));
    //: Who said a quote without a source. In Polish: Autor:
    m_form->addRow(tr("Author:"), m_authorEdit);
    m_workEdit = new QLineEdit(this);
    m_workEdit->setObjectName(QStringLiteral("quoteWorkEdit"));
    //: Where a quote without a source was said. In Polish: Dzieło:
    m_form->addRow(tr("Work:"), m_workEdit);
    m_fromSourceLabel = hintLabel(this, QString(), "quoteFromSourceLabel");
    //: In Polish: Autor, dzieło:
    m_form->addRow(tr("Author, work:"), m_fromSourceLabel);
    m_referenceLabel = new QLabel(this);
    m_referenceLabel->setObjectName(QStringLiteral("quoteReferenceLabel"));
    m_referenceLabel->setTextFormat(Qt::PlainText);
    m_referenceLabel->setWordWrap(true);
    m_referenceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                              Qt::TextSelectableByKeyboard);
    //: How the text refers to a quote: (Wiśniewska 2011, s. 45). In Polish: Odnośnik:
    m_form->addRow(tr("Reference:"), m_referenceLabel);

    connect(m_sourceBox, &QComboBox::activated, this, [this]() {
        updateShown();
        save();
    });
    connect(m_openSourceButton, &QPushButton::clicked, this, [this]() {
        const QString source = m_sourceBox->currentData().toString();
        if (m_book.findElement(source) != nullptr) {
            emit requestOpenSource(source, QString());
        }
    });
    for (QLineEdit* edit : {m_pageEdit, m_authorEdit, m_workEdit}) {
        connect(edit, &QLineEdit::editingFinished, this, &QuoteSourceBox::save);
        connect(edit, &QLineEdit::textEdited, this, &QuoteSourceBox::updateShown);
    }
    // The sources changed: their descriptions, a new one, the style of the book
    connect(&m_book, &core::ProjectManager::sourcesChanged, this, [this]() {
        if (const core::ProjectElement* element = m_book.findElement(m_elementId)) {
            showMaterial(*element);
        }
    });
}

void QuoteSourceBox::showMaterial(const core::ProjectElement& element) {
    const bool other = element.id != m_elementId;
    m_elementId = element.id;
    m_quote = element.quote;
    m_updating = true;
    fillSources(m_quote.source);
    const std::initializer_list<std::pair<QLineEdit*, QString>> edits{
        {m_pageEdit, m_quote.page}, {m_authorEdit, m_quote.author}, {m_workEdit, m_quote.work}};
    for (const auto& [edit, text] : edits) {
        if (other || !edit->hasFocus()) {
            edit->setText(text);
        }
    }
    const bool readOnly = m_book.isReadOnly();
    m_sourceBox->setEnabled(!readOnly);
    for (QLineEdit* edit : {m_pageEdit, m_authorEdit, m_workEdit}) {
        edit->setReadOnly(readOnly);
    }
    m_updating = false;
    updateShown();
}

core::QuoteSource QuoteSourceBox::shownQuote() const {
    core::QuoteSource quote = m_quote;
    quote.source = m_sourceBox->currentData().toString();
    quote.page = m_pageEdit->text().trimmed();
    quote.author = m_authorEdit->text().trimmed();
    quote.work = m_workEdit->text().trimmed();
    return quote;
}

void QuoteSourceBox::save() {
    if (m_updating || m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const core::QuoteSource quote = shownQuote();
    if (quote == m_quote) {
        return;
    }
    if (!m_book.setQuoteSource(m_elementId, quote)) {
        //: In Polish: Nie zapisano źródła cytatu
        dialogs::MessageDialog::warning(this, tr("Source of the Quote Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    m_quote = quote;
    emit changed(m_elementId);
}

void QuoteSourceBox::fillSources(const QString& chosen) {
    QList<const core::ProjectElement*> sources = m_book.sources();
    sources.removeIf(
        [this](const core::ProjectElement* each) { return each->id == m_elementId; });
    std::sort(sources.begin(), sources.end(),
              [](const core::ProjectElement* left, const core::ProjectElement* right) {
                  return QString::localeAwareCompare(sourceTitle(*left), sourceTitle(*right)) < 0;
              });
    const QSignalBlocker blocker(m_sourceBox);
    m_sourceBox->clear();
    //: The choice of a quote that comes from no material. In Polish: Bez źródła – autor i
    //: dzieło wpisane niżej
    m_sourceBox->addItem(tr("No source – the author and the work below"), QString());
    for (const core::ProjectElement* source : sources) {
        m_sourceBox->addItem(SourceNames::shortName(source->source, source->title), source->id);
    }
    if (!chosen.isEmpty() && m_sourceBox->findData(chosen) < 0) {
        const core::ProjectElement* material = m_book.findElement(chosen);
        m_sourceBox->addItem(material != nullptr
                                 //: A source of a quote whose description was emptied: %1 its
                                 //: title. In Polish: %1 (bez opisu bibliograficznego)
                                 ? tr("%1 (no bibliographic description)").arg(material->title)
                                 //: In Polish: Nie ma już tego źródła
                                 : tr("The source is gone"),
                             chosen);
    }
    m_sourceBox->setCurrentIndex(std::max(m_sourceBox->findData(chosen), 0));
}

void QuoteSourceBox::updateShown() {
    const core::QuoteSource quote = shownQuote();
    const core::ProjectElement* source =
        quote.source.isEmpty() ? nullptr : m_book.findElement(quote.source);
    const bool none = quote.source.isEmpty();
    m_form->setRowVisible(m_authorEdit, none);
    m_form->setRowVisible(m_workEdit, none);
    m_form->setRowVisible(m_fromSourceLabel, !none);
    m_openSourceButton->setEnabled(source != nullptr);

    const QString language = m_book.bookLanguage();
    QString reference;
    if (source != nullptr) {
        QStringList authors;
        for (const core::SourcePerson& person :
             source->source.peopleOf(SourceDescription::AUTHOR)) {
            authors << person.name;
        }
        const QString title = sourceTitle(*source);
        m_fromSourceLabel->setText(
            authors.isEmpty()
                //: The author and the work of a quote from a source: %1 its title. In
                //: Polish: z karty źródła: „%1”
                ? tr("From the card of the source: “%1”").arg(title)
                //: %1 the authors, %2 the title. In Polish: z karty źródła: %1, „%2”
                : tr("From the card of the source: %1, “%2”")
                      .arg(authors.join(QStringLiteral(", ")), title));
        const QList<const core::ProjectElement*> sources = m_book.sources();
        const auto at = std::find(sources.cbegin(), sources.cend(), source);
        const int number =
            at != sources.cend() ? static_cast<int>(std::distance(sources.cbegin(), at)) + 1 : 1;
        reference = Bibliography::reference(source->source, source->title, quote.page,
                                            Bibliography::settingsReferenceForm(), number,
                                            language);
    } else if (none) {
        reference = Bibliography::reference(quote, language);
    } else {
        m_fromSourceLabel->setText(tr("The source is gone"));
    }
    //: In Polish: Wybierz źródło albo wpisz autora.
    m_referenceLabel->setText(reference.isEmpty() ? tr("Choose a source or type the author.")
                                                  : reference);
    m_referenceLabel->setForegroundRole(reference.isEmpty() ? QPalette::PlaceholderText
                                                            : QPalette::WindowText);
}

}  // namespace kalahari::gui
