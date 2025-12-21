/// @file spell_check_service.cpp
/// @brief Spell checking service implementation (OpenSpec #00042 Phase 6.4-6.9)

#include <kalahari/editor/spell_check_service.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/core/logger.h>

#include <hunspell/hunspell.hxx>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QRegularExpression>

namespace kalahari::editor {

// =============================================================================
// DocumentObserver (Private Implementation)
// =============================================================================

/// @brief Private document observer for SpellCheckService
///
/// Implements IDocumentObserver to receive document change notifications
/// and forward them to the service for spell checking.
class SpellCheckService::DocumentObserver : public IDocumentObserver {
public:
    explicit DocumentObserver(SpellCheckService* service)
        : m_service(service)
    {}

    void onContentChanged() override {
        if (m_service) {
            m_service->onDocumentChanged();
        }
    }

    void onParagraphInserted(int index) override {
        if (m_service) {
            m_service->markParagraphForCheck(index);
        }
    }

    void onParagraphRemoved(int index) override {
        Q_UNUSED(index);
        // No action needed - paragraph is gone
    }

    void onParagraphModified(int index) override {
        if (m_service) {
            m_service->markParagraphForCheck(index);
        }
    }

private:
    SpellCheckService* m_service;
};

// =============================================================================
// SpellCheckService
// =============================================================================

SpellCheckService::SpellCheckService(QObject* parent)
    : QObject(parent)
    , m_observer(std::make_unique<DocumentObserver>(this))
    , m_debounceTimer(new QTimer(this))
{
    // Setup debounce timer
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(DEBOUNCE_MS);
    connect(m_debounceTimer, &QTimer::timeout, this, &SpellCheckService::onDebounceTimeout);

    // Load user dictionary
    loadUserDictionary();

    core::Logger::getInstance().debug("SpellCheckService created");
}

SpellCheckService::~SpellCheckService()
{
    // Disconnect from document
    if (m_document) {
        m_document->removeObserver(m_observer.get());
    }

    // Clean up Hunspell
    cleanupHunspell();

    core::Logger::getInstance().debug("SpellCheckService destroyed");
}

// =============================================================================
// Setup
// =============================================================================

void SpellCheckService::setDocument(KmlDocument* document)
{
    if (m_document == document) {
        return;
    }

    // Disconnect from previous document
    if (m_document) {
        m_document->removeObserver(m_observer.get());
    }

    m_document = document;

    // Clear pending checks
    m_pendingParagraphs.clear();

    // Connect to new document
    if (m_document) {
        m_document->addObserver(m_observer.get());

        // Mark all paragraphs for initial check
        if (m_enabled && isDictionaryLoaded()) {
            for (int i = 0; i < m_document->paragraphCount(); ++i) {
                m_pendingParagraphs.insert(i);
            }
            m_debounceTimer->start();
        }
    }
}

bool SpellCheckService::loadDictionary(const QString& language)
{
    // Find dictionary files
    QString dictPath = findDictionaryPath(language);
    if (dictPath.isEmpty()) {
        QString error = tr("Dictionary not found for language: %1").arg(language);
        core::Logger::getInstance().warn("SpellCheckService: {}", error.toStdString());
        emit dictionaryError(error);
        return false;
    }

    QString affPath = dictPath + "/" + language + ".aff";
    QString dicPath = dictPath + "/" + language + ".dic";

    // Check files exist
    if (!QFile::exists(affPath) || !QFile::exists(dicPath)) {
        QString error = tr("Dictionary files missing for: %1").arg(language);
        core::Logger::getInstance().warn("SpellCheckService: {} (aff: {}, dic: {})",
                                         error.toStdString(), affPath.toStdString(), dicPath.toStdString());
        emit dictionaryError(error);
        return false;
    }

    // Initialize Hunspell
    if (!initHunspell(affPath, dicPath)) {
        QString error = tr("Failed to initialize Hunspell for: %1").arg(language);
        emit dictionaryError(error);
        return false;
    }

    m_currentLanguage = language;

    // Add user dictionary words to Hunspell runtime dictionary
    Hunspell* hunspell = static_cast<Hunspell*>(m_hunspell);
    for (const QString& word : m_userDictionary) {
        hunspell->add(word.toStdString());
    }

    core::Logger::getInstance().info("SpellCheckService: Loaded dictionary for '{}'", language.toStdString());
    emit dictionaryLoaded(language);

    // Trigger check of current document if any
    if (m_document && m_enabled) {
        for (int i = 0; i < m_document->paragraphCount(); ++i) {
            m_pendingParagraphs.insert(i);
        }
        m_debounceTimer->start();
    }

    return true;
}

QStringList SpellCheckService::availableDictionaries() const
{
    QStringList dictionaries;
    QSet<QString> found;  // Avoid duplicates

    QStringList searchPaths = getSystemDictionaryPaths();

    for (const QString& path : searchPaths) {
        QDir dir(path);
        if (!dir.exists()) {
            continue;
        }

        // Find .aff files
        QStringList affFiles = dir.entryList(QStringList() << "*.aff", QDir::Files);
        for (const QString& affFile : affFiles) {
            QString lang = affFile.left(affFile.length() - 4);  // Remove .aff

            // Check that .dic file also exists
            if (dir.exists(lang + ".dic") && !found.contains(lang)) {
                found.insert(lang);
                dictionaries.append(lang);
            }
        }
    }

    dictionaries.sort();
    return dictionaries;
}

QString SpellCheckService::currentLanguage() const
{
    return m_currentLanguage;
}

// =============================================================================
// Checking
// =============================================================================

bool SpellCheckService::isCorrect(const QString& word) const
{
    if (!m_hunspell || word.isEmpty()) {
        return true;  // No dictionary = no errors
    }

    // Check ignored words first
    if (m_ignoredWords.contains(word.toLower())) {
        return true;
    }

    // Check user dictionary
    if (m_userDictionary.contains(word.toLower())) {
        return true;
    }

    Hunspell* hunspell = static_cast<Hunspell*>(m_hunspell);
    return hunspell->spell(word.toStdString()) != 0;
}

QStringList SpellCheckService::suggestions(const QString& word, int maxSuggestions) const
{
    QStringList result;

    if (!m_hunspell || word.isEmpty()) {
        return result;
    }

    Hunspell* hunspell = static_cast<Hunspell*>(m_hunspell);
    std::vector<std::string> suggests = hunspell->suggest(word.toStdString());

    for (size_t i = 0; i < suggests.size() && static_cast<int>(i) < maxSuggestions; ++i) {
        result.append(QString::fromStdString(suggests[i]));
    }

    return result;
}

QList<SpellErrorInfo> SpellCheckService::checkParagraph(const QString& text) const
{
    QList<SpellErrorInfo> errors;

    if (!m_hunspell || !m_enabled || text.isEmpty()) {
        return errors;
    }

    // Extract words with positions
    QList<QPair<int, QString>> words = extractWords(text);

    for (const auto& [pos, word] : words) {
        if (!isCorrect(word)) {
            SpellErrorInfo error(pos, word.length(), word);
            error.suggestions = suggestions(word, 5);
            errors.append(error);
        }
    }

    return errors;
}

void SpellCheckService::checkDocumentAsync()
{
    if (!m_document || !m_hunspell || !m_enabled) {
        emit documentCheckComplete();
        return;
    }

    // Mark all paragraphs for checking
    m_pendingParagraphs.clear();
    for (int i = 0; i < m_document->paragraphCount(); ++i) {
        m_pendingParagraphs.insert(i);
    }

    // Start debounce timer (will process all paragraphs when it fires)
    m_debounceTimer->start();
}

void SpellCheckService::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;

    if (enabled && m_document && isDictionaryLoaded()) {
        // Trigger full document check
        checkDocumentAsync();
    } else if (!enabled) {
        // Clear pending checks and clear errors from paragraphs
        m_pendingParagraphs.clear();
        m_debounceTimer->stop();

        // Emit empty error lists for all paragraphs to clear UI
        if (m_document) {
            for (int i = 0; i < m_document->paragraphCount(); ++i) {
                emit paragraphChecked(i, QList<SpellErrorInfo>());
            }
        }
    }
}

bool SpellCheckService::isEnabled() const
{
    return m_enabled;
}

bool SpellCheckService::isDictionaryLoaded() const
{
    return m_hunspell != nullptr;
}

// =============================================================================
// User Dictionary
// =============================================================================

void SpellCheckService::addToUserDictionary(const QString& word)
{
    QString lowerWord = word.toLower();

    if (m_userDictionary.contains(lowerWord)) {
        return;
    }

    m_userDictionary.insert(lowerWord);

    // Add to Hunspell runtime dictionary
    if (m_hunspell) {
        Hunspell* hunspell = static_cast<Hunspell*>(m_hunspell);
        hunspell->add(word.toStdString());
    }

    // Persist
    saveUserDictionary();

    core::Logger::getInstance().debug("SpellCheckService: Added '{}' to user dictionary", word.toStdString());

    // Re-check document to update UI
    if (m_document && m_enabled) {
        checkDocumentAsync();
    }
}

void SpellCheckService::ignoreWord(const QString& word)
{
    QString lowerWord = word.toLower();

    if (m_ignoredWords.contains(lowerWord)) {
        return;
    }

    m_ignoredWords.insert(lowerWord);

    core::Logger::getInstance().debug("SpellCheckService: Ignoring '{}' for this session", word.toStdString());

    // Re-check document to update UI
    if (m_document && m_enabled) {
        checkDocumentAsync();
    }
}

void SpellCheckService::removeFromUserDictionary(const QString& word)
{
    QString lowerWord = word.toLower();

    if (!m_userDictionary.contains(lowerWord)) {
        return;
    }

    m_userDictionary.remove(lowerWord);

    // Note: Hunspell doesn't have a remove method, so we need to reload
    // For now, the word will still pass until dictionary is reloaded

    // Persist
    saveUserDictionary();

    core::Logger::getInstance().debug("SpellCheckService: Removed '{}' from user dictionary", word.toStdString());
}

bool SpellCheckService::isInUserDictionary(const QString& word) const
{
    return m_userDictionary.contains(word.toLower());
}

QStringList SpellCheckService::userDictionaryWords() const
{
    QStringList words(m_userDictionary.begin(), m_userDictionary.end());
    words.sort();
    return words;
}

// =============================================================================
// Private Slots
// =============================================================================

void SpellCheckService::onDebounceTimeout()
{
    if (!m_document || !m_hunspell || !m_enabled) {
        m_pendingParagraphs.clear();
        return;
    }

    // Check all pending paragraphs
    for (int idx : m_pendingParagraphs) {
        if (idx < 0 || idx >= m_document->paragraphCount()) {
            continue;
        }

        const KmlParagraph* para = m_document->paragraph(idx);
        if (!para) {
            continue;
        }

        QString text = para->plainText();
        QList<SpellErrorInfo> errors = checkParagraph(text);

        emit paragraphChecked(idx, errors);
    }

    m_pendingParagraphs.clear();
    emit documentCheckComplete();
}

// =============================================================================
// Hunspell Management
// =============================================================================

bool SpellCheckService::initHunspell(const QString& affPath, const QString& dicPath)
{
    // Clean up existing instance
    cleanupHunspell();

    try {
        Hunspell* hunspell = new Hunspell(affPath.toLocal8Bit().constData(),
                                          dicPath.toLocal8Bit().constData());
        m_hunspell = hunspell;

        core::Logger::getInstance().debug("SpellCheckService: Hunspell initialized (aff: {}, dic: {})",
                                          affPath.toStdString(), dicPath.toStdString());
        return true;
    } catch (const std::exception& e) {
        core::Logger::getInstance().error("SpellCheckService: Failed to initialize Hunspell: {}",
                                          e.what());
        return false;
    }
}

void SpellCheckService::cleanupHunspell()
{
    if (m_hunspell) {
        Hunspell* hunspell = static_cast<Hunspell*>(m_hunspell);
        delete hunspell;
        m_hunspell = nullptr;
        m_currentLanguage.clear();
    }
}

// =============================================================================
// Dictionary Paths
// =============================================================================

QString SpellCheckService::findDictionaryPath(const QString& language) const
{
    QStringList searchPaths = getSystemDictionaryPaths();

    for (const QString& path : searchPaths) {
        QDir dir(path);
        if (dir.exists(language + ".aff") && dir.exists(language + ".dic")) {
            return path;
        }
    }

    return QString();
}

QStringList SpellCheckService::getSystemDictionaryPaths() const
{
    QStringList paths;

    // 1. Application directory: ./dictionaries/
    QString appDir = QCoreApplication::applicationDirPath();
    paths.append(appDir + "/dictionaries");
    paths.append(appDir + "/resources/dictionaries");

    // 2. User data location: AppData/Kalahari/dictionaries/
    QString userDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    paths.append(userDataPath + "/dictionaries");

#ifdef Q_OS_WIN
    // 3. Windows: Check common locations
    // LibreOffice extensions
    paths.append("C:/Program Files/LibreOffice/share/extensions/dict-pl");
    paths.append("C:/Program Files/LibreOffice/share/extensions/dict-en");
    paths.append("C:/Program Files (x86)/LibreOffice/share/extensions/dict-pl");
    paths.append("C:/Program Files (x86)/LibreOffice/share/extensions/dict-en");

    // Hunspell default location on Windows
    paths.append("C:/Program Files/hunspell/share/hunspell");
    paths.append("C:/hunspell");
#else
    // 4. Linux/macOS: System paths
    paths.append("/usr/share/hunspell");
    paths.append("/usr/share/myspell");
    paths.append("/usr/share/myspell/dicts");
    paths.append("/usr/local/share/hunspell");

    // LibreOffice on Linux
    paths.append("/usr/share/libreoffice/share/extensions/dict-pl");
    paths.append("/usr/share/libreoffice/share/extensions/dict-en");

    // macOS
    paths.append("/Library/Spelling");
    paths.append(QDir::homePath() + "/Library/Spelling");
#endif

    return paths;
}

// =============================================================================
// Word Extraction
// =============================================================================

QList<QPair<int, QString>> SpellCheckService::extractWords(const QString& text) const
{
    QList<QPair<int, QString>> words;

    // Unicode-aware word extraction
    // Matches sequences of Unicode letters (including Polish, German, etc.)
    static QRegularExpression wordRe("\\b(\\p{L}+)\\b");

    QRegularExpressionMatchIterator it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int pos = match.capturedStart();
        QString word = match.captured();

        // Skip very short words (typically not spell-checked)
        if (word.length() >= 2) {
            words.append({pos, word});
        }
    }

    return words;
}

// =============================================================================
// User Dictionary Persistence
// =============================================================================

void SpellCheckService::loadUserDictionary()
{
    QString path = userDictionaryPath();

    QFile file(path);
    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        core::Logger::getInstance().warn("SpellCheckService: Failed to open user dictionary: {}",
                                         path.toStdString());
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed().toLower();
        if (!line.isEmpty() && !line.startsWith('#')) {
            m_userDictionary.insert(line);
        }
    }

    core::Logger::getInstance().debug("SpellCheckService: Loaded {} words from user dictionary",
                                      m_userDictionary.size());
}

void SpellCheckService::saveUserDictionary()
{
    QString path = userDictionaryPath();

    // Ensure directory exists
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        core::Logger::getInstance().error("SpellCheckService: Failed to save user dictionary: {}",
                                          path.toStdString());
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Write header
    out << "# Kalahari User Dictionary\n";
    out << "# One word per line, UTF-8 encoded\n";
    out << "#\n";

    // Write words sorted
    QStringList words(m_userDictionary.begin(), m_userDictionary.end());
    words.sort();

    for (const QString& word : words) {
        out << word << "\n";
    }

    core::Logger::getInstance().debug("SpellCheckService: Saved {} words to user dictionary",
                                      m_userDictionary.size());
}

QString SpellCheckService::userDictionaryPath() const
{
    QString userDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return userDataPath + "/user_dictionary.txt";
}

// =============================================================================
// Document Observer
// =============================================================================

void SpellCheckService::onDocumentChanged()
{
    // Full document might have changed - re-check all
    if (m_document && m_enabled && isDictionaryLoaded()) {
        for (int i = 0; i < m_document->paragraphCount(); ++i) {
            m_pendingParagraphs.insert(i);
        }
        m_debounceTimer->start();
    }
}

void SpellCheckService::markParagraphForCheck(int index)
{
    if (m_enabled && isDictionaryLoaded()) {
        m_pendingParagraphs.insert(index);
        m_debounceTimer->start();
    }
}

}  // namespace kalahari::editor
