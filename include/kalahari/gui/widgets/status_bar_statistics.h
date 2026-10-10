/// @file status_bar_statistics.h
/// @brief The words, characters and reading time of the document in front, in the status bar

#pragma once

#include <QObject>

class QLabel;
class QStatusBar;
class QTimer;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

/// @brief The counts of the document in front, in the status bar
///
/// Three labels on the right of the status bar: the words, the characters with spaces
/// (without paragraph ends) and the reading time of the editor in front, a chapter or a
/// file outside the book. With a selection, the words and the characters are those of the
/// selection out of the whole text ("Words: 12 of 3,480"). The counts follow the edits, the
/// selection and new rules of counting, and are hidden without an editor in front.
class StatusBarStatistics : public QObject {
    Q_OBJECT

public:
    /// @brief The reading speed of the reading time
    static constexpr int WORDS_PER_MINUTE = 200;

    /// @brief The counts follow the edits and the selection at most this often
    static constexpr int INTERVAL_MS = 100;

    /// @brief Add the labels to the right of @p statusBar (hidden until an editor is set)
    explicit StatusBarStatistics(QStatusBar* statusBar, QObject* parent = nullptr);

    /// @brief Show the counts of @p bookEditor, the editor in front (nullptr: none, the
    ///        counts are hidden)
    void setEditor(editor::BookEditor* bookEditor);

    /// @brief The label of the words
    QLabel* wordsLabel() const { return m_words; }

    /// @brief The label of the characters
    QLabel* charactersLabel() const { return m_characters; }

    /// @brief The label of the reading time
    QLabel* readingTimeLabel() const { return m_readingTime; }

private:
    /// @brief Show the counts of the editor now
    void update();

    /// @brief Show the counts soon, at most every INTERVAL_MS, always ending with the
    ///        latest state
    void schedule();

    /// @brief Forget the editor when it is destroyed
    void onEditorDestroyed(QObject* object);

    editor::BookEditor* m_editor{nullptr};  ///< The editor in front, or nullptr
    QLabel* m_words{nullptr};               ///< "Words: 3,480" or "Words: 12 of 3,480"
    QLabel* m_characters{nullptr};          ///< "Characters: 20,112"
    QLabel* m_readingTime{nullptr};         ///< "Reading: 18 min"
    QTimer* m_timer{nullptr};               ///< schedule()
};

}  // namespace kalahari::gui
