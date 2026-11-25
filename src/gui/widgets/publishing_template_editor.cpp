// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file publishing_template_editor.cpp
/// @brief The template of a text for publishers changed in place, beside the text

#include "kalahari/gui/widgets/publishing_template_editor.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_parts.h"

#include <QEvent>
#include <QFormLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

using core::LocalizedText;
using core::PublishingTemplate;
using core::PublishingTemplateField;
using core::PublishingTemplatePoint;
using core::PublishingTexts;

/// The columns of the points
constexpr int POINT_NAME_COLUMN = 0;
constexpr int POINT_HINT_COLUMN = 1;
/// The columns of the fields
constexpr int FIELD_ID_COLUMN = 0;
constexpr int FIELD_NAME_COLUMN = 1;
constexpr int FIELD_HINT_COLUMN = 2;
/// The id of a point, kept in the cell of its name
constexpr int POINT_ID_ROLE = Qt::UserRole;
/// The largest target length
constexpr int MAX_TARGET = 100000;
/// The rows of the tables shown without scrolling
constexpr int TABLE_ROWS = 6;

/// A text of the writer's own template: none for an empty one
LocalizedText ownText(const QString& text) {
    return text.trimmed().isEmpty() ? LocalizedText() : LocalizedText::plain(text.trimmed());
}

/// @p text in the program's language
QString shown(const LocalizedText& text) {
    return text.text(programLanguage());
}

/// A new id of a point, unique among @p taken: "p1", "p2"...
QString newPointId(const QStringList& taken) {
    for (int number = 1;; ++number) {
        const QString id = QStringLiteral("p%1").arg(number);
        if (!taken.contains(id)) {
            return id;
        }
    }
}

/// @p base, or "<base> 2", "<base> 3"... when one of @p own has that name
QString freeName(const QString& base, const QList<PublishingTemplate>& own,
                 const QString& except) {
    auto taken = [&own, &except](const QString& name) {
        return std::any_of(own.cbegin(), own.cend(), [&name, &except](const PublishingTemplate& t) {
            return t.id != except && shown(t.name) == name;
        });
    };
    QString name = base;
    for (int number = 2; taken(name); ++number) {
        name = QStringLiteral("%1 %2").arg(base).arg(number);
    }
    return name;
}

/// A table of the editor with @p columns: rows chosen whole, its cells changed in place
QTableWidget* tableOf(const QStringList& columns, QWidget* parent) {
    auto* table = new QTableWidget(0, static_cast<int>(columns.size()), parent);
    table->setHorizontalHeaderLabels(columns);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                           QAbstractItemView::AnyKeyPressed);
    table->setTabKeyNavigation(false);
    table->setMinimumHeight(table->horizontalHeader()->sizeHint().height() +
                            TABLE_ROWS * table->verticalHeader()->defaultSectionSize());
    return table;
}

/// The text of cell (@p row, @p column) of @p table; empty without one
QString cellText(const QTableWidget& table, int row, int column) {
    const QTableWidgetItem* item = table.item(row, column);
    return item != nullptr ? item->text() : QString();
}

/// A button of the points with the icon @p iconId and the tooltip @p tip
QToolButton* pointButton(const QString& iconId, const QString& tip, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setAutoRaise(true);
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    card::keepIcon(button, iconId, core::IconContext::Menu);
    return button;
}

}  // namespace

PublishingTemplateEditor::PublishingTemplateEditor(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("publishingTemplateEditor"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("publishingTemplateNote"));
    m_note->setWordWrap(true);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_note);

    auto* fields = new QFormLayout();
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("publishingTemplateName"));
    //: The name of a template changed beside a text for publishers. In Polish: N&azwa:
    fields->addRow(tr("N&ame:"), m_nameEdit);
    auto* target = new QHBoxLayout();
    m_targetSpin = new QSpinBox(this);
    m_targetSpin->setObjectName(QStringLiteral("publishingTemplateTarget"));
    m_targetSpin->setRange(0, MAX_TARGET);
    target->addWidget(m_targetSpin, 1);
    m_targetUnit = new QLabel(this);
    target->addWidget(m_targetUnit);
    //: The target length of a template changed beside a text for publishers. In Polish:
    //: Docelowa &długość:
    auto* targetLabel = new QLabel(tr("Target &length:"), this);
    targetLabel->setBuddy(m_targetSpin);
    fields->addRow(targetLabel, target);
    layout->addLayout(fields);

    // The points of a synopsis, with their buttons
    m_pointsArea = new QWidget(this);
    auto* points = new QVBoxLayout(m_pointsArea);
    points->setContentsMargins(0, 0, 0, 0);
    //: Over the points of a template changed beside a synopsis. In Polish: P&unkty:
    auto* pointsLabel = new QLabel(tr("&Points:"), m_pointsArea);
    points->addWidget(pointsLabel);
    //: The columns of the points of a template. In Polish: Punkt, Podpowiedź
    m_pointsTable = tableOf({tr("Point"), tr("Hint")}, m_pointsArea);
    m_pointsTable->setObjectName(QStringLiteral("publishingTemplatePoints"));
    pointsLabel->setBuddy(m_pointsTable);
    points->addWidget(m_pointsTable);
    auto* buttons = new QHBoxLayout();
    //: The buttons of the points of a template. In Polish: Dodaj punkt (Insert), Usuń punkt
    //: (Delete), W górę (Alt+↑), W dół (Alt+↓)
    m_addButton = pointButton(QStringLiteral("common.add"), tr("Add Point (Insert)"), m_pointsArea);
    m_removeButton =
        pointButton(QStringLiteral("common.remove"), tr("Remove Point (Delete)"), m_pointsArea);
    m_upButton =
        pointButton(QStringLiteral("common.arrowUpward"), tr("Up (Alt+Up)"), m_pointsArea);
    m_downButton =
        pointButton(QStringLiteral("common.arrowDownward"), tr("Down (Alt+Down)"), m_pointsArea);
    for (QToolButton* button : {m_addButton, m_removeButton, m_upButton, m_downButton}) {
        buttons->addWidget(button);
    }
    buttons->addStretch(1);
    points->addLayout(buttons);
    layout->addWidget(m_pointsArea);
    m_pointsTable->installEventFilter(this);
    connect(m_addButton, &QToolButton::clicked, this, &PublishingTemplateEditor::addPoint);
    connect(m_removeButton, &QToolButton::clicked, this, &PublishingTemplateEditor::removePoint);
    connect(m_upButton, &QToolButton::clicked, this, [this]() { movePoint(-1); });
    connect(m_downButton, &QToolButton::clicked, this, [this]() { movePoint(1); });
    connect(m_pointsTable, &QTableWidget::currentCellChanged, this,
            &PublishingTemplateEditor::updatePointButtons);

    // The pattern of a sentence, with its fields
    m_patternArea = new QWidget(this);
    auto* pattern = new QFormLayout(m_patternArea);
    pattern->setContentsMargins(0, 0, 0, 0);
    m_patternEdit = new QLineEdit(m_patternArea);
    m_patternEdit->setObjectName(QStringLiteral("publishingTemplatePattern"));
    //: The pattern of a sentence about the book: the fields in braces. In Polish: Gdy {event},
    //: {hero} musi {goal}, zanim {stakes}.
    m_patternEdit->setPlaceholderText(tr("When {event}, {hero} must {goal} before {stakes}."));
    //: The pattern of a template of a sentence changed beside it. In Polish: Wzó&r:
    pattern->addRow(tr("Patte&rn:"), m_patternEdit);
    //: The columns of the fields of a pattern. In Polish: Pole, Co wpisać, Podpowiedź
    m_fieldsTable = tableOf({tr("Field"), tr("What to Type"), tr("Hint")}, m_patternArea);
    m_fieldsTable->setObjectName(QStringLiteral("publishingTemplateFields"));
    //: The fields of the pattern of a sentence. In Polish: Po&la:
    pattern->addRow(tr("Fiel&ds:"), m_fieldsTable);
    layout->addWidget(m_patternArea);
    connect(m_patternEdit, &QLineEdit::textEdited, this, &PublishingTemplateEditor::updateFields);

    auto* actions = new QHBoxLayout();
    actions->addStretch(1);
    //: Keeps the changes of a template. In Polish: Zachowaj (Ctrl+Enter)
    m_keepButton = new QPushButton(tr("Keep (Ctrl+Enter)"), this);
    m_keepButton->setObjectName(QStringLiteral("publishingTemplateKeep"));
    //: Drops the changes of a template. In Polish: Porzuć (Esc)
    m_dropButton = new QPushButton(tr("Drop (Esc)"), this);
    m_dropButton->setObjectName(QStringLiteral("publishingTemplateDrop"));
    actions->addWidget(m_keepButton);
    actions->addWidget(m_dropButton);
    layout->addLayout(actions);
    connect(m_keepButton, &QPushButton::clicked, this, &PublishingTemplateEditor::kept);
    connect(m_dropButton, &QPushButton::clicked, this, &PublishingTemplateEditor::dropped);
}

PublishingTemplateEditor::~PublishingTemplateEditor() = default;

void PublishingTemplateEditor::edit(const PublishingTemplate& templ, bool words) {
    m_template = templ;
    m_words = words;
    if (templ.isOwn()) {
        //: Over a template of the writer changed beside a text. In Polish: Twój szablon –
        //: zmiany obowiązują we wszystkich książkach.
        m_note->setText(tr("Your template – the changes are for all books."));
    } else {
        //: Over a template of a book type changed beside a text. In Polish: Szablon wbudowany
        //: zostaje bez zmian – zmiany zapiszą się jako Twój szablon, dostępny we wszystkich
        //: książkach.
        m_note->setText(tr("A built-in template stays as it is – the changes are kept as your "
                           "template, for all books."));
    }
    m_nameEdit->setText(shown(templ.name));
    m_targetSpin->setValue(words ? templ.targetWords : templ.targetCharacters);
    //: The unit of the target length of a template. In Polish: słów / znaków ze spacjami
    m_targetUnit->setText(words ? tr("words") : tr("characters with spaces"));

    const bool points = PublishingTexts::hasPoints(templ.part) && !templ.eachText;
    m_pointsArea->setVisible(points);
    m_pointsTable->setRowCount(0);
    for (const PublishingTemplatePoint& point : templ.points) {
        insertPointRow(m_pointsTable->rowCount(), shown(point.name), shown(point.hint));
        m_pointsTable->item(m_pointsTable->rowCount() - 1, POINT_NAME_COLUMN)
            ->setData(POINT_ID_ROLE, point.id);
    }
    if (m_pointsTable->rowCount() > 0) {
        m_pointsTable->setCurrentCell(0, POINT_NAME_COLUMN);
    }
    updatePointButtons();

    m_patternArea->setVisible(PublishingTexts::hasPattern(templ.part));
    m_patternEdit->setText(shown(templ.pattern));
    m_fieldsTable->setRowCount(0);
    for (const PublishingTemplateField& field : templ.fields) {
        const int row = m_fieldsTable->rowCount();
        m_fieldsTable->insertRow(row);
        auto* id = new QTableWidgetItem(field.id);
        id->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_fieldsTable->setItem(row, FIELD_ID_COLUMN, id);
        m_fieldsTable->setItem(row, FIELD_NAME_COLUMN, new QTableWidgetItem(shown(field.name)));
        m_fieldsTable->setItem(row, FIELD_HINT_COLUMN, new QTableWidgetItem(shown(field.hint)));
    }
    m_nameEdit->setFocus(Qt::OtherFocusReason);
    m_nameEdit->selectAll();
}

PublishingTemplate PublishingTemplateEditor::edited() const {
    PublishingTemplate templ = m_template;
    templ.name = ownText(m_nameEdit->text());
    (m_words ? templ.targetWords : templ.targetCharacters) = m_targetSpin->value();
    if (m_pointsArea->isVisibleTo(this)) {
        templ.points.clear();
        QStringList ids;
        for (int row = 0; row < m_pointsTable->rowCount(); ++row) {
            const QString name = cellText(*m_pointsTable, row, POINT_NAME_COLUMN).trimmed();
            if (name.isEmpty()) {
                continue;  // a point without a name is none
            }
            QString id =
                m_pointsTable->item(row, POINT_NAME_COLUMN)->data(POINT_ID_ROLE).toString();
            if (id.isEmpty() || ids.contains(id)) {
                id = newPointId(ids + QStringList{id});
            }
            ids << id;
            templ.points.append(
                {id, ownText(name), ownText(cellText(*m_pointsTable, row, POINT_HINT_COLUMN))});
        }
    }
    if (m_patternArea->isVisibleTo(this)) {
        templ.pattern = ownText(m_patternEdit->text());
        templ.fields.clear();
        for (int row = 0; row < m_fieldsTable->rowCount(); ++row) {
            const QString id = cellText(*m_fieldsTable, row, FIELD_ID_COLUMN);
            const QString name = cellText(*m_fieldsTable, row, FIELD_NAME_COLUMN);
            templ.fields.append({id, ownText(name.trimmed().isEmpty() ? id : name),
                                 ownText(cellText(*m_fieldsTable, row, FIELD_HINT_COLUMN))});
        }
    }
    return templ;
}

PublishingTemplate PublishingTemplateEditor::keptTemplate(
    const QList<PublishingTemplate>& own) const {
    PublishingTemplate templ = edited();
    const bool known = templ.isOwn() &&
                       std::any_of(own.cbegin(), own.cend(), [&templ](const PublishingTemplate& t) {
                           return t.id == templ.id;
                       });
    if (!known) {
        if (!templ.isOwn() && shown(templ.name) == shown(m_template.name)) {
            //: The name of the writer's template made of a built-in one. In Polish: %1 – mój
            templ.name = ownText(tr("%1 – mine").arg(shown(m_template.name)));
        }
        templ.package.clear();
        templ.extra = {};
        templ.id = PublishingTexts::newOwnTemplateId(own);
    }
    if (templ.name.isEmpty()) {
        templ.name = m_template.name.isEmpty() ? ownText(tr("New template")) : m_template.name;
    }
    templ.name = ownText(freeName(shown(templ.name), own, known ? templ.id : QString()));
    return templ;
}

PublishingTemplate PublishingTemplateEditor::newTemplate(const QString& part,
                                                         const QList<PublishingTemplate>& own) {
    PublishingTemplate templ;
    templ.part = part;
    //: The name of a new template of the writer. In Polish: Nowy szablon
    templ.name = ownText(freeName(tr("New template"), own, QString()));
    templ.targetCharacters = PublishingTexts::defaultTargetOf(part, false);
    templ.targetWords = PublishingTexts::defaultTargetOf(part, true);
    if (PublishingTexts::hasPoints(part)) {
        //: The name of a new point of a template. In Polish: Punkt %1
        templ.points.append({QStringLiteral("p1"), ownText(tr("Point %1").arg(1)), {}});
    }
    return templ;
}

void PublishingTemplateEditor::insertPointRow(int row, const QString& name, const QString& hint) {
    m_pointsTable->insertRow(row);
    m_pointsTable->setItem(row, POINT_NAME_COLUMN, new QTableWidgetItem(name));
    m_pointsTable->setItem(row, POINT_HINT_COLUMN, new QTableWidgetItem(hint));
}

void PublishingTemplateEditor::addPoint() {
    const int row = m_pointsTable->currentRow() + 1;
    insertPointRow(row, tr("Point %1").arg(m_pointsTable->rowCount() + 1), QString());
    m_pointsTable->setCurrentCell(row, POINT_NAME_COLUMN);
    m_pointsTable->setFocus(Qt::OtherFocusReason);
    m_pointsTable->editItem(m_pointsTable->item(row, POINT_NAME_COLUMN));
    updatePointButtons();
}

void PublishingTemplateEditor::removePoint() {
    const int row = m_pointsTable->currentRow();
    if (row < 0) {
        return;
    }
    m_pointsTable->removeRow(row);
    if (m_pointsTable->rowCount() > 0) {
        m_pointsTable->setCurrentCell(std::min(row, m_pointsTable->rowCount() - 1),
                                      POINT_NAME_COLUMN);
    }
    updatePointButtons();
}

void PublishingTemplateEditor::movePoint(int step) {
    const int row = m_pointsTable->currentRow();
    const int to = row + step;
    if (row < 0 || to < 0 || to >= m_pointsTable->rowCount()) {
        return;
    }
    const int column = std::max(m_pointsTable->currentColumn(), POINT_NAME_COLUMN);
    QList<QTableWidgetItem*> items;
    for (int c = 0; c < m_pointsTable->columnCount(); ++c) {
        items << m_pointsTable->takeItem(row, c);
    }
    m_pointsTable->removeRow(row);
    m_pointsTable->insertRow(to);
    for (int c = 0; c < m_pointsTable->columnCount(); ++c) {
        m_pointsTable->setItem(to, c, items.at(c));
    }
    m_pointsTable->setCurrentCell(to, column);
    updatePointButtons();
}

void PublishingTemplateEditor::updatePointButtons() {
    const int row = m_pointsTable->currentRow();
    const int rows = m_pointsTable->rowCount();
    m_removeButton->setEnabled(row >= 0);
    m_upButton->setEnabled(row > 0);
    m_downButton->setEnabled(row >= 0 && row < rows - 1);
}

void PublishingTemplateEditor::updateFields() {
    // The fields the pattern names; those it had keep their names and hints
    QHash<QString, std::pair<QString, QString>> before;
    for (int row = 0; row < m_fieldsTable->rowCount(); ++row) {
        before.insert(cellText(*m_fieldsTable, row, FIELD_ID_COLUMN),
                      {cellText(*m_fieldsTable, row, FIELD_NAME_COLUMN),
                       cellText(*m_fieldsTable, row, FIELD_HINT_COLUMN)});
    }
    const QSignalBlocker blocker(m_fieldsTable);
    m_fieldsTable->setRowCount(0);
    for (const QString& id : PublishingTexts::fieldsOf(m_patternEdit->text())) {
        const int row = m_fieldsTable->rowCount();
        m_fieldsTable->insertRow(row);
        auto* idItem = new QTableWidgetItem(id);
        idItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_fieldsTable->setItem(row, FIELD_ID_COLUMN, idItem);
        const auto texts = before.value(id, {id, QString()});
        m_fieldsTable->setItem(row, FIELD_NAME_COLUMN, new QTableWidgetItem(texts.first));
        m_fieldsTable->setItem(row, FIELD_HINT_COLUMN, new QTableWidgetItem(texts.second));
    }
}

namespace {

/// Whether @p event is Ctrl+Enter, which keeps the changes
bool isKeep(const QKeyEvent& event) {
    const bool enter = event.key() == Qt::Key_Return || event.key() == Qt::Key_Enter;
    return enter && (event.modifiers() & ~Qt::KeypadModifier) == Qt::ControlModifier;
}

/// Whether @p event is Esc, which drops them
bool isDrop(const QKeyEvent& event) {
    return event.key() == Qt::Key_Escape && event.modifiers() == Qt::NoModifier;
}

}  // namespace

bool PublishingTemplateEditor::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (isKeep(*key) || isDrop(*key)) {
            event->accept();
            return true;
        }
    }
    return QWidget::event(event);
}

void PublishingTemplateEditor::keyPressEvent(QKeyEvent* event) {
    if (isKeep(*event)) {
        emit kept();
        return;
    }
    if (isDrop(*event)) {
        emit dropped();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool PublishingTemplateEditor::eventFilter(QObject* watched, QEvent* event) {
    // While a cell is changed its editor has the keys
    if (watched != m_pointsTable || event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    const auto* key = static_cast<QKeyEvent*>(event);
    const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
    if (modifiers == Qt::AltModifier && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
        movePoint(key->key() == Qt::Key_Up ? -1 : 1);
        return true;
    }
    if (modifiers == Qt::NoModifier && key->key() == Qt::Key_Insert) {
        addPoint();
        return true;
    }
    if (modifiers == Qt::NoModifier && key->key() == Qt::Key_Delete) {
        removePoint();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace kalahari::gui
