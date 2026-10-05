/// @file editor_test_utils.h
/// @brief Helpers shared by the editor tests and the editor benchmark

#pragma once

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kalahari_text_document_layout.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QResizeEvent>
#include <QStringList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>
#include <QTimer>
#include <QVariantMap>
#include <functional>

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
