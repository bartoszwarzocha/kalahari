// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file editor_test_utils.h
/// @brief Helpers shared by the editor tests and the editor benchmark

#ifndef KALAHARI_TESTS_EDITOR_EDITOR_TEST_UTILS_H
#define KALAHARI_TESTS_EDITOR_EDITOR_TEST_UTILS_H

#include <kalahari/editor/annotation.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QKeyEvent>
#include <QMenu>
#include <QPointer>
#include <QResizeEvent>
#include <QStringList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>
#include <QTimer>
#include <QVariantMap>
#include <functional>
#include <utility>

namespace kalahari::test {

/// KML document with one plain paragraph per string
inline QString kmlOf(const QStringList& paragraphs) {
    QString kml = QStringLiteral("<kml>");
    for (const auto& p : paragraphs) kml += QStringLiteral("<p>") + p + QStringLiteral("</p>");
    return kml + QStringLiteral("</kml>");
}

/// Deliver a resize to a hidden widget (QWidget::resize() alone only queues it)
inline void resizeWidget(QWidget& widget, const QSize& newSize) {
    const QSize oldSize = widget.size();
    widget.resize(newSize);
    QResizeEvent event(newSize, oldSize);
    QCoreApplication::sendEvent(&widget, &event);
}

/// Run the event loop for @p ms, calling @p onTick about every 5 ms
inline void runEventLoop(int ms, const std::function<void()>& onTick = {}) {
    QEventLoop loop;
    QTimer ticker;
    if (onTick) {
        QObject::connect(&ticker, &QTimer::timeout, onTick);
        ticker.start(5);
    }
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

/// How long a test waits for a dictionary loaded in the background: a debug build made with
/// MSVC, with other tests running beside it, takes more than half a minute for the Polish one
constexpr int DICTIONARY_WAIT_MS = 120000;

/// Run the event loop until @p done returns true or @p timeoutMs passes
inline bool waitUntil(const std::function<bool()>& done, int timeoutMs = 2000) {
    if (done()) return true;
    QEventLoop loop;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&loop, &done] {
        if (done()) loop.quit();
    });
    poll.start(5);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    return done();
}

/// Whether a menu is still waiting for the words to put in place of a misspelled one: they
/// come in it once the dictionary found them (BookEditor::SUGGESTIONS_PENDING_PROPERTY)
inline bool isFindingWords(const QMenu* menu) {
    return menu->property(editor::BookEditor::SUGGESTIONS_PENDING_PROPERTY).toBool();
}

/// Reads each popup menu as it shows (the menu of a misspelled word once its words are
/// found), with the items of its submenus after theirs, chooses the entry with the text
/// @p choice (empty: none; also in a submenu) and closes the menu, while
/// it lives. The menu gets no window on the screen, where the platform decides about a popup:
/// on macOS the popup of a test window was closed before the test could read it.
class PopupMenuReader : public QObject {
public:
    PopupMenuReader(QStringList& texts, QString choice)
        : m_texts(texts), m_choice(std::move(choice)) {
        QCoreApplication::instance()->installEventFilter(this);
    }
    ~PopupMenuReader() override { QCoreApplication::instance()->removeEventFilter(this); }
    PopupMenuReader(const PopupMenuReader&) = delete;
    PopupMenuReader& operator=(const PopupMenuReader&) = delete;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        auto* menu = qobject_cast<QMenu*>(watched);
        // The show event comes before the menu's window is put on the screen
        if (menu != nullptr && event->type() == QEvent::Show) {
            menu->setAttribute(Qt::WA_DontShowOnScreen);
            if (isFindingWords(menu)) {
                readWhenFound(menu);
            } else {
                read(menu);
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void read(QMenu* menu) {
        QAction* chosen = nullptr;
        readItems(menu, chosen);
        if (chosen != nullptr) {
            chosen->trigger();
        }
        // Closed once it is open, the menu ends its event loop
        QTimer::singleShot(0, menu, [menu]() { menu->close(); });
    }

    /// The texts of a menu's items, each submenu's after its own (the words of a misspelled
    /// one are in the submenu Suggestions), and the item to choose
    void readItems(const QMenu* menu, QAction*& chosen) {
        for (QAction* action : menu->actions()) {
            if (action->isSeparator()) {
                continue;
            }
            m_texts.append(action->text());
            if (!m_choice.isEmpty() && action->text() == m_choice && chosen == nullptr) {
                chosen = action;
            }
            if (const QMenu* submenu = action->menu()) {
                readItems(submenu, chosen);
            }
        }
    }

    /// Read once the words of a misspelled one are in the menu (at most DICTIONARY_WAIT_MS)
    void readWhenFound(QMenu* menu) {
        auto* poll = new QTimer(menu);
        int ticks = 0;
        QObject::connect(poll, &QTimer::timeout, menu, [this, menu, poll, ticks]() mutable {
            if (isFindingWords(menu) && ++ticks * POLL_MS < DICTIONARY_WAIT_MS) {
                return;
            }
            poll->stop();
            read(menu);
        });
        poll->start(POLL_MS);
    }

    static constexpr int POLL_MS = 5;
    QStringList& m_texts;
    QString m_choice;
};

/// While it lives, the popup menus that show get no window on the screen, as with
/// PopupMenuReader, for a test that reads them itself. On Windows a popup shown while the
/// application has no active window takes the foreground (QTBUG-44928), and the application,
/// deactivated as soon as another one takes it, closes all its popups
/// (QEvent::ApplicationDeactivate): with the tests run side by side, a menu waiting a few
/// seconds for the words of a misspelled one was closed before the test could read it.
/// The test takes the menu from shown(): Qt 6.8 and later leave a popup without a window on
/// the screen out of QApplication::activePopupWidget().
class OffScreenPopups : public QObject {
public:
    OffScreenPopups() { QCoreApplication::instance()->installEventFilter(this); }
    ~OffScreenPopups() override { QCoreApplication::instance()->removeEventFilter(this); }
    OffScreenPopups(const OffScreenPopups&) = delete;
    OffScreenPopups& operator=(const OffScreenPopups&) = delete;
    OffScreenPopups(OffScreenPopups&&) = delete;
    OffScreenPopups& operator=(OffScreenPopups&&) = delete;

    /// The menu shown last, while it is open; nullptr: none
    [[nodiscard]] QMenu* shown() const {
        return m_shown != nullptr && m_shown->isVisible() ? m_shown.data() : nullptr;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        // The show event comes before the menu's window is put on the screen
        if (auto* menu = qobject_cast<QMenu*>(watched);
            menu != nullptr && event->type() == QEvent::Show) {
            menu->setAttribute(Qt::WA_DontShowOnScreen);
            m_shown = menu;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<QMenu> m_shown;
};

/// Presses @p key in the first popup menu that shows, as the writer would, and closes the
/// later ones, while it lives (the menus get no window on the screen, as with
/// PopupMenuReader). A first menu the key leaves open is closed after it.
class PopupMenuKey : public QObject {
public:
    explicit PopupMenuKey(int key) : m_key(key) {
        QCoreApplication::instance()->installEventFilter(this);
    }
    ~PopupMenuKey() override { QCoreApplication::instance()->removeEventFilter(this); }
    PopupMenuKey(const PopupMenuKey&) = delete;
    PopupMenuKey& operator=(const PopupMenuKey&) = delete;

    [[nodiscard]] int menusShown() const { return m_menusShown; }
    /// The first menu took the key ahead of the program's shortcuts
    [[nodiscard]] bool keyTaken() const { return m_keyTaken; }
    /// The first menu was still open after the key
    [[nodiscard]] bool stayedOpen() const { return m_stayedOpen; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        auto* menu = qobject_cast<QMenu*>(watched);
        if (menu != nullptr && event->type() == QEvent::Show) {
            menu->setAttribute(Qt::WA_DontShowOnScreen);
            const bool first = m_menusShown++ == 0;
            QTimer::singleShot(0, menu, [this, menu, first]() {
                if (first) {
                    QKeyEvent shortcut(QEvent::ShortcutOverride, m_key, Qt::NoModifier);
                    shortcut.ignore();
                    QCoreApplication::sendEvent(menu, &shortcut);
                    m_keyTaken = shortcut.isAccepted();
                    QKeyEvent press(QEvent::KeyPress, m_key, Qt::NoModifier);
                    QCoreApplication::sendEvent(menu, &press);
                    m_stayedOpen = menu->isVisible();
                }
                menu->close();
            });
        }
        return QObject::eventFilter(watched, event);
    }

private:
    int m_key;
    int m_menusShown = 0;
    bool m_keyTaken = false;
    bool m_stayedOpen = false;
};

/// Run @p open, which shows a popup menu and waits for it (QMenu::exec()), as the writer
/// would: note the texts of the menu's entries, choose the one with the text @p choice
/// (empty: none) and close the menu. Returns the texts; none when no menu was shown.
inline QStringList runPopupMenu(const std::function<void()>& open,
                                const QString& choice = QString()) {
    QStringList texts;
    const PopupMenuReader reader(texts, choice);
    open();
    return texts;
}

/// Char format of the first fragment of @p block whose text contains @p needle.
/// Returns an invalid (default) format when no fragment matches.
inline QTextCharFormat formatOfFragmentContaining(const QTextBlock& block, const QString& needle) {
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (fragment.isValid() && fragment.text().contains(needle)) {
            return fragment.charFormat();
        }
    }
    return QTextCharFormat();
}

/// Metadata map stored under a KML metadata property
inline QVariantMap metadataOf(const QTextCharFormat& format, int property) {
    return format.property(property).toMap();
}

/// Ids of the annotations a format carries, in their order
inline QStringList annotationIds(const QTextFormat& format) {
    QStringList ids;
    for (const editor::Annotation& annotation : editor::annotationsOf(format)) {
        ids << annotation.id;
    }
    return ids;
}

/// Counts the blocks the editor's document layout lays out while the counter is alive
/// (KalahariTextDocumentLayout::blocksLaidOut())
class LaidOutBlockCounter {
public:
    explicit LaidOutBlockCounter(const editor::BookEditor& bookEditor)
        : LaidOutBlockCounter(*bookEditor.textDocument()) {}

    explicit LaidOutBlockCounter(const QTextDocument& document) {
        auto* layout =
            qobject_cast<editor::KalahariTextDocumentLayout*>(document.documentLayout());
        m_connection = QObject::connect(layout, &editor::KalahariTextDocumentLayout::blocksLaidOut,
                                        [this](int, int count) { m_count += count; });
    }
    ~LaidOutBlockCounter() { QObject::disconnect(m_connection); }
    LaidOutBlockCounter(const LaidOutBlockCounter&) = delete;
    LaidOutBlockCounter& operator=(const LaidOutBlockCounter&) = delete;

    [[nodiscard]] int count() const { return m_count; }

private:
    QMetaObject::Connection m_connection;
    int m_count = 0;
};

}  // namespace kalahari::test

#endif  // KALAHARI_TESTS_EDITOR_EDITOR_TEST_UTILS_H
