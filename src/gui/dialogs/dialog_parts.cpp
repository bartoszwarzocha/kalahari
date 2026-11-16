// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file dialog_parts.cpp
/// @brief Parts that the program's own dialogs share: a list as high as its rows, the keys of
/// a list with a filter, a note with an icon, and the Copy button

#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/core/art_provider.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int NOTE_ICON_SIZE = 16;      ///< Size of the icon of a note
constexpr int NOTE_SPACING = 8;         ///< Between the icon of a note and its sentence
constexpr int COPIED_NOTICE_MS = 2000;  ///< How long Copy says "Copied"

/// Key @p key typed in a list goes to its filter @p filter: Backspace, a letter, a digit or a
/// sign, also with Shift; whether it went there
bool typeInFilter(const QKeyEvent& key, Qt::KeyboardModifiers modifiers, QLineEdit* filter) {
    if (key.key() == Qt::Key_Backspace && modifiers == Qt::NoModifier) {
        filter->backspace();
        return true;
    }
    const QString text = key.text();
    const bool letter = !text.isEmpty() && text.at(0).isPrint() && !text.at(0).isSpace() &&
                        (modifiers & ~Qt::ShiftModifier) == Qt::NoModifier;
    if (letter) {
        filter->insert(text);
    }
    return letter;
}

/// Whether @p key, with @p modifiers, chooses in a list: the arrows up and down, Page Up and
/// Page Down, without other keys
bool choosesInList(const QKeyEvent& key, Qt::KeyboardModifiers modifiers) {
    const int code = key.key();
    return modifiers == Qt::NoModifier && (code == Qt::Key_Down || code == Qt::Key_Up ||
                                           code == Qt::Key_PageDown || code == Qt::Key_PageUp);
}

} // anonymous namespace

void RowList::setRows(int rows) {
    m_rows = rows;
    updateGeometry();
}

QSize RowList::sizeHint() const {
    return {QTreeWidget::sizeHint().width(), heightFor(qBound(1, m_rows, MAXIMUM_ROWS))};
}

QSize RowList::minimumSizeHint() const {
    return {QTreeWidget::minimumSizeHint().width(), heightFor(qBound(1, m_rows, MINIMUM_ROWS))};
}

int RowList::heightFor(int rows) const {
    const int measured = topLevelItemCount() > 0 ? sizeHintForRow(0) : 0;
    const int rowHeight =
        qMax(measured, qMax(fontMetrics().height() + 6, iconSize().height() + 4));
    return rowHeight * rows + 2 * frameWidth();
}

bool takeFilterKeys(const QObject* watched, const QEvent* event, QLineEdit* filter,
                    QAbstractItemView* list) {
    if (event->type() != QEvent::KeyPress) {
        return false;
    }
    const auto* key = static_cast<const QKeyEvent*>(event);
    const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
    if (watched == list) {
        return typeInFilter(*key, modifiers, filter);
    }
    if (watched != filter || !choosesInList(*key, modifiers)) {
        return false;
    }
    list->setFocus(Qt::OtherFocusReason);
    QKeyEvent copy(QEvent::KeyPress, key->key(), key->modifiers());
    QCoreApplication::sendEvent(list, &copy);
    return true;
}

RowList* newRowList(QWidget* parent, const QString& accessibleName) {
    auto* list = new RowList(parent);
    list->setColumnCount(2);
    list->setHeaderHidden(true);
    list->setRootIsDecorated(false);
    list->setItemsExpandable(false);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setAccessibleName(accessibleName);
    list->header()->setStretchLastSection(false);
    list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    return list;
}

QWidget* newNote(QWidget* parent, QLabel** icon, QLabel** text) {
    auto* note = new QWidget(parent);
    auto* layout = new QHBoxLayout(note);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(NOTE_SPACING);
    *icon = new QLabel(note);
    (*icon)->setAlignment(Qt::AlignTop);
    layout->addWidget(*icon);
    *text = new QLabel(note);
    (*text)->setTextFormat(Qt::PlainText);
    (*text)->setWordWrap(true);
    (*text)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(*text, 1);
    return note;
}

void setNoteIcon(QLabel* icon, const QString& iconId) {
    icon->setPixmap(core::ArtProvider::getInstance().getIcon(iconId).pixmap(NOTE_ICON_SIZE,
                                                                            NOTE_ICON_SIZE));
}

void setUpCopyButton(QPushButton* button, const std::function<QString()>& text) {
    const QString name = QCoreApplication::translate("DialogParts", "&Copy");
    const QString copied = QCoreApplication::translate("DialogParts", "Copied");
    button->setToolTip(
        QCoreApplication::translate("DialogParts", "Copy what the window says to the clipboard"));
    // As wide as its longest text, so "Copied" does not move the other buttons
    button->setText(copied);
    const int copiedWidth = button->sizeHint().width();
    button->setText(name);
    button->setMinimumWidth(std::max(copiedWidth, button->sizeHint().width()));
    QObject::connect(button, &QPushButton::clicked, button, [button, text, name, copied]() {
        QApplication::clipboard()->setText(text());
        button->setText(copied);
        QTimer::singleShot(COPIED_NOTICE_MS, button, [button, name]() { button->setText(name); });
    });
}

int widthOfLetters(const QFontMetrics& metrics, int letters)
{
    const QString text = QStringLiteral("the quick brown fox jumps over the lazy dog ");
    return qRound(metrics.horizontalAdvance(text) * static_cast<qreal>(letters) /
                  static_cast<qreal>(text.size()));
}

int FittingTabs::heightForWidth(int width) const
{
    // The frame around the pages, as QTabWidget measures it
    QStyleOption option(0);
    option.initFrom(this);
    option.state = QStyle::State_None;
    const QSize frame = style()->sizeFromContents(QStyle::CT_TabWidget, &option, QSize(0, 0), this);
    const int pageWidth = width - frame.width();
    int pages = 0;
    for (int i = 0; i < count(); ++i) {
        if (!isTabVisible(i)) {
            continue;  // the options of a format that is not chosen
        }
        const QWidget* page = widget(i);
        const int height = page->hasHeightForWidth() ? page->heightForWidth(pageWidth) : -1;
        pages = std::max(pages, height >= 0 ? height : page->sizeHint().height());
    }
    return frame.height() + tabBar()->sizeHint().height() + pages;
}

QSize FittingTabs::sizeHint() const
{
    const QSize plain = QTabWidget::sizeHint();
    return {plain.width(), heightForWidth(plain.width())};
}

QString literal(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

QLabel* newRemark(QWidget* parent, const QString& text)
{
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

QWidget* withRemark(QAbstractButton* button, QLabel* remark, QWidget* beside)
{
    auto* option = new QWidget(button->parentWidget());
    auto* layout = new QVBoxLayout(option);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    button->setParent(option);
    remark->setParent(option);
    const QStyle* style = button->style();
    remark->setContentsMargins(style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, button) +
                                   style->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, button),
                               0, 0, 0);
    if (beside != nullptr) {
        beside->setParent(option);
        auto* line = new QHBoxLayout();
        line->setSpacing(button->fontMetrics().averageCharWidth() * 3);
        line->addWidget(button);
        line->addWidget(beside);
        line->addStretch(1);
        layout->addLayout(line);
    } else {
        layout->addWidget(button);
    }
    layout->addWidget(remark);
    return option;
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
