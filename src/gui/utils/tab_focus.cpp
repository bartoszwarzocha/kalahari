/// @file tab_focus.cpp
/// @brief The keys go to the page in front of the tabs, also when a tab closes or is clicked

#include "kalahari/gui/utils/tab_focus.h"

#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
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

void focusPageInFront(QTabWidget& tabs) {
    // In a window behind another, the widget gets the focus when its window comes back
    if (QWidget* front = tabs.currentWidget()) {
        focusTarget(front)->setFocus(Qt::OtherFocusReason);
    } else {
        tabs.setFocus(Qt::OtherFocusReason);
    }
}

void removeTabKeepingFocus(QTabWidget& tabs, int index) {
    QWidget* page = tabs.widget(index);
    if (page == nullptr) {
        return;
    }
    // The focus of the tabs' window: also while a window of its own is in front of it, such as
    // the question about unsaved changes before a tab or the book closes
    const QWidget* focus = tabs.window()->focusWidget();
    const bool focusOnPage = focus != nullptr && (focus == page || page->isAncestorOf(focus));
    const bool inFront = index == tabs.currentIndex();

    tabs.removeTab(index);

    if (focusOnPage || inFront) {
        focusPageInFront(tabs);
    }
}

void focusPageOnTabClick(QTabWidget& tabs) {
    QTabWidget* target = &tabs;
    QObject::connect(tabs.tabBar(), &QTabBar::tabBarClicked, target, [target](int index) {
        if (index < 0) {
            return;  // beside the tabs
        }
        // The tab bar tells of the press before its tab comes to the front
        QTimer::singleShot(0, target, [target]() { focusPageInFront(*target); });
    });
}

} // namespace utils
} // namespace gui
} // namespace kalahari
