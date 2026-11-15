// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_outline_model.cpp
/// @brief The outline: the texts of the book in a table, with their description, point of
/// view, place and time of the action, words and state

#include "kalahari/gui/plot_outline_model.h"

#include <QComboBox>
#include <QCompleter>
#include <QFont>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>

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

void PlotOutlineModel::fill() {
    QList<Row> rows;
    for (const PlotBand& band : m_bands) {
        if (!band.partId.isEmpty()) {
            Row row;
            row.part = true;
            row.partId = band.partId;
            row.partTitle = band.title;
            row.partTexts = int(band.cards.size());
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
    // The same rows only change their cells: the table keeps its cell and its editor
    bool sameRows = rows.size() == m_rows.size();
    for (qsizetype i = 0; sameRows && i < rows.size(); ++i) {
        sameRows = rows.at(i).part == m_rows.at(i).part &&
                   rows.at(i).partId == m_rows.at(i).partId &&
                   rows.at(i).card.id == m_rows.at(i).card.id;
    }
    if (sameRows) {
        m_rows = rows;
        if (!m_rows.isEmpty()) {
            emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
        }
        return;
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

void PlotOutlineModel::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
}

int PlotOutlineModel::rowOf(const QString& id) const {
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        const Row& row = m_rows.at(i);
        if ((row.part && row.partId == id) || (!row.part && row.card.id == id)) {
            return int(i);
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
    return parent.isValid() ? 0 : int(m_rows.size());
}

int PlotOutlineModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant PlotOutlineModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const Row& row = m_rows.at(index.row());
    const int column = index.column();
    if (role == ID_ROLE) {
        return row.part ? row.partId : row.card.id;
    }
    if (role == PART_ROLE) {
        return row.part;
    }
    if (role == Qt::TextAlignmentRole) {
        if (column == WordsColumn || column == NumberColumn) {
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (row.part) {
        if (role == Qt::FontRole) {
            QFont font;
            font.setBold(true);
            return font;
        }
        if (role == Qt::BackgroundRole) {
            return QPalette().brush(QPalette::AlternateBase);
        }
        if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole) {
            if (column == TitleColumn) {
                return row.partTitle;
            }
            if (column == WordsColumn) {
                return QLocale().toString(row.partWords);
            }
        }
        return {};
    }
    const PlotCard& card = row.card;
    // A scene without a title shows its name, quietly
    const bool unnamed = card.isScene() && card.title.isEmpty() && column == TitleColumn;
    switch (role) {
    case Qt::DisplayRole:
    case Qt::AccessibleTextRole:
        switch (column) {
        case NumberColumn:
            if (card.isScene()) {
                return QStringLiteral("%1.%2").arg(QLocale().toString(card.number),
                                                   QLocale().toString(card.scene + 1));
            }
            return card.number;
        case TitleColumn:
            return unnamed ? card.kindName : card.title;
        case DescriptionColumn:
            return oneLine(card.description);
        case PovColumn:
            return card.plot.pov;
        case PlaceColumn:
            return card.plot.place;
        case TimeColumn:
            return card.plot.time;
        case WordsColumn:
            return QLocale().toString(card.words);
        case StatusColumn:
            return PlotBoard::statusName(card.status);
        default:
            break;
        }
        break;
    case Qt::EditRole:
        switch (column) {
        case TitleColumn:
            return card.title;
        case DescriptionColumn:
            return oneLine(card.description);
        case PovColumn:
            return card.plot.pov;
        case PlaceColumn:
            return card.plot.place;
        case TimeColumn:
            return card.plot.time;
        case StatusColumn:
            return card.status;
        default:
            break;
        }
        break;
    case Qt::ToolTipRole:
        if (column == DescriptionColumn && !card.description.isEmpty()) {
            return card.description;
        }
        if (column == TitleColumn && !card.kindName.isEmpty()) {
            return card.kindName;
        }
        break;
    case Qt::ForegroundRole:
        if (unnamed) {
            return QPalette().brush(QPalette::PlaceholderText);
        }
        break;
    case Qt::FontRole:
        if (unnamed) {
            QFont font;
            font.setItalic(true);
            return font;
        }
        break;
    default:
        break;
    }
    return {};
}

QVariant PlotOutlineModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case NumberColumn:
        //: A column of the outline: the number of the text. In Polish: Nr
        return tr("No.");
    case TitleColumn:
        //: In Polish: Tytuł
        return tr("Title");
    case DescriptionColumn:
        //: In Polish: Opis
        return tr("Description");
    case PovColumn:
        //: In Polish: Punkt widzenia
        return tr("Point of View");
    case PlaceColumn:
        //: In Polish: Miejsce
        return tr("Place");
    case TimeColumn:
        //: In Polish: Czas akcji
        return tr("Time of the Action");
    case WordsColumn:
        //: In Polish: Słowa
        return tr("Words");
    case StatusColumn:
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
    const bool editable = index.column() != NumberColumn && index.column() != WordsColumn;
    if (!m_rows.at(index.row()).part && editable && !m_readOnly) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

bool PlotOutlineModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole || !(flags(index) & Qt::ItemIsEditable)) {
        return false;
    }
    const PlotCard& card = m_rows.at(index.row()).card;
    const QString text = value.toString().trimmed();
    if (text == data(index, Qt::EditRole).toString()) {
        return false;
    }
    // A text keeps its title; a scene may have none
    if (index.column() == TitleColumn && text.isEmpty() && !card.isScene()) {
        return false;
    }
    emit cellEdited(card.id, index.column(), text);
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
    if (index.column() == PlotOutlineModel::StatusColumn) {
        auto* states = new QComboBox(parent);
        for (const QString& status : statuses()) {
            states->addItem(PlotBoard::statusName(status), status);
        }
        return states;
    }
    QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
    auto* line = qobject_cast<QLineEdit*>(editor);
    const QStringList& suggestions = index.column() == PlotOutlineModel::PovColumn ? m_povs
                                     : index.column() == PlotOutlineModel::PlaceColumn
                                         ? m_places
                                         : QStringList();
    if (line != nullptr && !suggestions.isEmpty()) {
        auto* completer = new QCompleter(suggestions, line);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        line->setCompleter(completer);
    }
    return editor;
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
