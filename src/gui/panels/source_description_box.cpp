// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file source_description_box.cpp
/// @brief The bibliographic description on the card of a material that is a source (the other
/// groups of source_boxes.h are in source_boxes.cpp)

#include "kalahari/gui/panels/source_boxes.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/bibliography.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/source_catalogs.h"
#include "kalahari/core/source_description.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/source_lookup_dialog.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/pdf_files.h"
#include "kalahari/gui/source_names.h"

#include <QAbstractItemDelegate>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace kalahari::gui {

namespace {

using card::buttonRow;
using card::cardTable;
using card::editTriggers;
using card::fitRows;
using card::groupForm;
using card::groupLayout;
using card::hintLabel;
using card::isEditing;
using card::isListKey;
using card::keepIcon;
using card::listButton;
using core::Bibliography;
using core::SourceDescription;

/// The columns of the people of a source
constexpr int NAME_COLUMN = 0;
constexpr int ROLE_COLUMN = 1;

/// What a person did (SourceDescription::AUTHOR...; empty: the author), in the role's cell
constexpr int ROLE_ROLE = Qt::UserRole;
/// The fields of a person this version does not know, in the first cell
constexpr int EXTRA_ROLE = Qt::UserRole + 1;

/// The fields of the descriptions before the fields of the kind: the kind and the people
constexpr int FIXED_ROWS = 2;

/// The space between the buttons and the table of the people
constexpr int PEOPLE_SPACING = 4;

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

/// Fills the empty fields of @p source with what a file says of itself (a source of kind
/// @p kind); false when it fills none
bool fillFromMetaData(SourceDescription& source, const PdfMetaData& meta, const QString& kind) {
    bool filled = false;
    const auto fill = [&source, &filled](const char* field, const QString& text) {
        if (source.value(field).isEmpty() && !text.isEmpty()) {
            source.setValue(QString::fromLatin1(field), text);
            filled = true;
        }
    };
    fill(SourceDescription::TITLE, meta.title);
    fill(SourceDescription::YEAR, meta.year);
    // The pages of a whole work; of a chapter or an article they are its range in the book
    const bool whole = kind == QLatin1String(SourceDescription::BOOK) ||
                       kind == QLatin1String(SourceDescription::THESIS);
    if (meta.pages > 0 && whole) {
        fill(SourceDescription::PAGES, QString::number(meta.pages));
    }
    if (!source.people.isEmpty()) {
        return filled;
    }
    // The authors, one after another with ";" as the files keep several
    for (const QString& name : meta.author.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        if (!name.trimmed().isEmpty()) {
            source.people << core::SourcePerson{
                .name = name.trimmed(),
                .role = QString::fromLatin1(SourceDescription::AUTHOR),
                .extra = {}};
            filled = true;
        }
    }
    return filled;
}

}  // namespace

SourceDescriptionBox::SourceDescriptionBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_book(book), m_kindBox(new QComboBox(this)), m_form(groupForm()) {
    setObjectName(QStringLiteral("sourceDescription"));
    //: The group of the card of a material with its author, title, publisher... In Polish:
    //: Opis bibliograficzny
    setTitle(tr("Bibliographic Description"));
    QVBoxLayout* layout = groupLayout(this);
    layout->addLayout(m_form);

    m_kindBox->setObjectName(QStringLiteral("sourceKindBox"));
    //: In Polish: Rodzaj źródła:
    m_form->addRow(tr("Kind of source:"), m_kindBox);

    // The people: a name and what each did
    auto* people = new QWidget(this);
    auto* peopleLayout = new QVBoxLayout(people);
    peopleLayout->setContentsMargins(0, 0, 0, 0);
    peopleLayout->setSpacing(PEOPLE_SPACING);
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
    //: In Polish: Uzupełnij z pliku
    m_fillButton = new QPushButton(tr("Fill In from File"), this);
    m_fillButton->setObjectName(QStringLiteral("sourceFillButton"));
    keepIcon(m_fillButton, QStringLiteral("material.pdf"), core::IconContext::Button);
    //: In Polish: Wpisuje w puste pola tytuł, autorów, rok i liczbę stron zapisane w pliku PDF
    m_fillButton->setToolTip(tr("Fills the empty fields with the title, the authors, the year "
                                "and the pages the PDF file has"));
    m_fillButton->hide();
    //: In Polish: Opis z sieci (ISBN, DOI)...
    m_lookupButton = new QPushButton(tr("From the Web by ISBN or DOI..."), this);
    m_lookupButton->setObjectName(QStringLiteral("sourceLookupButton"));
    keepIcon(m_lookupButton, QStringLiteral("material.page"), core::IconContext::Button);
    //: In Polish: Wysyła numer ISBN albo DOI do katalogu w sieci i bierze z niego opis źródła
    m_lookupButton->setToolTip(tr("Sends the ISBN or the DOI to a catalog on the web and takes "
                                  "the description of the source from it"));
    auto* pasteRow = new QHBoxLayout();
    pasteRow->addWidget(m_pasteButton);
    pasteRow->addWidget(m_lookupButton);
    pasteRow->addWidget(m_fillButton);
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
    connect(m_lookupButton, &QPushButton::clicked, this, &SourceDescriptionBox::lookUp);
    connect(m_fillButton, &QPushButton::clicked, this, &SourceDescriptionBox::fillFromFile);
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
    // The lookup on the web shows when the Settings allow it
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [box = QPointer<SourceDescriptionBox>(this)](const std::string& key) {
            if (box == nullptr || key != core::SourceCatalogs::LOOKUP_SETTING) {
                return;
            }
            QMetaObject::invokeMethod(
                box,
                [box]() {
                    if (box != nullptr) {
                        box->updateButtons();
                    }
                },
                Qt::QueuedConnection);
        });
    showKind();
    updateButtons();
}

SourceDescriptionBox::~SourceDescriptionBox() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

void SourceDescriptionBox::showMaterial(const core::ProjectElement& element) {
    const bool other = element.id != m_elementId;
    m_elementId = element.id;
    m_materialKind = SourceNames::baseKindOf(m_book, element);
    m_pdf = PdfFiles::previewAvailable() && PdfFiles::isPdf(m_book.filePathOf(element));
    m_source = element.source;
    m_updating = true;

    fillKinds();
    // The people, unless the table shows them as they are or edits one
    if (other || (!isEditing(m_peopleTable) && shownSource().people != m_source.people)) {
        fillPeople();
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

void SourceDescriptionBox::fillKinds() {
    // The kinds, with one this version does not know as it is written
    QString kind = m_source.type;
    if (kind.isEmpty()) {
        kind = m_source.isEmpty() ? SourceDescription::kindFor(m_materialKind)
                                  : QString::fromLatin1(SourceDescription::BOOK);
    }
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

void SourceDescriptionBox::fillPeople() {
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
        source.people << core::SourcePerson{
            .name = name->text().trimmed(),
            .role = role != nullptr ? role->data(ROLE_ROLE).toString() : QString(),
            .extra = name->data(EXTRA_ROLE).toJsonObject()};
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
    setTabOrder(m_pasteButton, m_lookupButton);
    setTabOrder(m_lookupButton, m_fillButton);
}

void SourceDescriptionBox::updateButtons() {
    const bool editable = !m_elementId.isEmpty() && !m_book.isReadOnly();
    m_addPersonButton->setEnabled(editable);
    m_removePersonButton->setEnabled(editable && m_peopleTable->currentRow() >= 0 &&
                                     m_peopleTable->rowCount() > 0);
    m_pasteButton->setEnabled(editable);
    m_lookupButton->setVisible(core::SourceCatalogs::lookupAllowed());
    m_lookupButton->setEnabled(editable);
    m_fillButton->setVisible(m_pdf);
    m_fillButton->setEnabled(editable && m_pdf);
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

void SourceDescriptionBox::lookUp() {
    if (m_elementId.isEmpty() || m_book.isReadOnly() || !core::SourceCatalogs::lookupAllowed()) {
        return;
    }
    const SourceDescription shown = shownSource();
    const QString doi = shown.value(SourceDescription::DOI);
    dialogs::SourceLookupDialog dialog(doi.isEmpty() ? shown.value(SourceDescription::ISBN) : doi,
                                       this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const std::optional<SourceDescription> found = dialog.found();
    if (!found) {
        return;
    }
    if (!m_book.setSource(m_elementId, core::SourceCatalogs::merged(shown, *found))) {
        //: In Polish: Nie zapisano opisu bibliograficznego
        dialogs::MessageDialog::warning(this, tr("Bibliographic Description Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    if (const core::ProjectElement* element = m_book.findElement(m_elementId)) {
        showMaterial(*element);
    }
    emit changed(m_elementId);
    //: %1: the catalog, Google Books, Open Library or doi.org. In Polish: Uzupełniono opis
    //: bibliograficzny z %1
    emit message(tr("Bibliographic description filled in from %1")
                     .arg(core::SourceCatalogs::catalogName(dialog.foundIn())));
}

void SourceDescriptionBox::fillFromFile() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly() || !m_pdf) {
        return;
    }
    //: In Polish: Uzupełnij z pliku
    const QString title = tr("Fill In from File");
    const std::optional<PdfMetaData> meta = PdfFiles::metaData(m_book.filePathOf(*element));
    if (!meta) {
        dialogs::MessageDialog::warning(
            this, title,
            //: In Polish: Nie da się odczytać pliku PDF. Może być uszkodzony albo chroniony
            //: hasłem.
            tr("The PDF file cannot be read. It may be damaged or protected with a password."));
        return;
    }
    // Only the empty fields: what the writer typed stays
    SourceDescription source = shownSource();
    const QString kind = m_kindBox->currentData().toString();
    if (!fillFromMetaData(source, *meta, kind)) {
        dialogs::MessageDialog::information(
            this, title,
            //: In Polish: Nie ma czego uzupełnić: plik nie ma tytułu, autora ani roku albo opis
            //: już je ma.
            tr("Nothing to fill in: the file has no title, author or year, or the description "
               "has them already."));
        return;
    }
    source.type = kind;
    if (!m_book.setSource(m_elementId, source)) {
        dialogs::MessageDialog::warning(this, tr("Bibliographic Description Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    if (const core::ProjectElement* updated = m_book.findElement(m_elementId)) {
        showMaterial(*updated);
    }
    emit changed(m_elementId);
    //: In Polish: Uzupełniono opis z pliku
    emit message(tr("Description filled in from the file"));
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

}  // namespace kalahari::gui
