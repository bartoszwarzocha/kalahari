/// @file tab_focus.cpp
/// @brief Closing a tab gives the keys to the page in front

#include "kalahari/gui/utils/tab_focus.h"

#include <QTabWidget>
#include <QWidget>

namespace kalahari {
namespace gui {
namespace utils {

namespace {

/// Whether @p widget of @p page can take the focus now
bool canTakeFocus(const QWidget* page, const QWidget* widget) {
    return widget->isVisibleTo(page) && widget->isEnabled();
}

/// The widget of @p page to get the focus, as a switch of tabs picks it
QWidget* focusTarget(QWidget* page) {
    // The field of the page that had the focus last
    if (QWidget* last = page->focusWidget(); last != nullptr && canTakeFocus(page, last)) {
        return last;
    }
    // Its first field that takes the focus from Tab
    for (QWidget* widget = page->nextInFocusChain(); widget != page;
         widget = widget->nextInFocusChain()) {
        if (page->isAncestorOf(widget) && (widget->focusPolicy() & Qt::TabFocus) == Qt::TabFocus
            && widget->focusProxy() == nullptr && canTakeFocus(page, widget)) {
            return widget;
        }
    }
    return page;
}

}  // anonymous namespace

void removeTabKeepingFocus(QTabWidget& tabs, int index) {
    QWidget* page = tabs.widget(index);
    if (page == nullptr) {
        return;
    }
    // The focus of the tabs' window: also while a window of its own is in front of it, such as
    // the question about unsaved changes before a tab or the book closes
    const QWidget* focus = tabs.window()->focusWidget();
    const bool focusOnPage = focus != nullptr && (focus == page || page->isAncestorOf(focus));

    tabs.removeTab(index);

    if (!focusOnPage) {
        return;
    }
    // In a window behind another, the widget gets the focus when its window comes back
    if (QWidget* front = tabs.currentWidget()) {
        focusTarget(front)->setFocus(Qt::OtherFocusReason);
    } else {
        tabs.setFocus(Qt::OtherFocusReason);
    }
}

} // namespace utils
} // namespace gui
} // namespace kalahari
