/// @file spell_check_service.cpp
/// @brief The spelling dictionary every editor checks its text with (Hunspell)

#include <kalahari/editor/spell_check_service.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/resource_paths.h>

#include <hunspell/hunspell.hxx>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringView>
#include <QTextStream>
#include <QThread>

#include <algorithm>
#include <utility>
#include <vector>

namespace kalahari::editor {

namespace {

// =============================================================================
// Dictionary files
// =============================================================================

/// @brief The folders dictionaries are looked for in, the first first
QStringList dictionaryFolders() {
    QStringList folders;

    // The dictionaries shipped with Kalahari, in its resources
    const QString resourcesDir = core::ResourcePaths::getInstance().getResourcesDir();
    if (!resourcesDir.isEmpty()) {
        folders.append(resourcesDir + QStringLiteral("/dictionaries"));
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    folders.append(appDir + QStringLiteral("/dictionaries"));
    folders.append(appDir + QStringLiteral("/resources/dictionaries"));

    // The writer's own: AppData/Kalahari/dictionaries
    folders.append(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                   QStringLiteral("/dictionaries"));

#ifdef Q_OS_WIN
    // LibreOffice's and Hunspell's
    folders.append(QStringLiteral("C:/Program Files/LibreOffice/share/extensions/dict-pl"));
    folders.append(QStringLiteral("C:/Program Files/LibreOffice/share/extensions/dict-en"));
    folders.append(QStringLiteral("C:/Program Files (x86)/LibreOffice/share/extensions/dict-pl"));
    folders.append(QStringLiteral("C:/Program Files (x86)/LibreOffice/share/extensions/dict-en"));
    folders.append(QStringLiteral("C:/Program Files/hunspell/share/hunspell"));
    folders.append(QStringLiteral("C:/hunspell"));
#else
    // The system's
    folders.append(QStringLiteral("/usr/share/hunspell"));
    folders.append(QStringLiteral("/usr/share/myspell"));
    folders.append(QStringLiteral("/usr/share/myspell/dicts"));
    folders.append(QStringLiteral("/usr/local/share/hunspell"));
    folders.append(QStringLiteral("/usr/share/libreoffice/share/extensions/dict-pl"));
    folders.append(QStringLiteral("/usr/share/libreoffice/share/extensions/dict-en"));
    folders.append(QStringLiteral("/Library/Spelling"));
    folders.append(QDir::homePath() + QStringLiteral("/Library/Spelling"));
#endif
    return folders;
}

/// @brief The folder with a dictionary's .aff and .dic files, or empty
QString folderOf(const QString& dictionary) {
    if (dictionary.isEmpty()) {
        return QString();
    }
    for (const QString& folder : dictionaryFolders()) {
        const QDir dir(folder);
        if (dir.exists(dictionary + QStringLiteral(".aff")) &&
            dir.exists(dictionary + QStringLiteral(".dic"))) {
            return folder;
        }
    }
    return QString();
}

/// @brief A file's path as Hunspell opens it
QByteArray hunspellPath(const QString& path) {
#ifdef _MSC_VER
    // A UTF-8 path after the long path prefix opens whatever the system's code page
    return QByteArrayLiteral("\\\\?\\") +
           QDir::toNativeSeparators(QDir::cleanPath(QFileInfo(path).absoluteFilePath())).toUtf8();
#else
    return QFile::encodeName(path);
#endif
}

// =============================================================================
// Words
// =============================================================================

/// @brief A word of a text to check
struct WordSpan {
    qsizetype start = 0;     ///< Where it starts in the text
    qsizetype length = 0;    ///< How long it is (in UTF-16 units)
    int letters = 0;         ///< How many letters it has
    bool lowercase = false;  ///< It has a lowercase letter
};

/// @brief The character at a position, and how many UTF-16 units it takes
char32_t codePointAt(QStringView text, qsizetype i, qsizetype* size) {
    const QChar c = text[i];
    if (c.isHighSurrogate() && i + 1 < text.size() && text[i + 1].isLowSurrogate()) {
        *size = 2;
        return QChar::surrogateToUcs4(c, text[i + 1]);
    }
    *size = 1;
    return c.unicode();
}

/// @brief The character before a position
char32_t codePointBefore(QStringView text, qsizetype i) {
    const QChar c = text[i - 1];
    if (c.isLowSurrogate() && i >= 2 && text[i - 2].isHighSurrogate()) {
        return QChar::surrogateToUcs4(text[i - 2], c);
    }
    return c.unicode();
}

/// @brief A hyphen: joins words into one (well-known)
bool isHyphen(char32_t c) {
    return c == U'-' || c == U'\u2010' || c == U'\u2011';
}

/// @brief An apostrophe or a hyphen: joins the letters around it into one word
bool isJoiner(char32_t c) {
    return c == U'\'' || c == U'\u2019' || isHyphen(c);
}

/// @brief A digit or an underscore: a word touching one is a code or a name (abc123, x_y)
bool isCodeCharacter(char32_t c) {
    return QChar::isNumber(c) || c == U'_';
}

/// @brief Whether a run of text without spaces is a web or e-mail address
bool isAddress(QStringView chunk) {
    return chunk.contains(u"://") || chunk.contains(u'@') ||
           chunk.startsWith(u"www.", Qt::CaseInsensitive);
}

/// @brief Add the words of a run of text without spaces
/// @param offset Where the run starts in the text
void addWords(QStringView chunk, qsizetype offset, QList<WordSpan>& words) {
    const qsizetype n = chunk.size();
    qsizetype i = 0;
    while (i < n) {
        qsizetype size = 0;
        if (!QChar::isLetter(codePointAt(chunk, i, &size))) {
            i += size;
            continue;
        }

        // Letters (with their accents), also joined by apostrophes and hyphens
        WordSpan word;
        qsizetype j = i;
        while (j < n) {
            const char32_t c = codePointAt(chunk, j, &size);
            if (QChar::isLetter(c)) {
                ++word.letters;
                word.lowercase = word.lowercase || QChar::isLower(c);
                j += size;
            } else if (qsizetype next = 0;
                       QChar::isMark(c) ||
                       (isJoiner(c) && j + size < n &&
                        QChar::isLetter(codePointAt(chunk, j + size, &next)))) {
                j += size;  // an accent, or an apostrophe or a hyphen before a letter
            } else {
                break;
            }
        }

        const bool touchesCode = (i > 0 && isCodeCharacter(codePointBefore(chunk, i))) ||
                                 (j < n && isCodeCharacter(codePointAt(chunk, j, &size)));
        if (!touchesCode) {
            word.start = offset + i;
            word.length = j - i;
            words.append(word);
        }
        i = j;
    }
}

/// @brief The words of a text, in their order, but those in addresses
QList<WordSpan> wordsOf(QStringView text) {
    QList<WordSpan> words;
    const qsizetype n = text.size();
    qsizetype i = 0;
    while (i < n) {
        while (i < n && text[i].isSpace()) {
            ++i;
        }
        const qsizetype start = i;
        while (i < n && !text[i].isSpace()) {
            ++i;
        }
        const QStringView chunk = text.sliced(start, i - start);
        if (!chunk.isEmpty() && !isAddress(chunk)) {
            addWords(chunk, start, words);
        }
    }
    return words;
}

/// @brief Whether a word is checked: of two letters or more, not in capitals (NATO)
bool isChecked(const WordSpan& word) {
    return word.letters >= 2 && word.lowercase;
}

/// @brief Whether a word has a hyphen
bool hasHyphen(QStringView word) {
    return std::any_of(word.begin(), word.end(),
                       [](QChar c) { return isHyphen(c.unicode()); });
}

/// @brief A hyphenated word with spaces in place of its hyphens: its parts as words
QString partsOf(const QString& word) {
    QString parts = word;
    for (QChar& c : parts) {
        if (isHyphen(c.unicode())) {
            c = QLatin1Char(' ');
        }
    }
    return parts;
}

// =============================================================================
// Suggestions
// =============================================================================

/// How far a suggestion is from the word typed: the cost of each change
constexpr int ACCENT_COST = 1;  ///< A letter typed without its accent, or with another one
constexpr int SWAP_COST = 2;    ///< Two letters next to each other swapped
constexpr int LETTER_COST = 4;  ///< A letter wrong, missing or extra

/// @brief Whether a word has a capital first letter and no other (Ktory, Teh): it starts a
///        sentence, or it is a name
bool isCapitalized(const QString& word) {
    if (word.size() < 2 || !word.front().isUpper()) {
        return false;
    }
    const QString rest = word.sliced(1);
    return rest == rest.toLower();
}

/// @brief A word with a capital first letter (The for the)
QString capitalized(const QString& word) {
    if (word.isEmpty() || !word.front().isLower()) {
        return word;
    }
    return word.front().toUpper() + word.sliced(1);
}

/// @brief A letter in lower case without its accent (o for O with an acute, l for L with a
///        stroke)
QChar withoutAccent(QChar letter) {
    const QChar lower = letter.toLower();
    // Letters with a stroke are not accented letters for Unicode
    switch (lower.unicode()) {
    case 0x0142:  // l with stroke
        return QLatin1Char('l');
    case 0x0111:  // d with stroke
        return QLatin1Char('d');
    case 0x00F8:  // o with stroke
        return QLatin1Char('o');
    default:
        break;
    }
    const QString decomposed = QString(lower).normalized(QString::NormalizationForm_D);
    return decomposed.isEmpty() ? lower : decomposed.front();
}

/// @brief A word in lower case without its accents: the letters of a word typed without them
QString lettersOf(const QString& word) {
    QString letters;
    letters.reserve(word.size());
    for (const QChar c : word) {
        if (c.category() != QChar::Mark_NonSpacing) {
            letters.append(withoutAccent(c));
        }
    }
    return letters;
}

/// @brief How far a suggestion is from the word typed, in any case (see the costs above)
int distanceBetween(const QString& typed, const QString& suggestion) {
    const QString a = typed.toLower();
    const QString b = suggestion.toLower();
    const qsizetype columns = b.size() + 1;

    // The cost of turning the first i letters of the one into the first j of the other
    std::vector<int> costs(static_cast<std::size_t>((a.size() + 1) * columns));
    const auto cost = [&costs, columns](qsizetype i, qsizetype j) -> int& {
        return costs[static_cast<std::size_t>((i * columns) + j)];
    };
    for (qsizetype i = 0; i <= a.size(); ++i) {
        cost(i, 0) = static_cast<int>(i) * LETTER_COST;
    }
    for (qsizetype j = 0; j <= b.size(); ++j) {
        cost(0, j) = static_cast<int>(j) * LETTER_COST;
    }
    for (qsizetype i = 1; i <= a.size(); ++i) {
        for (qsizetype j = 1; j <= b.size(); ++j) {
            int change = 0;
            if (a[i - 1] != b[j - 1]) {
                change = withoutAccent(a[i - 1]) == withoutAccent(b[j - 1]) ? ACCENT_COST
                                                                            : LETTER_COST;
            }
            int best = std::min({cost(i - 1, j) + LETTER_COST, cost(i, j - 1) + LETTER_COST,
                                 cost(i - 1, j - 1) + change});
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                best = std::min(best, cost(i - 2, j - 2) + SWAP_COST);
            }
            cost(i, j) = best;
        }
    }
    return cost(a.size(), b.size());
}

/// @brief Two lists in turns, from the first one (a1 b1 a2 b2 ...)
QStringList inTurns(const QStringList& first, const QStringList& second) {
    QStringList merged;
    merged.reserve(first.size() + second.size());
    for (qsizetype i = 0; i < std::max(first.size(), second.size()); ++i) {
        if (i < first.size()) {
            merged.append(first[i]);
        }
        if (i < second.size()) {
            merged.append(second[i]);
        }
    }
    return merged;
}

}  // anonymous namespace

// =============================================================================
// Construction
// =============================================================================

SpellCheckService::SpellCheckService(QObject* parent)
    : QObject(parent)
{
}

SpellCheckService::~SpellCheckService()
{
    dropLoader();
}

// =============================================================================
// Dictionaries
// =============================================================================

QStringList SpellCheckService::availableDictionaries()
{
    QStringList dictionaries;
    for (const QString& folder : dictionaryFolders()) {
        const QDir dir(folder);
        if (!dir.exists()) {
            continue;
        }
        const QStringList affixFiles = dir.entryList({QStringLiteral("*.aff")}, QDir::Files);
        for (const QString& affixFile : affixFiles) {
            const QString dictionary = QFileInfo(affixFile).completeBaseName();
            if (dir.exists(dictionary + QStringLiteral(".dic")) &&
                !dictionaries.contains(dictionary)) {
                dictionaries.append(dictionary);
            }
        }
    }
    dictionaries.sort();
    return dictionaries;
}

QString SpellCheckService::dictionaryFor(const QString& language)
{
    QString code = language.trimmed();
    code.replace(QLatin1Char('-'), QLatin1Char('_'));
    if (code.isEmpty()) {
        return QString();
    }
    const QStringList dictionaries = availableDictionaries();

    // A full code such as pl_PL or en_GB names the dictionary itself
    for (const QString& dictionary : dictionaries) {
        if (dictionary.compare(code, Qt::CaseInsensitive) == 0) {
            return dictionary;
        }
    }

    // A language such as "pl" or "en": its main dictionary (pl_PL, en_US), else any of it
    const QString lang = code.section(QLatin1Char('_'), 0, 0).toLower();
    const QString mainDictionary =
        lang + QLatin1Char('_') +
        (lang == QLatin1String("en") ? QStringLiteral("US") : lang.toUpper());
    if (dictionaries.contains(mainDictionary)) {
        return mainDictionary;
    }
    for (const QString& dictionary : dictionaries) {
        if (dictionary.section(QLatin1Char('_'), 0, 0).compare(lang, Qt::CaseInsensitive) == 0) {
            return dictionary;
        }
    }
    return QString();
}

bool SpellCheckService::loadDictionary(const QString& dictionary)
{
    const QString folder = folderOf(dictionary);
    if (folder.isEmpty()) {
        const QString error = tr("No dictionary %1 was found").arg(dictionary);
        core::Logger::getInstance().warn("SpellCheckService: {}", error.toStdString());
        emit dictionaryError(error);
        return false;
    }

    dropLoader();
    m_wantedDictionary = dictionary;
    install(std::make_unique<Hunspell>(
                hunspellPath(folder + QLatin1Char('/') + dictionary + QStringLiteral(".aff"))
                    .constData(),
                hunspellPath(folder + QLatin1Char('/') + dictionary + QStringLiteral(".dic"))
                    .constData()),
            dictionary);
    return true;
}

void SpellCheckService::loadDictionaryInBackground(const QString& dictionary)
{
    m_wantedDictionary = dictionary;
    startLoader();
}

bool SpellCheckService::isLoading() const
{
    return m_loader != nullptr;
}

void SpellCheckService::unloadDictionary()
{
    // The dictionary stays made until another one is (see the members), unused
    m_wantedDictionary.clear();
    if (m_inUse) {
        m_inUse = false;
        core::Logger::getInstance().info("SpellCheckService: No dictionary checks the words");
        emit wordsChanged();
    }
}

QString SpellCheckService::currentDictionary() const
{
    return m_inUse ? m_dictionary : QString();
}

bool SpellCheckService::isDictionaryLoaded() const
{
    return m_inUse;
}

void SpellCheckService::startLoader()
{
    if (m_loader != nullptr || m_wantedDictionary.isEmpty()) {
        return;  // after the one being loaded (onLoaderFinished())
    }
    if (m_hunspell && m_dictionary == m_wantedDictionary) {
        if (!m_inUse) {
            m_inUse = true;  // made before
            emit wordsChanged();
        }
        return;
    }

    const QString dictionary = m_wantedDictionary;
    const QString folder = folderOf(dictionary);
    if (folder.isEmpty()) {
        const QString error = tr("No dictionary %1 was found").arg(dictionary);
        core::Logger::getInstance().warn("SpellCheckService: {}", error.toStdString());
        emit dictionaryError(error);
        return;
    }

    const QByteArray affixPath =
        hunspellPath(folder + QLatin1Char('/') + dictionary + QStringLiteral(".aff"));
    const QByteArray wordsPath =
        hunspellPath(folder + QLatin1Char('/') + dictionary + QStringLiteral(".dic"));
    m_loadingDictionary = dictionary;
    m_loader = QThread::create([this, affixPath, wordsPath]() {
        m_loaded = std::make_unique<Hunspell>(affixPath.constData(), wordsPath.constData());
    });
    connect(m_loader, &QThread::finished, this, &SpellCheckService::onLoaderFinished);
    m_loader->start(QThread::LowPriority);
}

void SpellCheckService::onLoaderFinished()
{
    // The notice of a loader dropped before (loadDictionary()) finds nothing to take
    if (m_loader == nullptr || !m_loader->isFinished()) {
        return;
    }
    m_loader->wait();
    delete m_loader;
    m_loader = nullptr;
    std::unique_ptr<Hunspell> loaded = std::move(m_loaded);
    const QString dictionary = std::exchange(m_loadingDictionary, QString());

    if (dictionary == m_wantedDictionary) {
        install(std::move(loaded), dictionary);
    } else {
        // Another dictionary is wanted now: this one goes before it is loaded
        loaded.reset();
        startLoader();
    }
}

void SpellCheckService::dropLoader()
{
    if (m_loader == nullptr) {
        return;
    }
    m_loader->wait();
    delete m_loader;
    m_loader = nullptr;
    m_loaded.reset();
    m_loadingDictionary.clear();
}

void SpellCheckService::install(std::unique_ptr<Hunspell> hunspell, const QString& dictionary)
{
    // Words go to the dictionary in its encoding: UTF-8 (the shipped ones) or ISO 8859-1
    const std::string encoding = hunspell->get_dict_encoding();
    m_latin1 = encoding == "ISO8859-1";
    if (encoding != "UTF-8" && !m_latin1) {
        core::Logger::getInstance().warn(
            "SpellCheckService: Dictionary {} is in {}; words with letters outside ASCII will "
            "be reported as misspelled",
            dictionary.toStdString(), encoding);
    }

    m_hunspell = std::move(hunspell);
    m_dictionary = dictionary;
    m_inUse = true;
    core::Logger::getInstance().info("SpellCheckService: Dictionary {} loaded",
                                     dictionary.toStdString());
    emit dictionaryLoaded(dictionary);
    emit wordsChanged();
}

// =============================================================================
// Checking
// =============================================================================

void SpellCheckService::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit wordsChanged();
    }
}

bool SpellCheckService::isEnabled() const
{
    return m_enabled;
}

bool SpellCheckService::isActive() const
{
    return m_enabled && m_inUse;
}

bool SpellCheckService::isCorrect(const QString& word) const
{
    if (!m_inUse || word.isEmpty()) {
        return true;
    }
    if (isWordCorrect(word)) {
        return true;
    }

    // A hyphenated word whose every part is right
    if (!hasHyphen(word)) {
        return false;
    }
    for (const WordSpan& part : wordsOf(partsOf(word))) {
        if (isChecked(part) && !isWordCorrect(word.mid(part.start, part.length))) {
            return false;
        }
    }
    return true;
}

QStringList SpellCheckService::suggestions(const QString& word, int maxSuggestions) const
{
    QStringList result;
    if (!m_inUse || word.isEmpty()) {
        return result;
    }
    QString asked = word;
    asked.replace(QChar(0x2019), QLatin1Char('\''));
    const bool capitalizedWord = isCapitalized(asked);
    const auto suggest = [this, &asked](const QString& misspelled, bool capitalize) {
        QStringList found;
        for (const std::string& suggestion : m_hunspell->suggest(toDictionary(misspelled))) {
            const QString offered = fromDictionary(suggestion);
            found.append(capitalize ? capitalized(offered) : offered);
        }
        found.removeAll(asked);
        return found;
    };
    QStringList found = suggest(asked, false);

    // A word with a capital first letter, at the start of a sentence or a name: for it the
    // dictionary offers names and capitalized words first (Kory and Tory for the Polish
    // "Ktory" typed without its accent) and the words written small only later. Its
    // suggestions for the word written small go in turns with them, from the list whose first
    // word is closer to the word typed.
    if (capitalizedWord) {
        const QStringList small = suggest(asked.toLower(), true);
        const bool namesFirst = !found.isEmpty() &&
                                (small.isEmpty() || distanceBetween(asked, found.front()) <
                                                        distanceBetween(asked, small.front()));
        found = namesFirst ? inTurns(found, small) : inTurns(small, found);
    }

    // The writer's own words close to the word typed (the names of the book): before the
    // suggestions not closer to it than they are
    const int farthest = lettersOf(asked).size() <= 4 ? LETTER_COST : 2 * LETTER_COST;
    QList<std::pair<int, QString>> ownWords;
    for (const QString& own : m_userWords) {
        const QString offered = capitalizedWord ? capitalized(own) : own;
        const int distance = distanceBetween(asked, offered);
        if (distance <= farthest && offered != asked) {
            ownWords.append({distance, offered});
        }
    }
    std::sort(ownWords.begin(), ownWords.end());
    for (const std::pair<int, QString>& own : std::as_const(ownWords)) {
        const int distance = own.first;
        const auto notCloser =
            std::find_if(found.begin(), found.end(), [&asked, distance](const QString& offered) {
                return distanceBetween(asked, offered) >= distance;
            });
        found.insert(notCloser, own.second);
    }

    // The word typed without its accents, with them put right, comes first
    const QString typedLetters = lettersOf(asked);
    std::stable_partition(found.begin(), found.end(), [&typedLetters](const QString& offered) {
        return lettersOf(offered) == typedLetters;
    });

    for (const QString& suggestion : std::as_const(found)) {
        if (result.size() >= maxSuggestions) {
            break;
        }
        if (!result.contains(suggestion)) {
            result.append(suggestion);
        }
    }
    return result;
}

QList<SpellErrorInfo> SpellCheckService::checkParagraph(const QString& text) const
{
    QList<SpellErrorInfo> errors;
    if (!isActive() || text.isEmpty()) {
        return errors;
    }

    for (const WordSpan& span : wordsOf(text)) {
        if (!isChecked(span)) {
            continue;
        }
        const QString word = text.mid(span.start, span.length);
        if (isWordCorrect(word)) {
            continue;
        }
        if (!hasHyphen(word)) {
            errors.append(SpellErrorInfo(static_cast<int>(span.start),
                                         static_cast<int>(span.length), word));
            continue;
        }

        // A hyphenated word wrong as a whole: its wrong parts (none: it is right)
        for (const WordSpan& part : wordsOf(partsOf(word))) {
            const QString partWord = word.mid(part.start, part.length);
            if (isChecked(part) && !isWordCorrect(partWord)) {
                errors.append(SpellErrorInfo(static_cast<int>(span.start + part.start),
                                             static_cast<int>(part.length), partWord));
            }
        }
    }
    return errors;
}

bool SpellCheckService::isWordCorrect(const QString& word) const
{
    if (isOwnWord(word)) {
        return true;
    }
    // The dictionaries know the typewriter apostrophe (don't)
    QString asked = word;
    asked.replace(QChar(0x2019), QLatin1Char('\''));
    return m_hunspell->spell(toDictionary(asked));
}

bool SpellCheckService::isOwnWord(const QString& word) const
{
    if (m_userWords.isEmpty() && m_ignoredWords.isEmpty()) {
        return false;
    }
    const auto known = [this](const QString& candidate) {
        return m_userWords.contains(candidate) || m_ignoredWords.contains(candidate);
    };
    // As it is written, or in lower case (Kalahari for kalahari)
    const auto knownInAnyCase = [&known](const QString& candidate) {
        return known(candidate) || known(candidate.toLower());
    };
    if (knownInAnyCase(word)) {
        return true;
    }

    // With an ending after an apostrophe (Kalahari's)
    for (qsizetype i = 1; i < word.size(); ++i) {
        if (word[i] == QLatin1Char('\'') || word[i] == QChar(0x2019)) {
            return knownInAnyCase(word.left(i));
        }
    }
    return false;
}

std::string SpellCheckService::toDictionary(const QString& word) const
{
    return m_latin1 ? word.toLatin1().toStdString() : word.toStdString();
}

QString SpellCheckService::fromDictionary(const std::string& word) const
{
    return m_latin1 ? QString::fromLatin1(word.data(), static_cast<qsizetype>(word.size()))
                    : QString::fromStdString(word);
}

// =============================================================================
// The writer's own words
// =============================================================================

void SpellCheckService::setUserDictionaryFile(const QString& path)
{
    m_userDictionaryFile = path;
    QSet<QString> words;
    QFile file(path);
    if (!path.isEmpty() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
                words.insert(line);
            }
        }
        core::Logger::getInstance().debug("SpellCheckService: {} words in the user dictionary",
                                          words.size());
    }
    if (words != m_userWords) {
        m_userWords = std::move(words);
        emit wordsChanged();
    }
}

QString SpellCheckService::userDictionaryFile() const
{
    return m_userDictionaryFile;
}

void SpellCheckService::addToUserDictionary(const QString& word)
{
    const QString added = word.trimmed();
    if (added.isEmpty() || m_userWords.contains(added)) {
        return;
    }
    m_userWords.insert(added);
    saveUserDictionary();
    emit wordsChanged();
}

void SpellCheckService::removeFromUserDictionary(const QString& word)
{
    if (m_userWords.remove(word.trimmed())) {
        saveUserDictionary();
        emit wordsChanged();
    }
}

void SpellCheckService::setUserDictionaryWords(const QStringList& words)
{
    QSet<QString> kept;
    for (const QString& word : words) {
        const QString trimmed = word.trimmed();
        if (!trimmed.isEmpty()) {
            kept.insert(trimmed);
        }
    }
    if (kept != m_userWords) {
        m_userWords = std::move(kept);
        saveUserDictionary();
        emit wordsChanged();
    }
}

bool SpellCheckService::isInUserDictionary(const QString& word) const
{
    return m_userWords.contains(word);
}

QStringList SpellCheckService::userDictionaryWords() const
{
    QStringList words(m_userWords.begin(), m_userWords.end());
    words.sort();
    return words;
}

void SpellCheckService::ignoreWord(const QString& word)
{
    const QString ignored = word.trimmed();
    if (ignored.isEmpty() || m_ignoredWords.contains(ignored)) {
        return;
    }
    m_ignoredWords.insert(ignored);
    emit wordsChanged();
}

bool SpellCheckService::isIgnored(const QString& word) const
{
    return m_ignoredWords.contains(word);
}

void SpellCheckService::saveUserDictionary() const
{
    if (m_userDictionaryFile.isEmpty()) {
        return;
    }
    QDir().mkpath(QFileInfo(m_userDictionaryFile).absolutePath());

    // Written whole, in place of the old file only when it is complete
    QSaveFile file(m_userDictionaryFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        core::Logger::getInstance().error("SpellCheckService: Cannot write the user dictionary {}",
                                          m_userDictionaryFile.toStdString());
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "# Kalahari user dictionary: one word per line, UTF-8\n";
    for (const QString& word : userDictionaryWords()) {
        out << word << '\n';
    }
    out.flush();
    if (!file.commit()) {
        core::Logger::getInstance().error("SpellCheckService: Cannot write the user dictionary {}",
                                          m_userDictionaryFile.toStdString());
    }
}

}  // namespace kalahari::editor
