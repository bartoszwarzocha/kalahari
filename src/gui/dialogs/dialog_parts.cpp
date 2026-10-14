/// @file dialog_parts.cpp
/// @brief Parts that the program's own dialogs share: a list as high as its rows, a note with
/// an icon, and the Copy button

#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/core/art_provider.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include <algorithm>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int NOTE_ICON_SIZE = 16;      ///< Size of the icon of a note
constexpr int NOTE_SPACING = 8;         ///< Between the icon of a note and its sentence
constexpr int COPIED_NOTICE_MS = 2000;  ///< How long Copy says "Copied"

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

} // namespace dialogs
} // namespace gui
} // namespace kalahari
