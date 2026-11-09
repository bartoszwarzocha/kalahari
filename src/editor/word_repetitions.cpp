/// @file word_repetitions.cpp
/// @brief The repetitions of words

#include <kalahari/editor/word_repetitions.h>

#include <QHash>

#include <algorithm>

namespace kalahari::editor {

namespace {

/// @brief How many letters a stem must share with the start of its word to be its base form
constexpr int SHARED_START = 3;

/// @brief About how many characters of the text a card of a repetition shows before its first
///        word and after its last one (whole words)
constexpr int CONTEXT_CHARS = 30;

/// @brief The text between two words of a repetition in one paragraph that a card shows whole;
///        a longer one is shortened to "…"
constexpr int MAX_GAP_CHARS = 80;

/// @brief The start of the word at or after @p place, going back to a space: the context does
///        not start in the middle of a word
int contextStart(const QString& text, int place) {
    if (place <= 0) {
        return 0;
    }
    const qsizetype space = text.lastIndexOf(QLatin1Char(' '), place);
    return space < 0 ? 0 : static_cast<int>(space) + 1;
}

/// @brief The end of the word at @p place, going on to a space
int contextEnd(const QString& text, int place) {
    if (place >= text.size()) {
        return static_cast<int>(text.size());
    }
    const qsizetype space = text.indexOf(QLatin1Char(' '), place);
    return space < 0 ? static_cast<int>(text.size()) : static_cast<int>(space);
}

/// @brief Whether the character at @p i of @p text joins two letters into one word: an
///        apostrophe or a hyphen with a letter on each side
bool joinsLetters(const QString& text, qsizetype i) {
    const QChar c = text.at(i);
    const bool joiner = c == QLatin1Char('\'') || c == QChar(0x2019) || c == QLatin1Char('-') ||
                        c == QChar(0x2011);  // non-breaking hyphen
    return joiner && i > 0 && i + 1 < text.size() && text.at(i - 1).isLetter() &&
           text.at(i + 1).isLetter();
}

/// @brief The words of a paragraph: where each starts and how long it is
QList<std::pair<int, int>> wordSpans(const QString& text) {
    QList<std::pair<int, int>> spans;
    qsizetype i = 0;
    while (i < text.size()) {
        if (!text.at(i).isLetter()) {
            ++i;
            continue;
        }
        const qsizetype start = i;
        while (i < text.size() && (text.at(i).isLetter() || joinsLetters(text, i))) {
            ++i;
        }
        // A word next to a digit is a code (B12), not a word of the text
        const bool code = (start > 0 && text.at(start - 1).isDigit()) ||
                          (i < text.size() && text.at(i).isDigit());
        if (!code) {
            spans.append({static_cast<int>(start), static_cast<int>(i - start)});
        }
    }
    return spans;
}

/// @brief How many letters @p a and @p b share at their start
int sharedStart(const QString& a, const QString& b) {
    const qsizetype length = std::min(a.size(), b.size());
    int shared = 0;
    while (shared < length && a.at(shared) == b.at(shared)) {
        ++shared;
    }
    return shared;
}

}  // namespace

QString chooseBaseForm(const QString& word, const QStringList& stems, const QString& language) {
    QString lower = word.toLower();
    QStringList candidates;
    for (const QString& stem : stems) {
        const QString base = stem.toLower();
        if (!base.isEmpty() && !candidates.contains(base)) {
            candidates.append(base);
        }
    }
    if (candidates.isEmpty()) {
        return lower;
    }

    // A Polish verb: its infinitive ("była": była, były, być)
    if (language.startsWith(QLatin1String("pl"))) {
        for (const QString& base : candidates) {
            if (base != lower && base.endsWith(QChar(0x0107))) {  // ć
                return base;
            }
        }
    }

    // The shortest stem that starts as the word ("walking": walking, walk)
    QString chosen;
    for (const QString& base : candidates) {
        if (base == lower || sharedStart(base, lower) < SHARED_START) {
            continue;
        }
        if (chosen.isEmpty() || base.size() < chosen.size()) {
            chosen = base;
        }
    }
    return chosen.isEmpty() ? lower : chosen;
}

RepetitionContext repetitionContext(const QStringList& paragraphs, const QList<TextWord>& words) {
    // The pieces of the paragraphs shown, each with the words in it
    struct Piece {
        int paragraph = 0;
        int from = 0;
        int to = 0;
        QList<std::pair<int, int>> words;  // in the paragraph
    };
    QList<Piece> pieces;
    const auto paragraphText = [&paragraphs](int paragraph) {
        return paragraph >= 0 && paragraph < paragraphs.size() ? paragraphs.at(paragraph)
                                                               : QString();
    };
    for (const TextWord& word : words) {
        const QString text = paragraphText(word.paragraph);
        const int end = word.start + word.length;
        if (!pieces.isEmpty() && pieces.last().paragraph == word.paragraph &&
            word.start - pieces.last().to <= MAX_GAP_CHARS) {
            pieces.last().to = end;
        } else {
            pieces.append({word.paragraph, contextStart(text, word.start - CONTEXT_CHARS), end, {}});
        }
        pieces.last().words.append({word.start, word.length});
    }

    RepetitionContext context;
    const QString ellipsis(QChar(0x2026));
    for (int i = 0; i < pieces.size(); ++i) {
        const Piece& piece = pieces.at(i);
        QString text = paragraphText(piece.paragraph);
        text.replace(QChar::LineSeparator, QLatin1Char(' '));
        // To the end of a word: two pieces of one paragraph are farther apart than the text
        // around them
        const int to = contextEnd(text, piece.to + CONTEXT_CHARS);
        if (i > 0) {
            context.text += QLatin1Char(' ');
        }
        if (piece.from > 0) {
            context.text += ellipsis;
        }
        const int offset = static_cast<int>(context.text.size()) - piece.from;
        context.text += text.mid(piece.from, to - piece.from);
        if (to < text.size()) {
            context.text += ellipsis;
        }
        for (const auto& [start, length] : piece.words) {
            context.marks.append({offset + start, length});
        }
    }
    return context;
}

const QSet<QString>& commonWords(const QString& language) {
    static const QSet<QString> polish = {
        QStringLiteral("aby"),     QStringLiteral("ale"),     QStringLiteral("albo"),
        QStringLiteral("ani"),     QStringLiteral("bez"),     QStringLiteral("bo"),
        QStringLiteral("był"),     QStringLiteral("była"),    QStringLiteral("było"),
        QStringLiteral("były"),    QStringLiteral("byli"),    QStringLiteral("być"),
        QStringLiteral("bym"),     QStringLiteral("byś"),     QStringLiteral("będzie"),
        QStringLiteral("chociaż"), QStringLiteral("choć"),    QStringLiteral("ciebie"),
        QStringLiteral("cię"),     QStringLiteral("coś"),     QStringLiteral("czy"),
        QStringLiteral("czyli"),   QStringLiteral("dla"),     QStringLiteral("gdy"),
        QStringLiteral("gdyby"),   QStringLiteral("gdzie"),   QStringLiteral("ich"),
        QStringLiteral("jak"),     QStringLiteral("jakby"),   QStringLiteral("jako"),
        QStringLiteral("jednak"),  QStringLiteral("jego"),    QStringLiteral("jej"),
        QStringLiteral("jemu"),    QStringLiteral("jest"),    QStringLiteral("jestem"),
        QStringLiteral("jesteś"),  QStringLiteral("jeszcze"), QStringLiteral("jeśli"),
        QStringLiteral("jeżeli"),  QStringLiteral("już"),     QStringLiteral("kiedy"),
        QStringLiteral("kto"),     QStringLiteral("która"),   QStringLiteral("które"),
        QStringLiteral("którego"), QStringLiteral("której"),  QStringLiteral("który"),
        QStringLiteral("których"), QStringLiteral("którym"),  QStringLiteral("lub"),
        QStringLiteral("mnie"),    QStringLiteral("mną"),     QStringLiteral("może"),
        QStringLiteral("nad"),     QStringLiteral("nam"),     QStringLiteral("nas"),
        QStringLiteral("nawet"),   QStringLiteral("nic"),     QStringLiteral("nie"),
        QStringLiteral("niech"),   QStringLiteral("niego"),   QStringLiteral("niej"),
        QStringLiteral("niemu"),   QStringLiteral("nim"),     QStringLiteral("nimi"),
        QStringLiteral("nią"),     QStringLiteral("ona"),     QStringLiteral("one"),
        QStringLiteral("oni"),     QStringLiteral("ono"),     QStringLiteral("oraz"),
        QStringLiteral("pod"),     QStringLiteral("potem"),   QStringLiteral("przed"),
        QStringLiteral("przez"),   QStringLiteral("przy"),    QStringLiteral("sam"),
        QStringLiteral("sama"),    QStringLiteral("się"),     QStringLiteral("siebie"),
        QStringLiteral("sobie"),   QStringLiteral("swój"),    QStringLiteral("swoja"),
        QStringLiteral("swoje"),   QStringLiteral("swojej"),  QStringLiteral("swoim"),
        QStringLiteral("swoją"),   QStringLiteral("są"),      QStringLiteral("tak"),
        QStringLiteral("także"),   QStringLiteral("tam"),     QStringLiteral("tego"),
        QStringLiteral("tej"),     QStringLiteral("ten"),     QStringLiteral("teraz"),
        QStringLiteral("też"),     QStringLiteral("tobie"),   QStringLiteral("tobą"),
        QStringLiteral("tutaj"),   QStringLiteral("tych"),    QStringLiteral("tylko"),
        QStringLiteral("tym"),     QStringLiteral("wam"),     QStringLiteral("was"),
        QStringLiteral("więc"),    QStringLiteral("wszystko"), QStringLiteral("wtedy"),
        QStringLiteral("żeby"),    QStringLiteral("mój"),     QStringLiteral("moja"),
        QStringLiteral("moje"),    QStringLiteral("mojej"),   QStringLiteral("twój"),
        QStringLiteral("twoja"),   QStringLiteral("twoje"),   QStringLiteral("bardzo"),
        QStringLiteral("jeden"),   QStringLiteral("jedna"),   QStringLiteral("jedno")};
    static const QSet<QString> english = {
        QStringLiteral("the"),   QStringLiteral("and"),   QStringLiteral("but"),
        QStringLiteral("for"),   QStringLiteral("nor"),   QStringLiteral("yet"),
        QStringLiteral("with"),  QStringLiteral("that"),  QStringLiteral("this"),
        QStringLiteral("these"), QStringLiteral("those"), QStringLiteral("from"),
        QStringLiteral("was"),   QStringLiteral("were"),  QStringLiteral("are"),
        QStringLiteral("been"),  QStringLiteral("being"), QStringLiteral("have"),
        QStringLiteral("has"),   QStringLiteral("had"),   QStringLiteral("not"),
        QStringLiteral("you"),   QStringLiteral("your"),  QStringLiteral("his"),
        QStringLiteral("her"),   QStringLiteral("hers"),  QStringLiteral("she"),
        QStringLiteral("him"),   QStringLiteral("they"),  QStringLiteral("them"),
        QStringLiteral("their"), QStringLiteral("what"),  QStringLiteral("which"),
        QStringLiteral("who"),   QStringLiteral("whom"),  QStringLiteral("when"),
        QStringLiteral("where"), QStringLiteral("there"), QStringLiteral("then"),
        QStringLiteral("than"),  QStringLiteral("into"),  QStringLiteral("onto"),
        QStringLiteral("out"),   QStringLiteral("about"), QStringLiteral("would"),
        QStringLiteral("could"), QStringLiteral("should"), QStringLiteral("will"),
        QStringLiteral("shall"), QStringLiteral("all"),   QStringLiteral("any"),
        QStringLiteral("some"),  QStringLiteral("its"),   QStringLiteral("our"),
        QStringLiteral("ours"),  QStringLiteral("very"),  QStringLiteral("just"),
        QStringLiteral("also"),  QStringLiteral("can"),   QStringLiteral("did"),
        QStringLiteral("does"),  QStringLiteral("doing"), QStringLiteral("only"),
        QStringLiteral("over"),  QStringLiteral("own"),   QStringLiteral("too"),
        QStringLiteral("off"),   QStringLiteral("each"),  QStringLiteral("both"),
        QStringLiteral("such"),  QStringLiteral("here"),  QStringLiteral("now"),
        QStringLiteral("myself"), QStringLiteral("himself"), QStringLiteral("herself"),
        QStringLiteral("itself"), QStringLiteral("themselves"), QStringLiteral("because")};
    static const QSet<QString> none;
    if (language.startsWith(QLatin1String("pl"))) {
        return polish;
    }
    if (language.startsWith(QLatin1String("en"))) {
        return english;
    }
    return none;
}

RepetitionAnalysis analyzeRepetitions(const QList<QStringList>& texts,
                                      const RepetitionOptions& options,
                                      const BaseFormOf& baseForm) {
    RepetitionAnalysis analysis;
    const QSet<QString>& common = commonWords(options.language);
    QHash<QString, QString> keys;  // A word as written -> its key

    for (int text = 0; text < texts.size(); ++text) {
        int index = 0;
        const QStringList& paragraphs = texts.at(text);
        for (int paragraph = 0; paragraph < paragraphs.size(); ++paragraph) {
            const QString& content = paragraphs.at(paragraph);
            for (const auto& [start, length] : wordSpans(content)) {
                const int place = index++;
                ++analysis.totalWords;
                if (length < options.minLength) {
                    continue;
                }
                const QString written = content.mid(start, length);
                const QString form = written.toLower();
                if ((options.skipCommon && common.contains(form)) ||
                    options.skipped.contains(form)) {
                    continue;
                }
                auto found = keys.constFind(written);
                if (found == keys.constEnd()) {
                    QString key = options.mergeForms && baseForm ? baseForm(written) : QString();
                    found = keys.insert(written, key.isEmpty() ? form : key);
                }
                const QString& key = found.value();
                if (options.skipped.contains(key) || (options.skipCommon && common.contains(key))) {
                    continue;
                }
                analysis.words.append({text, paragraph, start, length, place, form, key});
            }
        }
    }

    // The words of each key, in the order of the texts
    QHash<QString, QList<int>> wordsOfKey;
    QStringList keyOrder;
    for (int i = 0; i < analysis.words.size(); ++i) {
        const QString& key = analysis.words.at(i).key;
        auto it = wordsOfKey.find(key);
        if (it == wordsOfKey.end()) {
            keyOrder.append(key);
            it = wordsOfKey.insert(key, {});
        }
        it->append(i);
    }

    for (const QString& key : keyOrder) {
        const QList<int>& words = wordsOfKey.value(key);

        // How often
        WordCount count;
        count.key = key;
        count.count = static_cast<int>(words.size());
        count.words = words;
        QHash<QString, int> formCounts;
        QStringList formOrder;
        for (const int i : words) {
            const QString& form = analysis.words.at(i).form;
            if (!formCounts.contains(form)) {
                formOrder.append(form);
            }
            ++formCounts[form];
        }
        for (const QString& form : formOrder) {
            count.forms.append({form, formCounts.value(form)});
        }
        std::stable_sort(count.forms.begin(), count.forms.end(),
                         [](const auto& a, const auto& b) { return a.second > b.second; });
        analysis.counts.append(count);

        // Too close: each word within the distance of the one before, in the same text
        RepetitionCluster cluster;
        const auto close = [&analysis, &cluster]() {
            if (cluster.words.size() >= 2) {
                const TextWord& first = analysis.words.at(cluster.words.first());
                const TextWord& last = analysis.words.at(cluster.words.last());
                cluster.span = last.index - first.index + 1;
                analysis.clusters.append(cluster);
            }
            cluster.words.clear();
            cluster.forms.clear();
        };
        cluster.key = key;
        for (const int i : words) {
            const TextWord& word = analysis.words.at(i);
            if (!cluster.words.isEmpty()) {
                const TextWord& previous = analysis.words.at(cluster.words.last());
                if (previous.text != word.text || word.index - previous.index > options.distance) {
                    close();
                }
            }
            cluster.words.append(i);
            if (!cluster.forms.contains(word.form)) {
                cluster.forms.append(word.form);
            }
        }
        close();
    }

    std::sort(analysis.clusters.begin(), analysis.clusters.end(),
              [](const RepetitionCluster& a, const RepetitionCluster& b) {
                  return a.words.first() < b.words.first();
              });
    std::stable_sort(analysis.counts.begin(), analysis.counts.end(),
                     [](const WordCount& a, const WordCount& b) {
                         if (a.count != b.count) {
                             return a.count > b.count;
                         }
                         return a.key < b.key;
                     });
    return analysis;
}

}  // namespace kalahari::editor
