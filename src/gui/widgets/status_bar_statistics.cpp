/// @file status_bar_statistics.cpp
/// @brief The words, characters and reading time of the document in front, in the status bar

#include "kalahari/gui/widgets/status_bar_statistics.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/editor/book_editor.h"

#include <QLabel>
#include <QLocale>
#include <QStatusBar>
#include <QTimer>

namespace kalahari::gui {

namespace {

/// @brief A label of the status bar's right side, hidden until there are counts to show
QLabel* addLabel(QStatusBar* statusBar, int minimumWidth, const QString& toolTip) {
    auto* label = new QLabel(statusBar);
    label->setFrameStyle(QFrame::NoFrame);
    label->setMinimumWidth(minimumWidth);
    label->setToolTip(toolTip);
    statusBar->addPermanentWidget(label);
    label->hide();
    return label;
}

}  // namespace

StatusBarStatistics::StatusBarStatistics(QStatusBar* statusBar, QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_words = addLabel(statusBar, 100,
                       tr("Words in the document. With a selection: words in the selection and "
                          "in the whole document."));
    m_characters = addLabel(statusBar, 120,
                            tr("Characters with spaces, without paragraph ends. With a "
                               "selection: characters in the selection and in the whole "
                               "document."));
    m_readingTime = addLabel(
        statusBar, 100,
        tr("Reading time of the whole document at %1 words a minute").arg(WORDS_PER_MINUTE));

    m_timer->setSingleShot(true);
    m_timer->setInterval(INTERVAL_MS);
    connect(m_timer, &QTimer::timeout, this, &StatusBarStatistics::update);
}

void StatusBarStatistics::setEditor(editor::BookEditor* bookEditor)
{
    if (bookEditor != m_editor) {
        if (m_editor) {
            disconnect(m_editor, nullptr, this, nullptr);
        }
        m_editor = bookEditor;
        if (m_editor) {
            // An edit, a new selection or new rules of counting change the counts
            connect(m_editor, &editor::BookEditor::contentChanged,
                    this, &StatusBarStatistics::schedule);
            connect(m_editor, &editor::BookEditor::selectionChanged,
                    this, &StatusBarStatistics::schedule);
            connect(m_editor, &editor::BookEditor::countsChanged,
                    this, &StatusBarStatistics::schedule);
            connect(m_editor, &QObject::destroyed, this, &StatusBarStatistics::onEditorDestroyed);
        }
    }
    update();
}

void StatusBarStatistics::update()
{
    m_timer->stop();
    m_words->setVisible(m_editor != nullptr);
    m_characters->setVisible(m_editor != nullptr);
    m_readingTime->setVisible(m_editor != nullptr);
    if (!m_editor) {
        return;
    }

    // Numbers as the system writes them (3 480 in Polish, 3,480 in English)
    const QLocale locale;
    const core::TextCounts text = m_editor->textCounts();
    if (m_editor->hasSelection()) {
        const core::TextCounts selection = m_editor->selectionCounts();
        m_words->setText(tr("Words: %1 of %2")
                             .arg(locale.toString(selection.words), locale.toString(text.words)));
        m_characters->setText(tr("Characters: %1 of %2")
                                  .arg(locale.toString(selection.characters),
                                       locale.toString(text.characters)));
    } else {
        m_words->setText(tr("Words: %1").arg(locale.toString(text.words)));
        m_characters->setText(tr("Characters: %1").arg(locale.toString(text.characters)));
    }

    m_readingTime->setText(readingTimeText(text.words));
}

QString StatusBarStatistics::readingTimeText(int words)
{
    constexpr int MINUTES_PER_HOUR = 60;
    // Rounded up: a minute begun is a minute
    const int minutes = (words + WORDS_PER_MINUTE - 1) / WORDS_PER_MINUTE;
    const QLocale locale;
    if (minutes < MINUTES_PER_HOUR) {
        return tr("Reading: %1 min").arg(locale.toString(minutes));
    }
    const int hours = minutes / MINUTES_PER_HOUR;
    const int rest = minutes % MINUTES_PER_HOUR;
    if (rest == 0) {
        return tr("Reading: %1 h").arg(locale.toString(hours));
    }
    return tr("Reading: %1 h %2 min").arg(locale.toString(hours), locale.toString(rest));
}

void StatusBarStatistics::schedule()
{
    // Not restarted while it runs, so that the counts change during a long burst of typing
    // or a selection made with the mouse too
    if (!m_timer->isActive()) {
        m_timer->start();
    }
}

void StatusBarStatistics::onEditorDestroyed(QObject* object)
{
    if (object == m_editor) {
        m_editor = nullptr;
        update();
    }
}

}  // namespace kalahari::gui
