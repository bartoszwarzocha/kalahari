// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file affix_expander.cpp
/// @brief The forms a Hunspell dictionary gives its words: each word with its suffixes

#include <kalahari/core/affix_expander.h>

#include <QFile>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari::core {

namespace {

/// Fields of the line that starts the suffixes of a flag: SFX, the flag, Y or N, their count
constexpr qsizetype SUFFIX_HEADER_FIELDS = 4;

/// Fields of a suffix: SFX, the flag, what it strips, what it adds, its condition
constexpr qsizetype SUFFIX_FIELDS = 5;

/// Where the fields of a suffix are
constexpr qsizetype FLAG_FIELD = 1;
constexpr qsizetype STRIP_FIELD = 2;
constexpr qsizetype ADD_FIELD = 3;
constexpr qsizetype CONDITION_FIELD = 4;

/// Characters of a flag of FLAG long
constexpr qsizetype LONG_FLAG_SIZE = 2;

/// "0" in a field of a suffix: nothing
constexpr QLatin1String NOTHING("0");

/// Whether @p text is a whole number written with digits
bool isNumber(const QString& text) {
    return !text.isEmpty() &&
           std::ranges::all_of(text, [](QChar character) { return character.isDigit(); });
}

/// The encoding a SET line names, without hyphens and in upper case ("UTF8", "ISO88591")
QString encodingName(const QByteArray& affixBytes) {
    static const QRegularExpression setLine(QStringLiteral("^SET\\s+(\\S+)"),
                                            QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = setLine.match(QString::fromLatin1(affixBytes));
    if (!match.hasMatch()) {
        return QStringLiteral("ISO88591");  // Hunspell's default
    }
    return match.captured(1).toUpper().remove(QLatin1Char('-'));
}

/// Whether @p byte ends the word of an entry of the .dic file
bool endsWord(char byte) {
    return byte == '/' || byte == '\n' || byte == '\r' || byte == '\t' || byte == ' ';
}

/// Append @p text to @p list unless it has it
void appendOnce(QStringList& list, const QString& text) {
    if (!list.contains(text)) {
        list.append(text);
    }
}

/// Where the word of an entry of the .dic file that goes on at @p from ends
qsizetype wordEnd(const QByteArray& words, qsizetype from) {
    qsizetype end = from;
    while (end < words.size() && !endsWord(words.at(end))) {
        ++end;
    }
    return end;
}

/// A word a form may be a form of, with the flags whose suffixes make the form of it
using Root = std::pair<QString, QStringList>;

/// Add @p root with @p flag to @p roots: the root once, each of its flags once
void addRoot(QList<Root>& roots, const QString& root, const QString& flag) {
    const auto known = std::ranges::find(roots, root, &Root::first);
    if (known == roots.end()) {
        roots.append({root, {flag}});
    } else {
        appendOnce(known->second, flag);
    }
}

}  // namespace

bool AffixExpander::load(const QString& affixFile, const QString& wordsFile) {
    m_suffixes.clear();
    m_aliases.clear();
    m_flagType = FlagType::Character;
    m_needAffix.clear();
    m_forbidden.clear();
    m_onlyInCompound.clear();
    m_words.clear();

    QFile words(wordsFile);
    if (!readAffixFile(affixFile) || !words.open(QIODevice::ReadOnly)) {
        m_suffixes.clear();
        return false;
    }
    m_words = QByteArrayLiteral("\n") + words.readAll();
    return true;
}

bool AffixExpander::isLoaded() const {
    return !m_words.isEmpty();
}

bool AffixExpander::readAffixFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.readAll();
    const QString encoding = encodingName(bytes);
    if (encoding != QLatin1String("UTF8") && encoding != QLatin1String("ISO88591")) {
        return false;
    }
    m_latin1 = encoding == QLatin1String("ISO88591");

    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    bool aliasesStarted = false;
    for (const QString& line : decode(bytes).split(QLatin1Char('\n'))) {
        const QStringList fields = line.split(spaces, Qt::SkipEmptyParts);
        if (!fields.isEmpty() && !fields.front().startsWith(QLatin1Char('#'))) {
            readAffixLine(fields, aliasesStarted);
        }
    }
    return true;
}

void AffixExpander::readAffixLine(const QStringList& fields, bool& aliasesStarted) {
    const QString& keyword = fields.front();
    const QString value = fields.size() > 1 ? fields.at(1) : QString();
    // The flags that mark entries and suffixes
    const std::array<std::pair<QLatin1String, QString*>, 3> markingFlags{
        {{QLatin1String("NEEDAFFIX"), &m_needAffix},
         {QLatin1String("FORBIDDENWORD"), &m_forbidden},
         {QLatin1String("ONLYINCOMPOUND"), &m_onlyInCompound}}};
    for (const auto& [name, flag] : markingFlags) {
        if (keyword == name) {
            *flag = value;
            return;
        }
    }
    if (keyword == QLatin1String("FLAG")) {
        readFlagType(value);
    } else if (keyword == QLatin1String("AF") && !value.isEmpty()) {
        // The first AF line gives how many aliases follow; each of the others is one
        if (aliasesStarted) {
            m_aliases.append(flagsOf(value));
        }
        aliasesStarted = true;
    } else if (keyword == QLatin1String("SFX")) {
        readSuffix(fields);
    }
}

void AffixExpander::readFlagType(const QString& value) {
    if (value == QLatin1String("long")) {
        m_flagType = FlagType::Long;
    } else if (value == QLatin1String("num")) {
        m_flagType = FlagType::Number;
    }
}

void AffixExpander::readSuffix(const QStringList& fields) {
    const bool header = fields.size() == SUFFIX_HEADER_FIELDS && isNumber(fields.at(ADD_FIELD));
    if (header || fields.size() < SUFFIX_HEADER_FIELDS) {
        return;
    }
    const QString& strip = fields.at(STRIP_FIELD);
    const QString& added = fields.at(ADD_FIELD);
    const qsizetype slash = added.indexOf(QLatin1Char('/'));
    const QString add = slash < 0 ? added : added.left(slash);
    Suffix suffix{
        .strip = strip == NOTHING ? QString() : strip,
        .add = add == NOTHING ? QString() : add,
        .condition = parseCondition(fields.size() >= SUFFIX_FIELDS ? fields.at(CONDITION_FIELD)
                                                                   : QStringLiteral(".")),
        .continuation = slash < 0 ? QStringList() : flagsOf(added.mid(slash + 1)),
    };
    m_suffixes[fields.at(FLAG_FIELD)].append(std::move(suffix));
}

QStringList AffixExpander::flagsOf(const QString& text) const {
    if (!m_aliases.isEmpty() && isNumber(text)) {
        return m_aliases.value(text.toInt() - 1);
    }
    QStringList flags;
    switch (m_flagType) {
    case FlagType::Number:
        flags = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
        break;
    case FlagType::Long:
        for (qsizetype at = 0; at + LONG_FLAG_SIZE <= text.size(); at += LONG_FLAG_SIZE) {
            flags.append(text.mid(at, LONG_FLAG_SIZE));
        }
        break;
    case FlagType::Character:
        for (const QChar character : text) {
            flags.append(QString(character));
        }
        break;
    }
    return flags;
}

QList<AffixExpander::Entry> AffixExpander::entries(const QString& word) const {
    QList<Entry> found;
    if (m_words.isEmpty() || word.isEmpty()) {
        return found;
    }
    const QByteArray key = QByteArrayLiteral("\n") + encode(word);
    for (qsizetype at = m_words.indexOf(key); at >= 0; at = m_words.indexOf(key, at + 1)) {
        const qsizetype after = at + key.size();
        if (wordEnd(m_words, after) != after) {
            continue;  // a longer word that starts with it
        }
        Entry entry{.word = word, .flags = flagsAt(after)};
        if (m_forbidden.isEmpty() || !entry.flags.contains(m_forbidden)) {
            found.append(std::move(entry));
        }
    }
    return found;
}

QStringList AffixExpander::flagsAt(qsizetype after) const {
    if (after >= m_words.size() || m_words.at(after) != '/') {
        return {};
    }
    const qsizetype end = wordEnd(m_words, after + 1);
    return flagsOf(decode(m_words.mid(after + 1, end - after - 1)));
}

bool AffixExpander::hasEntry(const QString& word) const {
    return !entries(word).isEmpty();
}

QStringList AffixExpander::formsOf(const QString& word) const {
    QStringList forms;
    for (const Entry& entry : entries(word)) {
        const bool alone = (m_needAffix.isEmpty() || !entry.flags.contains(m_needAffix)) &&
                           (m_onlyInCompound.isEmpty() || !entry.flags.contains(m_onlyInCompound));
        if (alone) {
            appendOnce(forms, word);
        }
        // The forms with the suffixes the suffixes allow (one level)
        QList<std::pair<QString, QStringList>> continued;
        addForms(word, entry.flags, forms, &continued);
        for (const auto& [form, flags] : std::as_const(continued)) {
            addForms(form, flags, forms, nullptr);
        }
    }
    return forms;
}

void AffixExpander::addForms(const QString& word, const QStringList& flags, QStringList& forms,
                             QList<std::pair<QString, QStringList>>* continued) const {
    for (const QString& flag : flags) {
        if (const auto suffixes = m_suffixes.constFind(flag); suffixes != m_suffixes.constEnd()) {
            addSuffixForms(word, suffixes.value(), forms, continued);
        }
    }
}

void AffixExpander::addSuffixForms(const QString& word, const QList<Suffix>& suffixes,
                                   QStringList& forms,
                                   QList<std::pair<QString, QStringList>>* continued) const {
    for (const Suffix& suffix : suffixes) {
        if (!makesForm(word, suffix)) {
            continue;
        }
        const QString form = word.chopped(suffix.strip.size()) + suffix.add;
        if (m_needAffix.isEmpty() || !suffix.continuation.contains(m_needAffix)) {
            appendOnce(forms, form);
        }
        if (continued != nullptr && !suffix.continuation.isEmpty()) {
            continued->append({form, suffix.continuation});
        }
    }
}

bool AffixExpander::makesForm(const QString& word, const Suffix& suffix) const {
    // A suffix only inside compounds makes no word of its own
    return word.endsWith(suffix.strip) && matches(word, suffix.condition) &&
           (m_onlyInCompound.isEmpty() || !suffix.continuation.contains(m_onlyInCompound));
}

QStringList AffixExpander::entriesOf(const QString& form) const {
    QStringList found;
    if (form.isEmpty()) {
        return found;
    }
    if (hasEntry(form)) {
        found.append(form);
    }
    // Each word looked up once: an entry with one of the flags has the form. The flags are
    // captured by a name of their own: clang's analyzer sees a captured structured binding
    // as uninitialized
    for (const auto& [root, flags] : rootsOf(form)) {
        const QList<Entry> rootEntries = entries(root);
        const bool hasFlag =
            std::ranges::any_of(rootEntries, [&wanted = flags](const Entry& entry) {
                return std::ranges::any_of(
                    wanted, [&entry](const QString& flag) { return entry.flags.contains(flag); });
            });
        if (hasFlag) {
            appendOnce(found, root);
        }
    }
    return found;
}

QList<std::pair<QString, QStringList>> AffixExpander::rootsOf(const QString& form) const {
    QList<Root> roots;
    for (auto flag = m_suffixes.constBegin(); flag != m_suffixes.constEnd(); ++flag) {
        for (const Suffix& suffix : flag.value()) {
            if (form.endsWith(suffix.add) && form.size() != suffix.add.size()) {
                const QString root = form.chopped(suffix.add.size()) + suffix.strip;
                if (matches(root, suffix.condition)) {
                    addRoot(roots, root, flag.key());
                }
            }
        }
    }
    return roots;
}

QStringList AffixExpander::relatedForms(const QString& form) const {
    QStringList forms;
    for (const QString& entry : entriesOf(form)) {
        for (const QString& related : formsOf(entry)) {
            appendOnce(forms, related);
        }
    }
    return forms;
}

bool AffixExpander::matches(const QString& word, const QList<ConditionCharacter>& condition) {
    if (condition.size() > word.size()) {
        return false;
    }
    const qsizetype start = word.size() - condition.size();
    for (qsizetype index = 0; index < condition.size(); ++index) {
        const ConditionCharacter& wanted = condition.at(index);
        if (wanted.any) {
            continue;
        }
        const bool inSet = wanted.characters.contains(word.at(start + index));
        if (inSet == wanted.negated) {
            return false;
        }
    }
    return true;
}

QList<AffixExpander::ConditionCharacter> AffixExpander::parseCondition(const QString& text) {
    QList<ConditionCharacter> condition;
    if (text == QLatin1String(".")) {
        return condition;  // no condition
    }
    qsizetype index = 0;
    while (index < text.size()) {
        const QChar character = text.at(index);
        ConditionCharacter wanted;
        if (character == QLatin1Char('.')) {
            wanted.any = true;
            ++index;
        } else if (character == QLatin1Char('[')) {
            qsizetype end = text.indexOf(QLatin1Char(']'), index);
            if (end < 0) {
                end = text.size();
            }
            QString set = text.mid(index + 1, end - index - 1);
            if (set.startsWith(QLatin1Char('^'))) {
                wanted.negated = true;
                set.remove(0, 1);
            }
            wanted.characters = set;
            index = end + 1;
        } else {
            wanted.characters = QString(character);
            ++index;
        }
        condition.append(wanted);
    }
    return condition;
}

QString AffixExpander::decode(const QByteArray& bytes) const {
    return m_latin1 ? QString::fromLatin1(bytes) : QString::fromUtf8(bytes);
}

QByteArray AffixExpander::encode(const QString& text) const {
    return m_latin1 ? text.toLatin1() : text.toUtf8();
}

}  // namespace kalahari::core
