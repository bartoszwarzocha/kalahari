// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_card_editor.cpp
/// @brief A card of the plot board edited where it is: its title, description, point of view,
/// place and time of the action

#include "kalahari/gui/widgets/plot_card_editor.h"

#include "kalahari/gui/plot_board.h"
#include "kalahari/gui/widgets/plot_board_view.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>

namespace kalahari::gui {

namespace {

/// @brief The lines of the description the field shows
constexpr int DESCRIPTION_LINES = 4;
constexpr int DESCRIPTION_PADDING = 8;  ///< Inside the field of the description
constexpr double LABEL_FONT_SCALE = 0.9;  ///< The labels of the fields are this smaller
constexpr int MARGIN_X = 8;             ///< On the left and right of the fields
constexpr int MARGIN_TOP = 6;           ///< Over the first field
constexpr int MARGIN_BOTTOM = 8;        ///< Under the buttons
constexpr int BUTTONS_GAP = 6;          ///< Between the last field and the buttons

/// @brief What a key does in a field of the editor
enum class KeyAnswer : std::uint8_t {
    None,   ///< What it does in the field
    Keep,   ///< Keep what was written
    Leave   ///< Leave the card as it was
};

/// @brief Whether a suggestion of @p line is shown: it takes Enter and Esc for itself
bool suggesting(const QLineEdit* line) {
    return line != nullptr && line->completer() != nullptr &&
           line->completer()->popup() != nullptr && line->completer()->popup()->isVisible();
}

/// @brief Whether @p key keeps what was written in field @p line (nullptr: not a field of one
/// line): Ctrl+Enter, and Enter in a field of one line
bool keeps(const QKeyEvent* key, Qt::KeyboardModifiers modifiers, const QLineEdit* line) {
    if (key->key() != Qt::Key_Return && key->key() != Qt::Key_Enter) {
        return false;
    }
    return modifiers == Qt::ControlModifier ||
           (modifiers == Qt::NoModifier && line != nullptr && !suggesting(line));
}

/// @brief What @p key does in field @p line (nullptr: not a field of one line)
KeyAnswer answerOf(const QKeyEvent* key, const QLineEdit* line) {
    Qt::KeyboardModifiers modifiers = key->modifiers();
    modifiers.setFlag(Qt::KeypadModifier, false);
    if (keeps(key, modifiers, line)) {
        return KeyAnswer::Keep;
    }
    if (key->key() == Qt::Key_Escape && modifiers == Qt::NoModifier && !suggesting(line)) {
        return KeyAnswer::Leave;
    }
    return KeyAnswer::None;
}

/// @brief A field of one line with suggestions that a few letters of any word filter
QLineEdit* suggestingEdit(const QString& text, const QStringList& suggestions, QWidget* parent) {
    auto* edit = new QLineEdit(text, parent);
    if (!suggestions.isEmpty()) {
        auto* completer = new QCompleter(suggestions, edit);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        edit->setCompleter(completer);
    }
    return edit;
}

/// @brief A small label over a field, with the field as its buddy
QLabel* fieldLabel(const QString& text, QWidget* buddy, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setBuddy(buddy);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() * LABEL_FONT_SCALE);
    label->setFont(font);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

}  // namespace

PlotCardEditor::PlotCardEditor(const PlotCard& card, const QStringList& povs,
                               const QStringList& places, QWidget* parent)
    : QFrame(parent),
      m_elementId(card.id),
      m_plot(card.plot),
      m_title(new QLineEdit(card.title, this)),
      m_description(new QPlainTextEdit(card.description, this)),
      m_pov(suggestingEdit(card.plot.pov, povs, this)),
      m_place(suggestingEdit(card.plot.place, places, this)),
      m_time(new QLineEdit(card.plot.time, this)) {
    setObjectName(QStringLiteral("plotCardEditor"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);
    //: The frame that edits a card on the plot board, for the screen reader. In Polish:
    //: Edycja karty
    setAccessibleName(tr("Card Editor"));

    m_description->setTabChangesFocus(true);
    m_description->setFixedHeight(
        (m_description->fontMetrics().lineSpacing() * DESCRIPTION_LINES) +
        (2 * m_description->frameWidth()) + DESCRIPTION_PADDING);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(MARGIN_X, MARGIN_TOP, MARGIN_X, MARGIN_BOTTOM);
    layout->setSpacing(2);
    //: A field of a card edited on the plot board. In Polish: &Tytuł
    layout->addWidget(fieldLabel(tr("&Title"), m_title, this));
    layout->addWidget(m_title);
    //: In Polish: &Opis
    layout->addWidget(fieldLabel(tr("&Description"), m_description, this));
    layout->addWidget(m_description);
    //: In Polish: &Punkt widzenia
    layout->addWidget(fieldLabel(tr("Point of &view"), m_pov, this));
    layout->addWidget(m_pov);
    //: In Polish: &Miejsce
    layout->addWidget(fieldLabel(tr("&Place"), m_place, this));
    layout->addWidget(m_place);
    //: In Polish: &Czas akcji
    layout->addWidget(fieldLabel(tr("Ti&me of the action"), m_time, this));
    layout->addWidget(m_time);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, BUTTONS_GAP, 0, 0);
    //: The keys of a card edited on the plot board. In Polish: Ctrl+Enter zachowuje, Esc
    //: porzuca
    auto* hint = new QLabel(tr("Ctrl+Enter keeps it, Esc leaves it"), this);
    hint->setForegroundRole(QPalette::PlaceholderText);
    hint->setWordWrap(true);
    //: In Polish: Zachowaj
    m_save = new QPushButton(tr("Save"), this);
    //: In Polish: Anuluj
    m_cancel = new QPushButton(tr("Cancel"), this);
    m_save->setAutoDefault(false);
    m_cancel->setAutoDefault(false);
    buttons->addWidget(hint, 1);
    buttons->addWidget(m_save);
    buttons->addWidget(m_cancel);
    layout->addLayout(buttons);

    connect(m_save, &QPushButton::clicked, this, &PlotCardEditor::accepted);
    connect(m_cancel, &QPushButton::clicked, this, &PlotCardEditor::rejected);
    for (QWidget* field : {static_cast<QWidget*>(m_title), static_cast<QWidget*>(m_description),
                           static_cast<QWidget*>(m_pov), static_cast<QWidget*>(m_place),
                           static_cast<QWidget*>(m_time)}) {
        field->installEventFilter(this);
    }
    setTabOrder(m_title, m_description);
    setTabOrder(m_description, m_pov);
    setTabOrder(m_pov, m_place);
    setTabOrder(m_place, m_time);
    setTabOrder(m_time, m_save);
    setTabOrder(m_save, m_cancel);
}

PlotCardEdit PlotCardEditor::edit() const {
    PlotCardEdit edit;
    edit.title = m_title->text().trimmed();
    edit.description = m_description->toPlainText().trimmed();
    edit.plot = m_plot;
    edit.plot.pov = m_pov->text().trimmed();
    edit.plot.place = m_place->text().trimmed();
    edit.plot.time = m_time->text().trimmed();
    return edit;
}

void PlotCardEditor::focusTitle() {
    m_title->setFocus(Qt::OtherFocusReason);
    m_title->selectAll();
}

bool PlotCardEditor::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride) {
        return QFrame::eventFilter(watched, event);
    }
    auto* key = static_cast<QKeyEvent*>(event);
    const KeyAnswer answer = answerOf(key, qobject_cast<QLineEdit*>(watched));
    if (answer == KeyAnswer::None) {
        return QFrame::eventFilter(watched, event);
    }
    // The keys are the editor's, not the window's commands
    if (event->type() == QEvent::ShortcutOverride) {
        key->accept();
        return true;
    }
    if (answer == KeyAnswer::Keep) {
        emit accepted();
    } else {
        emit rejected();
    }
    return true;
}

bool PlotCardEditor::event(QEvent* event) {
    // Esc on a button of the editor leaves the card too
    if (event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        emit rejected();
        return true;
    }
    return QFrame::event(event);
}

}  // namespace kalahari::gui
