// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file name_forms_table.cpp
/// @brief The table of the forms of a name of the book's dictionary, by case

#include "kalahari/gui/widgets/name_forms_table.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/gui/name_labels.h"

#include <QHeaderView>

namespace kalahari::gui {

namespace {

/// @brief The columns of the table
constexpr int CASE_COLUMN = 0;
constexpr int SINGULAR_COLUMN = 1;
constexpr int PLURAL_COLUMN = 2;

/// @brief @p text with a capital first letter: "Genitive"
QString capitalized(const QString& text) {
    return text.isEmpty() ? text : text.front().toUpper() + text.mid(1);
}

}  // namespace

NameFormsTable::NameFormsTable(const core::NameDeclension& declension, QWidget* parent)
    : QTableWidget(parent), m_cases(declension.cases()) {
    setObjectName(QStringLiteral("nameFormsTable"));
    //: The table of the forms of a name in its window. In Polish: Formy
    setAccessibleName(tr("Forms"));
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                    QAbstractItemView::AnyKeyPressed | QAbstractItemView::SelectedClicked);
    setTabKeyNavigation(false);  // Tab goes to the next field of the window
    setWordWrap(false);
    verticalHeader()->hide();
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    horizontalHeader()->setHighlightSections(false);
    connect(this, &QTableWidget::itemChanged, this, &NameFormsTable::onItemChanged);
    fill();
}

void NameFormsTable::setVariant(const core::NameVariant& variant) {
    m_variant = variant;
    fill();
}

void NameFormsTable::setPluralShown(bool shown) {
    if (m_pluralShown == shown) {
        return;
    }
    m_pluralShown = shown;
    fill();
}

void NameFormsTable::setEditable(bool editable) {
    m_editable = editable;
    fill();
}

QString NameFormsTable::cellText(const QStringList& forms) {
    return forms.join(QStringLiteral(", "));
}

QStringList NameFormsTable::formsOfText(const QString& text) {
    QStringList forms;
    for (const QString& form : text.split(QLatin1Char(','))) {
        const QString written = form.simplified();
        if (!written.isEmpty() && !forms.contains(written)) {
            forms.append(written);
        }
    }
    return forms;
}

void NameFormsTable::fill() {
    m_filling = true;
    const QModelIndex current = currentIndex();
    const bool cases = m_cases.size() > 1;
    setColumnCount(m_pluralShown ? 3 : 2);
    setRowCount(static_cast<int>(m_cases.size()));
    //: The column of the cases in the table of the forms of a name. In Polish: Przypadek
    QStringList headers{cases ? tr("Case") : QString(),
                        //: In Polish: Liczba pojedyncza
                        tr("Singular")};
    if (m_pluralShown) {
        //: In Polish: Liczba mnoga
        headers.append(tr("Plural"));
    }
    setHorizontalHeaderLabels(headers);
    horizontalHeader()->setSectionResizeMode(CASE_COLUMN, QHeaderView::ResizeToContents);

    const Qt::ItemFlags formFlags = m_editable
                                        ? Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                                              Qt::ItemIsEditable
                                        : Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    for (int row = 0; row < rowCount(); ++row) {
        const core::GrammaticalCase grammaticalCase = m_cases.at(row);
        //: The row of a language without cases: the form of the name. In Polish: Forma
        auto* label = new QTableWidgetItem(
            cases ? QStringLiteral("%1 (%2)").arg(
                        capitalized(caseLabel(grammaticalCase, false)),
                        caseQuestion(grammaticalCase))
                  : tr("Form"));
        label->setFlags(Qt::ItemIsEnabled);
        QFont bold = label->font();
        bold.setBold(true);
        label->setFont(bold);
        setItem(row, CASE_COLUMN, label);
        for (int column = SINGULAR_COLUMN; column < columnCount(); ++column) {
            auto* forms = new QTableWidgetItem(
                cellText(m_variant.forms.cell(grammaticalCase, column == PLURAL_COLUMN)));
            forms->setFlags(formFlags);
            setItem(row, column, forms);
        }
    }
    setCurrentIndex(current.isValid() && current.column() < columnCount()
                        ? model()->index(current.row(), current.column())
                        : model()->index(0, SINGULAR_COLUMN));
    m_filling = false;
}

void NameFormsTable::onItemChanged(const QTableWidgetItem* item) {
    if (m_filling || item == nullptr || item->column() == CASE_COLUMN ||
        item->row() >= m_cases.size()) {
        return;
    }
    const core::GrammaticalCase grammaticalCase = m_cases.at(item->row());
    const bool inPlural = item->column() == PLURAL_COLUMN;
    QStringList& forms = m_variant.forms.cell(grammaticalCase, inPlural);
    const QStringList written = formsOfText(item->text());
    if (written == forms) {
        return;
    }
    forms = written;
    const QString key = core::FormTable::cellKey(grammaticalCase, inPlural);
    if (!m_variant.edited.contains(key)) {
        m_variant.edited.append(key);
    }
    emit formsEdited();
}

}  // namespace kalahari::gui
