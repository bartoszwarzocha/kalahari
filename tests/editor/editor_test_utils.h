/// @file editor_test_utils.h
/// @brief Helpers shared by the editor tests and the editor benchmark

#pragma once

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

/// Reads each popup menu as it shows, chooses the entry with the text @p choice (empty: none)
/// and closes the menu, while it lives. The menu gets no window on the screen, where the
/// platform decides about a popup: on macOS the popup of a test window was closed before the
/// test could read it.
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
            QAction* chosen = nullptr;
            for (QAction* action : menu->actions()) {
                if (!action->isSeparator()) {
                    m_texts.append(action->text());
                    if (!m_choice.isEmpty() && action->text() == m_choice && chosen == nullptr) {
                        chosen = action;
                    }
                }
            }
            if (chosen != nullptr) {
                chosen->trigger();
            }
            // Closed once it is open, the menu ends its event loop
            QTimer::singleShot(0, menu, [menu]() { menu->close(); });
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QStringList& m_texts;
    QString m_choice;
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

    int menusShown() const { return m_menusShown; }
    /// The first menu took the key ahead of the program's shortcuts
    bool keyTaken() const { return m_keyTaken; }
    /// The first menu was still open after the key
    bool stayedOpen() const { return m_stayedOpen; }

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

    int count() const { return m_count; }

private:
    QMetaObject::Connection m_connection;
    int m_count = 0;
};

}  // namespace kalahari::test
