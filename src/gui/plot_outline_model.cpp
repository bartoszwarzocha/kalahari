// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_outline_model.cpp
/// @brief The outline: the texts of the book in a table, with their description, point of
/// view, place and time of the action, words and state

#include "kalahari/gui/plot_outline_model.h"

#include "kalahari/gui/plot_board.h"

#include <QAbstractItemModel>
#include <QAbstractTableModel>
#include <QChar>
#include <QComboBox>
#include <QCompleter>
#include <QFont>
#include <QLineEdit>
#include <QList>
#include <QLocale>
#include <QModelIndex>
#include <QObject>
#include <QPalette>
#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>

namespace kalahari::gui {

namespace {

/// @brief The states of a text, in their order
const QStringList& statuses() {
    static const QStringList STATUSES{QStringLiteral("draft"), QStringLiteral("revision"),
                                      QStringLiteral("final")};
    return STATUSES;
}

/// @brief A description in one line of a cell
QString oneLine(const QString& text) {
    return text.simplified();
}

using Model = PlotOutlineModel;

/// @brief What a cell shows that has nothing to show: the words and the number of a scene in
/// the plan
constexpr char16_t NOTHING = u'\u2013';

/// @brief Whether @p card is a scene in the plan beside the scenes of its text: its row is in
/// italics, without a number
bool isPlanBesideText(const PlotCard& card) {
    return card.planned && PlotBoard::scenesVariant() == QLatin1String(PlotBoard::SCENES_BOTH);
}

/// @brief Whether cell @p column of @p card is the title of a scene without one: it shows the
/// name of the scene, quietly
bool isUnnamed(const PlotCard& card, int column) {
    return column == Model::TITLE_COLUMN && card.isScene() && card.title.isEmpty();
}

/// @brief Where the text of cell @p column is: the numbers on the right
QVariant alignmentOf(int column) {
    if (column == Model::WORDS_COLUMN || column == Model::NUMBER_COLUMN) {
        return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
    }
    return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
}

/// @brief The cells of @p card that look as they are edited: its description in one line,
/// its point of view, place and time
QVariant plainTextOf(const PlotCard& card, int column) {
    switch (column) {
    case Model::DESCRIPTION_COLUMN:
        return oneLine(card.description);
    case Model::POV_COLUMN:
        return card.plot.pov;
    case Model::PLACE_COLUMN:
        return card.plot.place;
    case Model::TIME_COLUMN:
        return card.plot.time;
    default:
        break;
    }
    return {};
}

/// @brief The number of @p card: "3" for the third text, "3.2" for its second scene
QVariant numberOf(const PlotCard& card) {
    if (isPlanBesideText(card)) {
        return QString(QChar(NOTHING));
    }
    if (card.isScene()) {
        return QStringLiteral("%1.%2").arg(QLocale().toString(card.number),
                                           QLocale().toString(card.scene + 1));
    }
    return card.number;
}

/// @brief What cell @p column of @p card shows
QVariant shownTextOf(const PlotCard& card, int column) {
    switch (column) {
    case Model::NUMBER_COLUMN:
        return numberOf(card);
    case Model::TITLE_COLUMN:
        return isUnnamed(card, column) ? card.kindName : card.title;
    case Model::WORDS_COLUMN:
        return card.planned ? QString(QChar(NOTHING)) : QLocale().toString(card.words);
    case Model::STATUS_COLUMN:
        return card.planned ? PlotBoard::plannedName() : PlotBoard::statusName(card.status);
    default:
        break;
    }
    return plainTextOf(card, column);
}

/// @brief What the editor of cell @p column of @p card starts with
QVariant editTextOf(const PlotCard& card, int column) {
    switch (column) {
    case Model::TITLE_COLUMN:
        return card.title;
    case Model::STATUS_COLUMN:
        return card.status;
    default:
        break;
    }
    return plainTextOf(card, column);
}

/// @brief The tip of cell @p column of @p card: its whole description, the kind of a title
QVariant toolTipOf(const PlotCard& card, int column) {
    if (column == Model::DESCRIPTION_COLUMN && !card.description.isEmpty()) {
        return card.description;
    }
    if (column == Model::TITLE_COLUMN && !card.kindName.isEmpty()) {
        return card.kindName;
    }
    return {};
}

/// @brief @p font in bold or in italics
QFont fontOf(bool bold, bool italic) {
    QFont font;
    font.setBold(bold);
    font.setItalic(italic);
    return font;
}

/// @brief The color of the cell of @p card in @p column: muted for a field without a name and
/// for the state of a scene in the plan; none: the color of the table
QVariant foregroundOf(const PlotCard& card, int column) {
    if (isUnnamed(card, column) || (card.planned && column == Model::STATUS_COLUMN)) {
        return QPalette().brush(QPalette::PlaceholderText);
    }
    return {};
}

/// @brief The font of the cell of @p card in @p column: italics for a field without a name and
/// for a scene in the plan among the scenes of the text; none: the font of the table
QVariant cellFontOf(const PlotCard& card, int column) {
    if (isUnnamed(card, column) || isPlanBesideText(card)) {
        return fontOf(false, true);
    }
    return {};
}

}  // namespace

// =============================================================================
// PlotOutlineModel
// =============================================================================

PlotOutlineModel::PlotOutlineModel(QObject* parent) : QAbstractTableModel(parent) {}

void PlotOutlineModel::setBands(const QList<PlotBand>& bands) {
    m_bands = bands;
    fill();
}

void PlotOutlineModel::setShowScenes(bool show) {
    if (m_showScenes == show) {
        return;
    }
    m_showScenes = show;
    fill();
}

QList<PlotOutlineModel::Row> PlotOutlineModel::rowsOf() const {
    QList<Row> rows;
    for (const PlotBand& band : m_bands) {
        if (!band.partId.isEmpty()) {
            Row row;
            row.part = true;
            row.partId = band.partId;
            row.partTitle = band.title;
            row.partTexts = static_cast<int>(band.cards.size());
            row.partWords = band.words();
            rows << row;
        }
        for (const PlotCard& card : band.cards) {
            Row row;
            row.card = card;
            rows << row;
            for (qsizetype s = 0; m_showScenes && s < card.scenes.size(); ++s) {
                Row scene;
                scene.card = card.scenes.at(s);
                rows << scene;
            }
        }
    }
    return rows;
}

void PlotOutlineModel::fill() {
    const QList<Row> rows = rowsOf();
    // The same rows only change their cells: the table keeps its cell and its editor
    const bool sameRows = std::ranges::equal(rows, m_rows, [](const Row& one, const Row& other) {
        return one.part == other.part && one.partId == other.partId &&
               one.card.id == other.card.id;
    });
    if (!sameRows) {
        beginResetModel();
        m_rows = rows;
        endResetModel();
        return;
    }
    m_rows = rows;
    if (!m_rows.isEmpty()) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, COLUMN_COUNT - 1));
    }
}

void PlotOutlineModel::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
}

int PlotOutlineModel::rowOf(const QString& id) const {
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        const Row& row = m_rows.at(i);
        if ((row.part && row.partId == id) || (!row.part && row.card.id == id)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

QString PlotOutlineModel::cardIdAt(int row) const {
    if (row < 0 || row >= m_rows.size() || m_rows.at(row).part) {
        return {};
    }
    return m_rows.at(row).card.id;
}

int PlotOutlineModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int PlotOutlineModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

QVariant PlotOutlineModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case ID_ROLE:
        return row.part ? row.partId : row.card.id;
    case PART_ROLE:
        return row.part;
    case Qt::TextAlignmentRole:
        return alignmentOf(index.column());
    default:
        break;
    }
    return row.part ? partData(row, index.column(), role)
                    : cardData(row.card, index.column(), role);
}

QVariant PlotOutlineModel::partData(const Row& row, int column, int role) {
    switch (role) {
    case Qt::FontRole:
        return fontOf(true, false);
    case Qt::BackgroundRole:
        return QPalette().brush(QPalette::AlternateBase);
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
        if (column == TITLE_COLUMN) {
            return row.partTitle;
        }
        if (column == WORDS_COLUMN) {
            return QLocale().toString(row.partWords);
        }
        break;
    default:
        break;
    }
    return {};
}

QVariant PlotOutlineModel::cardData(const PlotCard& card, int column, int role) {
    switch (role) {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
        return shownTextOf(card, column);
    case Qt::EditRole:
        return editTextOf(card, column);
    case Qt::ToolTipRole:
        return toolTipOf(card, column);
    case Qt::ForegroundRole:
        return foregroundOf(card, column);
    case Qt::FontRole:
        return cellFontOf(card, column);
    default:
        break;
    }
    return {};
}

QVariant PlotOutlineModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    const QString title = columnTitle(section);
    return title.isEmpty() ? QVariant() : QVariant(title);
}

const char* PlotOutlineModel::columnName(int column) {
    // In the order of the columns; the title has none
    static constexpr std::array<const char*, COLUMN_COUNT> NAMES{
        {"number", nullptr, PlotBoard::FIELD_DESCRIPTION, PlotBoard::FIELD_POV,
         PlotBoard::FIELD_PLACE, PlotBoard::FIELD_TIME, PlotBoard::FIELD_WORDS,
         PlotBoard::FIELD_STATUS}};
    return column >= 0 && column < COLUMN_COUNT ? NAMES.at(static_cast<std::size_t>(column))
                                                : nullptr;
}

QString PlotOutlineModel::columnTitle(int column) {
    switch (column) {
    case NUMBER_COLUMN:
        //: A column of the outline: the number of the text. In Polish: Nr
        return tr("No.");
    case TITLE_COLUMN:
        //: In Polish: Tytuł
        return tr("Title");
    case DESCRIPTION_COLUMN:
        //: In Polish: Opis
        return tr("Description");
    case POV_COLUMN:
        //: In Polish: Punkt widzenia
        return tr("Point of View");
    case PLACE_COLUMN:
        //: In Polish: Miejsce
        return tr("Place");
    case TIME_COLUMN:
        //: In Polish: Czas akcji
        return tr("Time of the Action");
    case WORDS_COLUMN:
        //: In Polish: Słowa
        return tr("Words");
    case STATUS_COLUMN:
        //: In Polish: Stan
        return tr("State");
    default:
        break;
    }
    return {};
}

Qt::ItemFlags PlotOutlineModel::flags(const QModelIndex& index) const {
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    const Row& row = m_rows.at(index.row());
    // A scene in the plan gets a state once it is written
    const bool editable = index.column() != NUMBER_COLUMN && index.column() != WORDS_COLUMN &&
                          (index.column() != STATUS_COLUMN || !row.card.planned);
    if (!row.part && editable && !m_readOnly) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

bool PlotOutlineModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole || !(flags(index) & Qt::ItemIsEditable)) {
        return false;
    }
    // A copy: the rows are made again while the change is made
    const QString id = m_rows.at(index.row()).card.id;
    const bool scene = m_rows.at(index.row()).card.isScene();
    const QString text = value.toString().trimmed();
    if (text == data(index, Qt::EditRole).toString()) {
        return false;
    }
    // A text keeps its title; a scene may have none
    if (index.column() == TITLE_COLUMN && text.isEmpty() && !scene) {
        return false;
    }
    emit cellEdited(id, index.column(), text);
    return true;
}

// =============================================================================
// PlotOutlineDelegate
// =============================================================================

PlotOutlineDelegate::PlotOutlineDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void PlotOutlineDelegate::setSuggestions(const QStringList& povs, const QStringList& places) {
    m_povs = povs;
    m_places = places;
}

QWidget* PlotOutlineDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                           const QModelIndex& index) const {
    if (index.column() == PlotOutlineModel::STATUS_COLUMN) {
        auto* states = new QComboBox(parent);
        for (const QString& status : statuses()) {
            states->addItem(PlotBoard::statusName(status), status);
        }
        return states;
    }
    QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
    auto* line = qobject_cast<QLineEdit*>(editor);
    const QStringList suggestions = suggestionsFor(index.column());
    if (line != nullptr && !suggestions.isEmpty()) {
        auto* completer = new QCompleter(suggestions, line);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        line->setCompleter(completer);
    }
    return editor;
}

QStringList PlotOutlineDelegate::suggestionsFor(int column) const {
    if (column == PlotOutlineModel::POV_COLUMN) {
        return m_povs;
    }
    if (column == PlotOutlineModel::PLACE_COLUMN) {
        return m_places;
    }
    return {};
}

void PlotOutlineDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    if (auto* states = qobject_cast<QComboBox*>(editor)) {
        states->setCurrentIndex(
            std::max(0, states->findData(index.data(Qt::EditRole).toString())));
        return;
    }
    QStyledItemDelegate::setEditorData(editor, index);
}

void PlotOutlineDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                       const QModelIndex& index) const {
    if (auto* states = qobject_cast<QComboBox*>(editor)) {
        model->setData(index, states->currentData(), Qt::EditRole);
        return;
    }
    QStyledItemDelegate::setModelData(editor, model, index);
}

}  // namespace kalahari::gui
