// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_card_editor.cpp
/// @brief A card of the plot board edited where it is: its title, description, point of view,
/// place and time of the action

#include "kalahari/gui/widgets/plot_card_editor.h"

#include "kalahari/gui/widgets/plot_board_view.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari::gui {

namespace {

/// @brief The lines of the description the field shows
constexpr int DESCRIPTION_LINES = 4;

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
    font.setPointSizeF(font.pointSizeF() * 0.9);
    label->setFont(font);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

}  // namespace

PlotCardEditor::PlotCardEditor(const PlotCard& card, const QStringList& povs,
                               const QStringList& places, QWidget* parent)
    : QFrame(parent), m_elementId(card.id), m_plot(card.plot) {
    setObjectName(QStringLiteral("plotCardEditor"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);
    //: The frame that edits a card on the plot board, for the screen reader. In Polish:
    //: Edycja karty
    setAccessibleName(tr("Card Editor"));

    m_title = new QLineEdit(card.title, this);
    m_description = new QPlainTextEdit(card.description, this);
    m_description->setTabChangesFocus(true);
    m_description->setFixedHeight(m_description->fontMetrics().lineSpacing() *
                                      DESCRIPTION_LINES +
                                  2 * m_description->frameWidth() + 8);
    m_pov = suggestingEdit(card.plot.pov, povs, this);
    m_place = suggestingEdit(card.plot.place, places, this);
    m_time = new QLineEdit(card.plot.time, this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 8);
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
    buttons->setContentsMargins(0, 6, 0, 0);
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
    const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
    const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
    // A suggestion shown takes Enter and Esc for itself
    const auto* line = qobject_cast<QLineEdit*>(watched);
    const bool suggesting = line != nullptr && line->completer() != nullptr &&
                            line->completer()->popup() != nullptr &&
                            line->completer()->popup()->isVisible();
    const bool keeps = enter && (modifiers == Qt::ControlModifier ||
                                 (modifiers == Qt::NoModifier && line != nullptr && !suggesting));
    const bool leaves = key->key() == Qt::Key_Escape && modifiers == Qt::NoModifier && !suggesting;
    if (!keeps && !leaves) {
        return QFrame::eventFilter(watched, event);
    }
    // The keys are the editor's, not the window's commands
    if (event->type() == QEvent::ShortcutOverride) {
        key->accept();
        return true;
    }
    if (keeps) {
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
