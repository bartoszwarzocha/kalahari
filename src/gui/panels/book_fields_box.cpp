// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_fields_box.cpp
/// @brief The group of the Properties panel with the writer's own fields of the book

#include "kalahari/gui/panels/book_fields_box.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/own_fields.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace kalahari::gui {

namespace {

constexpr int GROUP_MARGIN = 11;   ///< Inside the group, as the other groups of the panel
constexpr int FIELD_SPACING = 6;   ///< Between its rows and its buttons
constexpr int GEAR_SIZE = 16;      ///< The gear at the title, as at the group of the versions

/// @brief Whether the row shows a field of the series whose value is that of all the volumes
bool ofSeries(const OwnField& field)
{
    return field.series && !field.ownValue;
}

}  // namespace

BookFieldsBox::BookFieldsBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent)
    , m_book(book)
{
    setObjectName(QStringLiteral("bookFieldsBox"));
    //: The group of the Properties panel with the writer's own fields of the book. In Polish:
    //: Własne pola
    setTitle(tr("Own Fields"));
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(FIELD_SPACING);
    layout->setContentsMargins(GROUP_MARGIN, GROUP_MARGIN, GROUP_MARGIN, GROUP_MARGIN);

    //: In Polish: Książka nie ma jeszcze własnych pól.
    m_empty = new QLabel(tr("The book has no own fields yet."), this);
    m_empty->setObjectName(QStringLiteral("bookFieldsEmpty"));
    m_empty->setWordWrap(true);
    m_empty->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_empty);

    auto* rows = new QWidget(this);
    m_rows = new QFormLayout(rows);
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(FIELD_SPACING);
    layout->addWidget(rows);

    m_seriesNote = new QLabel(this);
    m_seriesNote->setObjectName(QStringLiteral("bookFieldsSeriesNote"));
    m_seriesNote->setWordWrap(true);
    m_seriesNote->setTextFormat(Qt::PlainText);
    m_seriesNote->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_seriesNote);

    // The buttons in two rows on a narrow panel
    auto* buttonRow = new QWidget(this);
    auto* buttons = new FlowLayout(buttonRow, FIELD_SPACING, FIELD_SPACING);
    buttons->setContentsMargins(0, 0, 0, 0);
    //: The button of the group of the own fields, as New Field... in Insert > Field... In
    //: Polish: Dodaj pole...
    m_add = new QPushButton(tr("Add Field..."), buttonRow);
    m_add->setObjectName(QStringLiteral("bookFieldsAddButton"));
    //: The button of the group of the own fields: Insert > Field..., the tab Own. In Polish:
    //: Wstaw do tekstu...
    m_insert = new QPushButton(tr("Insert in Text..."), buttonRow);
    m_insert->setObjectName(QStringLiteral("bookFieldsInsertButton"));
    buttons->addWidget(m_add);
    buttons->addWidget(m_insert);
    layout->addWidget(buttonRow);
    connect(m_add, &QPushButton::clicked, this, &BookFieldsBox::requestNewField);
    connect(m_insert, &QPushButton::clicked, this,
            [this]() { emit requestInsertField(m_lastField); });

    // Its settings: the gear at the end of the line of its title; Tab reaches it after the
    // buttons
    m_settings = new QToolButton(this);
    m_settings->setObjectName(QStringLiteral("bookFieldsSettingsButton"));
    m_settings->setAutoRaise(true);
    m_settings->setFixedSize(GEAR_SIZE, GEAR_SIZE);
    //: The gear at the title of the group of the own fields in the Properties panel: Settings
    //: > Editor > Writing, the background of the fields. In Polish: Ustawienia pól
    m_settings->setToolTip(tr("Field Settings"));
    m_settings->setAccessibleName(m_settings->toolTip());
    connect(m_settings, &QToolButton::clicked, this, &BookFieldsBox::requestSettings);
    updateIcons();
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &BookFieldsBox::updateIcons);

    // A field made, changed or deleted elsewhere (Insert > Field..., another volume)
    connect(&m_book, &core::ProjectManager::customFieldsChanged, this, [this]() {
        if (!m_saving) {
            refresh();
        }
    });
    connect(&m_book, &core::ProjectManager::currentBookChanged, this, &BookFieldsBox::refresh);
    refresh();
}

void BookFieldsBox::refresh()
{
    const QList<OwnField> fields =
        m_book.isProjectOpen() ? own_fields::of(m_book) : QList<OwnField>{};
    const bool same = std::equal(fields.cbegin(), fields.cend(), m_fields.cbegin(),
                                 m_fields.cend(), [](const OwnField& field, const Row& row) {
                                     return field.id == row.id && field.name == row.name &&
                                            ofSeries(field) == row.series;
                                 });
    if (!same) {
        rebuild(fields);
        return;
    }
    // The same fields: only the values, but the one being written
    for (qsizetype i = 0; i < fields.size(); ++i) {
        QLineEdit* edit = m_fields[i].edit;
        if (!edit->hasFocus() && edit->text() != fields[i].value) {
            edit->setText(fields[i].value);
        }
        edit->setToolTip(fields[i].description);
        edit->setReadOnly(m_book.isReadOnly());
    }
    updateButtons();
}

void BookFieldsBox::rebuild(const QList<OwnField>& fields)
{
    // The rows go without keeping what they say: the book changed under them
    for (const Row& row : std::as_const(m_fields)) {
        row.edit->disconnect(this);
        row.edit->removeEventFilter(this);
    }
    while (m_rows->rowCount() > 0) {
        m_rows->removeRow(0);
    }
    m_fields.clear();
    QStringList seriesNames;
    for (const OwnField& field : fields) {
        auto* edit = new QLineEdit(field.value, this);
        edit->setObjectName(QStringLiteral("bookField_") + field.id);
        edit->setToolTip(field.description);
        edit->setAccessibleName(field.name);
        //: The value of an own field without a value, in the Properties panel; %1: what the
        //: text shows. In Polish: bez wartości – w tekście %1
        edit->setPlaceholderText(
            tr("no value – in the text %1").arg(QStringLiteral("[%1]").arg(field.name)));
        edit->setReadOnly(m_book.isReadOnly());
        edit->installEventFilter(this);
        connect(edit, &QLineEdit::editingFinished, this, [this, id = field.id]() { save(id); });
        auto* label = new QLabel(field.name + QLatin1Char(':'), this);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setBuddy(edit);
        label->setToolTip(field.description);
        m_rows->addRow(label, edit);
        m_fields.append({field.id, field.name, ofSeries(field), edit});
        if (ofSeries(field)) {
            //: An own field named in the note of the fields of the series. In Polish: „%1”
            seriesNames.append(tr("“%1”").arg(field.name));
        }
    }
    if (!std::any_of(m_fields.cbegin(), m_fields.cend(),
                     [this](const Row& row) { return row.id == m_lastField; })) {
        m_lastField.clear();
    }
    m_empty->setVisible(fields.isEmpty());
    m_rows->parentWidget()->setVisible(!fields.isEmpty());
    //: The note under the own fields of a volume: %1 the names of the fields of the series. In
    //: Polish: %1 – pole serii: zmiana dotyczy wszystkich tomów. / %1 – pola serii: zmiana
    //: dotyczy wszystkich tomów.
    m_seriesNote->setText(
        tr("%1 – field(s) of the series: a change applies to all the volumes.", nullptr,
           static_cast<int>(seriesNames.size()))
            .arg(QLocale().createSeparatedList(seriesNames)));
    m_seriesNote->setVisible(!seriesNames.isEmpty());
    updateButtons();
}

void BookFieldsBox::setCanInsert(bool can)
{
    m_canInsert = can;
    updateButtons();
}

void BookFieldsBox::updateButtons()
{
    const bool open = m_book.isProjectOpen() && !m_book.isReadOnly();
    m_add->setEnabled(open);
    m_insert->setEnabled(open && m_canInsert && !m_fields.isEmpty());
    if (m_fields.isEmpty()) {
        m_insert->setToolTip(QString());
    } else if (!m_canInsert) {
        //: Why Insert in Text... of the own fields is greyed out. In Polish: Otwórz tekst
        //: książki, aby wstawić do niego pole.
        m_insert->setToolTip(tr("Open a text of the book to insert a field in it."));
    } else {
        //: In Polish: Wstaw pole do bieżącego tekstu (Wstaw > Pole..., karta „Własne”)
        m_insert->setToolTip(
            tr("Insert a field in the text in front (Insert > Field..., the tab Own)"));
    }
}

void BookFieldsBox::save(const QString& id)
{
    const auto row = std::find_if(m_fields.cbegin(), m_fields.cend(),
                                  [&id](const Row& field) { return field.id == id; });
    if (row == m_fields.cend() || m_book.isReadOnly()) {
        return;
    }
    const std::optional<OwnField> field = own_fields::find(m_book, id);
    if (!field || field->value == row->edit->text()) {
        return;
    }
    m_saving = true;
    own_fields::setValue(m_book, id, row->edit->text());
    m_saving = false;
}

QLineEdit* BookFieldsBox::valueEdit(const QString& id) const
{
    for (const Row& row : m_fields) {
        if (row.id == id) {
            return row.edit;
        }
    }
    return nullptr;
}

QStringList BookFieldsBox::fieldIds() const
{
    QStringList ids;
    for (const Row& row : m_fields) {
        ids.append(row.id);
    }
    return ids;
}

bool BookFieldsBox::event(QEvent* event)
{
    const bool done = QGroupBox::event(event);
    // The gear follows the title
    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::FontChange:
    case QEvent::StyleChange:
    case QEvent::LayoutDirectionChange:
        card::placeAtTitle(this, m_settings);
        break;
    default:
        break;
    }
    return done;
}

bool BookFieldsBox::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusIn) {
        for (const Row& row : std::as_const(m_fields)) {
            if (row.edit == watched) {
                m_lastField = row.id;
            }
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

void BookFieldsBox::updateIcons()
{
    m_settings->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("dock.settings")));
}

}  // namespace kalahari::gui
