// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_fields_box.cpp
/// @brief The group of the Properties panel with what the plot board says of a text

#include "kalahari/gui/panels/plot_fields_box.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/plot_board.h"

#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QEvent>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QStringList>
#include <QStringListModel>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace kalahari::gui {

namespace {

constexpr int DESCRIPTION_LINES = 4;     ///< The lines of the description the field shows
constexpr int DESCRIPTION_PADDING = 8;   ///< Inside the field of the description
constexpr int SWATCH_SIZE = 12;          ///< The square of a color on its item
constexpr int GROUP_MARGIN = 11;         ///< Inside the group, as the other groups of the panel
constexpr int FIELD_SPACING = 6;         ///< Between its fields

/// @brief Suggestions for @p line that a few letters of any word filter, from a list that
/// can change
QStringListModel* suggestionsOf(QLineEdit* line) {
    auto* model = new QStringListModel(line);
    auto* completer = new QCompleter(model, line);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    line->setCompleter(completer);
    return model;
}

/// @brief Give @p line the text @p text, unless it has it (its cursor stays)
void setLine(QLineEdit* line, const QString& text) {
    if (line->text() != text) {
        line->setText(text);
    }
}

}  // namespace

PlotFieldsBox::PlotFieldsBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent),
      m_book(book),
      m_description(new QPlainTextEdit(this)),
      m_pov(new QLineEdit(this)),
      m_place(new QLineEdit(this)),
      m_time(new QLineEdit(this)),
      m_color(new QComboBox(this)),
      m_povs(suggestionsOf(m_pov)),
      m_places(suggestionsOf(m_place)) {
    setObjectName(QStringLiteral("plotFieldsBox"));
    //: The group of the Properties panel with what the plan of the book says of a text. In
    //: Polish: Plan książki
    setTitle(tr("Book Plan"));
    auto* layout = new QFormLayout(this);
    layout->setSpacing(FIELD_SPACING);
    layout->setContentsMargins(GROUP_MARGIN, GROUP_MARGIN, GROUP_MARGIN, GROUP_MARGIN);

    m_description->setObjectName(QStringLiteral("plotDescriptionEdit"));
    m_description->setTabChangesFocus(true);
    m_description->setFixedHeight(
        (m_description->fontMetrics().lineSpacing() * DESCRIPTION_LINES) +
        (2 * m_description->frameWidth()) + DESCRIPTION_PADDING);
    //: The short description of a text on its card of the plot board. In Polish: Opis:
    layout->addRow(tr("Description:"), m_description);
    m_pov->setObjectName(QStringLiteral("plotPovEdit"));
    //: In Polish: Punkt widzenia:
    layout->addRow(tr("Point of view:"), m_pov);
    m_place->setObjectName(QStringLiteral("plotPlaceEdit"));
    //: In Polish: Miejsce:
    layout->addRow(tr("Place:"), m_place);
    //: The note under the point of view and the place. In Polish: Podpowiedzi z kart postaci i
    //: miejsc w Warsztacie; można wpisać dowolny tekst.
    auto* hint = new QLabel(tr("Suggestions from the character and place cards of the "
                               "Workshop; any text can be written."),
                            this);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    layout->addRow(hint);
    m_time->setObjectName(QStringLiteral("plotTimeEdit"));
    //: An example of the time of the action. In Polish: np. Dzień 3, świt
    m_time->setPlaceholderText(tr("e.g. Day 3, dawn"));
    //: In Polish: Czas akcji:
    layout->addRow(tr("Time of the action:"), m_time);
    m_color->setObjectName(QStringLiteral("plotColorCombo"));
    //: The own color of the card of a text on the plot board. In Polish: Kolor karty:
    layout->addRow(tr("Card color:"), m_color);
    fillColors();

    for (const QLineEdit* line : {m_pov, m_place, m_time}) {
        connect(line, &QLineEdit::editingFinished, this, &PlotFieldsBox::save);
    }
    m_description->installEventFilter(this);
    connect(m_color, &QComboBox::activated, this, &PlotFieldsBox::save);
    // A change made elsewhere (on the board, in the outline) shows here
    connect(&m_book, &core::ProjectManager::projectChanged, this, [this]() {
        if (!m_elementId.isEmpty()) {
            fill();
        }
    });
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &PlotFieldsBox::fillColors);
    hide();
}

void PlotFieldsBox::showElement(const QString& elementId) {
    const core::BookProject* project = m_book.project();
    const core::ProjectElement* element = m_book.findElement(elementId);
    const std::optional<core::ElementPlace> place =
        project != nullptr ? project->placeOf(elementId) : std::nullopt;
    const bool card = element != nullptr && element->hasKind() &&
                      m_book.formOf(*element) == core::ElementForm::Text && place &&
                      place->place == core::BookPlace::Main;
    m_elementId = card ? elementId : QString();
    setVisible(card);
    if (!card) {
        return;
    }
    m_povs->setStringList(PlotBoard::povSuggestions(m_book));
    m_places->setStringList(PlotBoard::placeSuggestions(m_book));
    const bool readOnly = m_book.isReadOnly();
    for (QLineEdit* line : {m_pov, m_place, m_time}) {
        line->setReadOnly(readOnly);
    }
    m_description->setReadOnly(readOnly);
    m_color->setEnabled(!readOnly);
    fill();
}

bool PlotFieldsBox::eventFilter(QObject* watched, QEvent* event) {
    // The description is kept when the keys leave it, as the notes of the text
    if (watched == m_description && event->type() == QEvent::FocusOut) {
        save();
    }
    return QGroupBox::eventFilter(watched, event);
}

void PlotFieldsBox::fill() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr) {
        return;
    }
    m_filling = true;
    if (!m_description->hasFocus() && m_description->toPlainText() != element->description) {
        m_description->setPlainText(element->description);
    }
    const std::array<std::pair<QLineEdit*, QString>, 3> lines{{{m_pov, element->plot.pov},
                                                              {m_place, element->plot.place},
                                                              {m_time, element->plot.time}}};
    for (const auto& [line, text] : lines) {
        if (!line->hasFocus()) {
            setLine(line, text);
        }
    }
    m_color->setCurrentIndex(std::max(0, m_color->findData(element->plot.color)));
    m_filling = false;
}

void PlotFieldsBox::fillColors() {
    const QString chosen = m_color->currentData().toString();
    m_color->clear();
    //: A card without an own color. In Polish: Bez koloru
    m_color->addItem(tr("No Color"), QString());
    const PlotColors palette = PlotColors::current();
    for (const QString& name : PlotColors::ownNames()) {
        QPixmap swatch(SWATCH_SIZE, SWATCH_SIZE);
        swatch.fill(palette.ofOwn(name));
        m_color->addItem(QIcon(swatch), PlotColors::ownTitle(name), name);
    }
    m_color->setCurrentIndex(std::max(0, m_color->findData(chosen)));
}

void PlotFieldsBox::save() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (m_filling || element == nullptr || m_book.isReadOnly()) {
        return;
    }
    const QString description = m_description->toPlainText().trimmed();
    core::ElementPlot plot = element->plot;
    plot.pov = m_pov->text().trimmed();
    plot.place = m_place->text().trimmed();
    plot.time = m_time->text().trimmed();
    plot.color = m_color->currentData().toString();
    if (description != element->description || !(plot == element->plot)) {
        // A copy: the panel may show another element while the book saves this one
        const QString elementId = m_elementId;
        m_book.setPlot(elementId, description, plot);
    }
}

}  // namespace kalahari::gui
