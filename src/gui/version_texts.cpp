/// @file version_texts.cpp
/// @brief What the program says of a version of a text

#include "kalahari/gui/version_texts.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/gui/command_registry.h"

#include <QCoreApplication>
#include <QHash>
#include <QKeySequence>
#include <QLocale>

namespace kalahari {
namespace gui {

/// @brief The context of the translations of the texts of the versions
struct VersionTexts {
    Q_DECLARE_TR_FUNCTIONS(VersionTexts)
};

namespace {

/// @brief The found text and its replacement are cut after this many characters
constexpr qsizetype MAX_REPLACED_LENGTH = 30;

/// @brief @p text in one line, cut with an ellipsis when it is long
QString shortened(const QString& text) {
    QString line = text.simplified();
    if (line.size() > MAX_REPLACED_LENGTH) {
        line = line.left(MAX_REPLACED_LENGTH - 1) + QChar(0x2026);
    }
    return line;
}

}  // namespace

QString versionTime(const QDateTime& created, const QDateTime& now) {
    const QDateTime local = created.toLocalTime();
    const QDate today = (now.isValid() ? now : QDateTime::currentDateTime()).toLocalTime().date();
    const QLocale locale;
    //: The time of a version. In Polish: H:mm
    const QString time = locale.toString(local.time(), VersionTexts::tr("HH:mm"));
    if (local.date() == today) {
        //: When a version was kept: %1 is the time. In Polish: dziś, %1
        return VersionTexts::tr("today, %1").arg(time);
    }
    if (local.date() == today.addDays(-1)) {
        //: When a version was kept: %1 is the time. In Polish: wczoraj, %1
        return VersionTexts::tr("yesterday, %1").arg(time);
    }
    //: The day of a version. In Polish: dd.MM.yyyy
    const QString day = locale.toString(local.date(), VersionTexts::tr("dd.MM.yyyy"));
    //: When a version was kept: %1 is the day, %2 the time
    return VersionTexts::tr("%1, %2").arg(day, time);
}

QString versionOrigin(const core::ChapterVersion& version, const core::KindWords& kind) {
    switch (version.origin) {
    case core::VersionOrigin::Manual:
        //: A version the writer saved. In Polish: Zapisana przez Ciebie
        return VersionTexts::tr("Saved by you");
    case core::VersionOrigin::Save:
        //: A version the program kept when the text was saved. In Polish: Przy zapisie
        //: {kind:genitive}
        return core::fillWords(VersionTexts::tr("When the {kind} was saved"),
                               {{QStringLiteral("kind"), kind}});
    case core::VersionOrigin::ReplaceAll:
        //: A version the program kept before Replace All: %1 was replaced with %2. In Polish:
        //: Przed „Zamień wszystko”: „%1” → „%2”, %Ln zamiana / zamiany / zamian
        return VersionTexts::tr("Before Replace All: \"%1\" → \"%2\", %Ln replacement(s)", nullptr,
                                version.replaced)
            .arg(shortened(version.found), shortened(version.replacement));
    case core::VersionOrigin::Restore:
        //: A version the program kept before another one was brought back: %1 is when that
        //: one was kept (today, 14:52). In Polish: Przed przywróceniem wersji z %1
        return VersionTexts::tr("Before the version of %1 was restored")
            .arg(version.restored.isValid() ? versionTime(version.restored) : QString());
    }
    return QString();
}

QString versionTitle(const core::ChapterVersion& version, const core::KindWords& kind) {
    const QString name = version.name.trimmed();
    if (name.isEmpty()) {
        return versionOrigin(version, kind);
    }
    //: A version's description in quotes. In Polish: „%1”
    return VersionTexts::tr("\"%1\"").arg(name);
}

QString versionWords(const core::ChapterVersion& version) {
    return wordCountText(version.words.wordsUnder(core::wordCountRules()));
}

QString wordCountText(int words) {
    //: In Polish: %Ln słowo / słowa / słów
    return VersionTexts::tr("%Ln word(s)", nullptr, words);
}

QString saveVersionKeys() {
    const Command* command = CommandRegistry::getInstance().getCommand("file.saveVersion");
    if (command != nullptr && !command->shortcut.isEmpty()) {
        return command->shortcut.toQKeySequence().toString(QKeySequence::NativeText);
    }
    //: Save Version without keys. In Polish: Plik > Zapisz wersję
    return VersionTexts::tr("File > Save Version");
}

QString versionIconId(core::VersionOrigin origin) {
    switch (origin) {
    case core::VersionOrigin::Manual:
        return QStringLiteral("version.manual");
    case core::VersionOrigin::Save:
        return QStringLiteral("version.save");
    case core::VersionOrigin::ReplaceAll:
        return QStringLiteral("version.replaceAll");
    case core::VersionOrigin::Restore:
        return QStringLiteral("version.restore");
    }
    return QStringLiteral("version.manual");
}

} // namespace gui
} // namespace kalahari
