/// @file spell_check_service.h
/// @brief The spelling dictionary every editor checks its text with (Hunspell)

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <string>
#include <vector>

class Hunspell;  // hunspell/hunspell.hxx
class QThread;
class QTimer;

namespace kalahari::editor {

/// @brief A misspelled word of a text
struct SpellErrorInfo {
    int startPos{0};  ///< Where it starts in the text
    int length{0};    ///< How long it is
    QString word;     ///< The word

    SpellErrorInfo() = default;
    SpellErrorInfo(int start, int len, const QString& w) : startPos(start), length(len), word(w) {}

    bool operator==(const SpellErrorInfo& other) const = default;
};

/// @brief The spelling dictionary the editors check their text with
///
/// Each editor checks its own text with it, a paragraph at a time
/// (BookEditor::setSpellCheckService()), and checks it again on wordsChanged(). The
/// application has one for the chapters of the book and one for the documents outside it
/// written in another language (SpellingCoordinator).
///
/// A dictionary is a Hunspell one: an .aff and a .dic file, shipped in the resources
/// (pl_PL, en_US) or installed on the computer. Loading a large one takes a while, so the
/// application loads it on a thread of its own (loadDictionaryInBackground()); until it is
/// ready, the one loaded before checks the words.
///
/// What is checked: words of letters, also joined by apostrophes (don't) and hyphens
/// (well-known), not touching a digit or an underscore. Words of one letter, words in
/// capitals (NATO) and web and e-mail addresses are left out. A hyphenated word wrong as a
/// whole is reported by its wrong parts.
///
/// The writer's own words are right too: the user dictionary (kept in a file, see
/// setUserDictionaryFile()) and the words ignored until the application closes, the same
/// for the services of every language (shareOwnWordsWith()). A word in lower case there is
/// right in any case (kalahari, Kalahari), one with capitals only with them, and either one
/// also with an ending after an apostrophe (Kalahari's).
class SpellCheckService : public QObject {
    Q_OBJECT

public:
    /// @brief Construct a service without a dictionary (nothing is reported)
    explicit SpellCheckService(QObject* parent = nullptr);

    /// @brief Destructor: waits for a dictionary being loaded
    ~SpellCheckService() override;

    SpellCheckService(const SpellCheckService&) = delete;
    SpellCheckService& operator=(const SpellCheckService&) = delete;

    // =========================================================================
    // Dictionaries
    // =========================================================================

    /// @brief The dictionaries found (pl_PL, en_US...), sorted
    static QStringList availableDictionaries();

    /// @brief The available dictionary for a language
    /// @param language A dictionary (pl_PL, en-GB) or a language (pl, en)
    /// @return The dictionary's name, or empty when none is available
    ///
    /// A language picks its main dictionary (pl_PL for "pl", en_US for "en"), else
    /// any dictionary of that language.
    static QString dictionaryFor(const QString& language);

    /// @brief Load a dictionary now, in place of the one loaded before
    /// @param dictionary Its name (pl_PL, en_US)
    /// @return Whether it was loaded (on failure the one loaded before stays)
    ///
    /// A dictionary being loaded in the background is dropped.
    bool loadDictionary(const QString& dictionary);

    /// @brief Load a dictionary on a thread of its own
    ///
    /// The one loaded before checks the words until it is ready (dictionaryLoaded()). When
    /// another dictionary is asked for meanwhile, the last one asked for is loaded.
    /// @param dictionary Its name (pl_PL, en_US)
    void loadDictionaryInBackground(const QString& dictionary);

    /// @brief Whether a dictionary is being loaded in the background
    bool isLoading() const;

    /// @brief Stop checking with the dictionary: nothing is reported until one is loaded
    ///
    /// For a language without a dictionary. A dictionary being loaded is dropped. The
    /// dictionary stays in memory, so it checks again at once when its language comes back.
    void unloadDictionary();

    /// @brief Stop checking with the dictionary and free the memory it takes
    ///
    /// For a dictionary no document needs any more. A dictionary being loaded is dropped.
    void releaseDictionary();

    /// @brief The dictionary loaded (pl_PL, en_US), or empty without one
    QString currentDictionary() const;

    /// @brief Whether a dictionary is loaded
    bool isDictionaryLoaded() const;

    // =========================================================================
    // Checking
    // =========================================================================

    /// @brief Turn the checking on or off (when off, nothing is reported)
    void setEnabled(bool enabled);

    /// @brief Whether the checking is on
    bool isEnabled() const;

    /// @brief Whether words are checked: the checking is on and a dictionary is loaded
    bool isActive() const;

    /// @brief Whether a word is right
    ///
    /// Without a dictionary, every word is.
    bool isCorrect(const QString& word) const;

    /// @brief Words to put in place of a misspelled one, the likeliest first
    ///
    /// The word typed without its accents, with them put right, comes first. For a word with
    /// a capital first letter the dictionary's suggestions for it written small count too, and
    /// the writer's own words close to the word are among the dictionary's suggestions.
    /// Takes up to a few tenths of a second for a long word, so it is asked only when the
    /// writer wants them.
    QStringList suggestions(const QString& word, int maxSuggestions = 5) const;

    /// @brief The misspelled words of a text (one paragraph), in their order
    QList<SpellErrorInfo> checkParagraph(const QString& text) const;

    // =========================================================================
    // The writer's own words
    // =========================================================================

    /// @brief Check with the writer's own words of another service from now on
    ///
    /// For the services of documents in different languages: the user dictionary and the
    /// words ignored are the same for all of them, and a word added or ignored in one is
    /// right in every one. The words this service had before are dropped.
    void shareOwnWordsWith(SpellCheckService& other);

    /// @brief The file the user dictionary is kept in, and read from now
    /// @param path One word per line, UTF-8; empty: the words are kept only while the
    ///        application runs
    void setUserDictionaryFile(const QString& path);

    /// @brief The file the user dictionary is kept in (empty: none)
    QString userDictionaryFile() const;

    /// @brief Add a word to the user dictionary (saved at once)
    void addToUserDictionary(const QString& word);

    /// @brief Take a word out of the user dictionary (saved at once)
    void removeFromUserDictionary(const QString& word);

    /// @brief Put these words in the user dictionary in place of its words (saved at once)
    void setUserDictionaryWords(const QStringList& words);

    /// @brief Whether the user dictionary has the word, as it is written
    bool isInUserDictionary(const QString& word) const;

    /// @brief The words of the user dictionary, sorted
    QStringList userDictionaryWords() const;

    /// @brief Take a word for right until the application closes (Ignore All)
    void ignoreWord(const QString& word);

    /// @brief Whether the word is ignored, as it is written
    bool isIgnored(const QString& word) const;

signals:
    /// @brief What is right changed: another dictionary, the writer's own words, or the
    ///        checking went on or off. The editors check their text again.
    void wordsChanged();

    /// @brief A dictionary is loaded
    void dictionaryLoaded(const QString& dictionary);

    /// @brief A dictionary could not be loaded
    void dictionaryError(const QString& error);

private slots:
    /// @brief The dictionary loaded in the background is ready
    void onLoaderFinished();

private:
    struct OwnWords;  ///< The writer's own words, shared by services (shareOwnWordsWith())

    /// @brief Load the dictionary wanted in the background, unless it is loaded or loading
    void startLoader();

    /// @brief Check with a dictionary from now on, in place of the one loaded before
    void install(std::unique_ptr<Hunspell> hunspell, const QString& dictionary);

    /// @brief Stop waiting for the dictionary being loaded, dropping it
    void dropLoader();

    /// @brief Destroy a dictionary not used any more, now or once no dictionary is being
    ///        made (see the members)
    void retire(std::unique_ptr<Hunspell> hunspell);

    /// @brief Destroy the dictionaries not used any more, unless a dictionary is being made:
    ///        then a little later, so the window does not wait for it
    void destroyRetired();

    /// @brief Every service checking with the writer's own words checks its text again
    void emitOwnWordsChanged();

    /// @brief Whether a word (or a part of a hyphenated one) is right
    bool isWordCorrect(const QString& word) const;

    /// @brief Whether a word is one of the writer's own words
    bool isOwnWord(const QString& word) const;

    /// @brief A word as the dictionary takes it (in its encoding)
    std::string toDictionary(const QString& word) const;

    /// @brief A word of the dictionary
    QString fromDictionary(const std::string& word) const;

    /// @brief Save the user dictionary to its file
    void saveUserDictionary() const;

    // Hunspell keeps tables shared by all its dictionaries, so in the whole application they
    // are made and destroyed one at a time, under one lock: a dictionary is made on the
    // loader's thread, and one not used any more is destroyed on the window's thread while
    // no other one is being made (retire()).
    std::unique_ptr<Hunspell> m_hunspell;  ///< The last dictionary loaded
    QString m_dictionary;                  ///< Its name
    bool m_inUse = false;                  ///< The words are checked with it
    bool m_latin1 = false;                 ///< Its words are in ISO 8859-1, not UTF-8
    bool m_enabled = true;                 ///< The checking is on

    QString m_wantedDictionary;          ///< The dictionary to check with (empty: none)
    QThread* m_loader = nullptr;         ///< Loads a dictionary in the background
    QString m_loadingDictionary;         ///< The dictionary it loads
    std::unique_ptr<Hunspell> m_loaded;  ///< What it loaded (written on its thread)

    std::vector<std::unique_ptr<Hunspell>> m_retired;  ///< Not used any more, to destroy
    QTimer* m_retireTimer;                             ///< Destroys them a little later

    std::shared_ptr<OwnWords> m_ownWords;  ///< The writer's own words
};

}  // namespace kalahari::editor
