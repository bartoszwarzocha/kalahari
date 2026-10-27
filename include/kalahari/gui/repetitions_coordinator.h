/// @file repetitions_coordinator.h
/// @brief The repetitions of words: the panel Repetitions, the marks in the text and Next
///        Repetition

#pragma once

#include <kalahari/editor/word_repetitions.h>

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <vector>

class QDockWidget;
class QStatusBar;
class QTabWidget;
class QTimer;

namespace kalahari::editor {
class BookEditor;
struct CursorPosition;
class SpellCheckService;
struct RepetitionMark;
}  // namespace kalahari::editor

namespace kalahari::gui {

class EditorPanel;
class RepetitionsPanel;
class SpellingCoordinator;

/// @brief Coordinates the panel Repetitions with the texts
///
/// Counts the words of the text in front (a moment after the writing stops) or of all the
/// texts of the book (when the writer chooses it or clicks Recount), as the settings of
/// Settings > Editor > Repetitions say; the forms of a word count as one by the spelling
/// dictionary of the text. While the panel shows, the words that come back too close are
/// marked in the texts open (the setting editor.repetitions.highlight, also View >
/// Repetitions in Text), and the words of the card or the word chosen in the panel are framed.
///
/// Tools > Word Repetitions opens the panel with the keys in it; Next Repetition goes to the
/// next word of the text in front that comes back too close, with the panel closed too.
class RepetitionsCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The settings of the repetitions (Settings > Editor > Repetitions)
    static constexpr const char* DISTANCE_KEY = "editor.repetitions.distance";
    static constexpr const char* MIN_LENGTH_KEY = "editor.repetitions.minLength";
    static constexpr const char* MERGE_FORMS_KEY = "editor.repetitions.mergeForms";
    static constexpr const char* SKIP_COMMON_KEY = "editor.repetitions.skipCommon";
    static constexpr const char* SKIPPED_KEY = "editor.repetitions.skipped";
    static constexpr const char* HIGHLIGHT_KEY = "editor.repetitions.highlight";
    static constexpr const char* STYLE_KEY = "editor.repetitions.style";

    /// @brief Opens a chapter of the book in a tab of its own, or brings its tab to the front
    using ChapterOpener = std::function<void(const QString& elementId)>;

    /// @brief Constructor
    /// @param panel The panel Repetitions
    /// @param dock The panel's dock
    /// @param centralTabs The tabs of the documents
    /// @param spelling The spelling as you type, whose dictionary gives the base forms of the
    ///        words of the chapters not open (may be null)
    /// @param openChapter Opens a chapter whose card the writer goes to
    /// @param statusBar Where the commands say what they found (may be null)
    /// @param parent Parent object
    RepetitionsCoordinator(RepetitionsPanel* panel, QDockWidget* dock, QTabWidget* centralTabs,
                           SpellingCoordinator* spelling, ChapterOpener openChapter,
                           QStatusBar* statusBar, QObject* parent = nullptr);

    /// @brief Destructor: the commands have no callbacks any more
    ~RepetitionsCoordinator() override;

    RepetitionsCoordinator(const RepetitionsCoordinator&) = delete;
    RepetitionsCoordinator& operator=(const RepetitionsCoordinator&) = delete;

    /// @brief Give Tools > Word Repetitions, Next Repetition and View > Repetitions in Text
    ///        their callbacks (the last also its check mark)
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief Open the panel with the keys in it (Tools > Word Repetitions)
    void showPanel();

    /// @brief Go to the next word of the text in front that comes back too close: its first
    ///        word after the cursor is selected (Next Repetition); after the last one, the
    ///        first of the text. The status bar says when there is none
    /// @return Whether one was found
    bool goToNextRepetition();

    /// @brief Count the scope of the panel again now
    void refresh();

    /// @brief What counts as a repetition, as the settings are now
    /// @param language The language of the text: its very common words
    static editor::RepetitionOptions options(const QString& language);

    /// @brief Skip a word from now on (its base form, in lower case): the setting
    ///        editor.repetitions.skipped
    static void skipWord(const QString& word);

    /// @brief The analysis shown in the panel
    [[nodiscard]] const editor::RepetitionAnalysis& analysis() const { return m_analysis; }

private slots:
    /// @brief The settings of the repetitions changed (called by name, queued)
    void onSettingsChanged();

private:
    /// @brief A text counted: a chapter of the book or the document in front
    struct Text {
        QString elementId;                ///< Its element in the book (empty: none)
        QString title;                    ///< Its title, for the cards of the whole book
        QStringList paragraphs;           ///< Its paragraphs as they were counted
        editor::BookEditor* editor = nullptr;  ///< Its editor (none: counted from its file);
                                               ///< cleared when the editor goes
    };

    // The texts
    [[nodiscard]] EditorPanel* currentEditorPanel() const;
    [[nodiscard]] editor::BookEditor* currentEditor() const;
    [[nodiscard]] EditorPanel* openPanel(const QString& elementId) const;
    [[nodiscard]] static QString elementIdOf(const EditorPanel* panel);
    [[nodiscard]] QString titleOf(const EditorPanel* panel) const;
    /// @brief The index of the text of an editor among those counted (-1: none)
    [[nodiscard]] int textOf(const editor::BookEditor* editor) const;
    /// @brief The paragraphs of an editor's text
    [[nodiscard]] static QStringList paragraphsOf(const editor::BookEditor& editor);
    /// @brief The texts of the book in reading order: from their editors, else their files
    [[nodiscard]] QList<Text> bookTexts() const;
    /// @brief Follow the changes of an editor's text (once)
    void follow(editor::BookEditor* editor);

    // Counting
    [[nodiscard]] editor::RepetitionAnalysis analyze(const QList<Text>& texts,
                                                     editor::SpellCheckService* service);
    [[nodiscard]] QString baseFormOf(const QString& word, editor::SpellCheckService* service);
    void countChapter();
    void countBook();
    void onTextChanged(editor::BookEditor* editor);
    void onCurrentTabChanged();

    // The panel and the texts
    void showResults();
    void updateMarks();
    [[nodiscard]] std::vector<editor::RepetitionMark> marksOf(int text) const;
    /// @brief The editor of a text counted, its chapter opened when @p open
    editor::BookEditor* editorOf(int text, bool open);
    /// @brief Whether the words are still where they were counted
    [[nodiscard]] bool wordsInPlace(const editor::BookEditor& editor,
                                    const QList<int>& words) const;
    void scrollTo(editor::BookEditor& editor, const editor::TextWord& word);
    /// @brief Where the next word is looked for: the cursor, or the end of the selection
    [[nodiscard]] static editor::CursorPosition after(const editor::BookEditor& editor);
    /// @brief The tab of an editor to the front
    void bringToFront(const editor::BookEditor* editor);
    void selectWord(editor::BookEditor& editor, const editor::TextWord& word);
    void onCardSelected(int index);
    void onCardActivated(int index);
    void onSkipWord(int index);
    void onWordActivated(int index);
    void onDockVisibilityChanged(bool visible);
    void showMessage(const QString& message);

    RepetitionsPanel* m_panel;
    QDockWidget* m_dock;
    QTabWidget* m_centralTabs;
    SpellingCoordinator* m_spelling;
    ChapterOpener m_openChapter;
    QStatusBar* m_statusBar;
    QTimer* m_countTimer{nullptr};

    QList<Text> m_texts;                      ///< The texts counted
    editor::RepetitionAnalysis m_analysis;    ///< Their repetitions
    QSet<int> m_changedTexts;                 ///< Texts changed after they were counted
    bool m_shown = false;                     ///< The panel can be seen
    bool m_stale = true;                      ///< Count again when the panel shows
    QSet<const editor::BookEditor*> m_followed;  ///< Editors whose changes are followed
    QHash<QString, QString> m_baseForms;      ///< Base forms by word, of m_baseFormsDictionary
    QString m_baseFormsDictionary;
    int m_settingsListener{0};
    std::atomic<bool> m_settingsPending{false};  ///< onSettingsChanged() is queued
};

}  // namespace kalahari::gui
