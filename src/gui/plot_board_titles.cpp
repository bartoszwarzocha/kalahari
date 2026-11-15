// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plot_board_titles.cpp
/// @brief PlotBoard: the numbers in the titles the program gave the texts ("Chapter 3"), after
/// the texts were rearranged

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/plot_board.h"

#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>
#include <QStringView>

#include <functional>
#include <optional>
#include <string>

namespace kalahari::gui {

namespace {

/// @brief Where the number is in a default title of a kind ("Chapter %n")
constexpr QStringView NUMBER_MARK = u"%n";

/// @brief A number in a title: arabic, or roman for a kind whose titles have roman numbers
/// (above 3999 they are arabic too)
constexpr const char* ARABIC_NUMBER = "\\d+";
constexpr const char* ROMAN_NUMBER = "[IVXLCDM]+|\\d+";

/// @brief What may not follow the number: a letter or a digit would make it another word
constexpr const char* NUMBER_END = "(?![\\p{L}\\p{N}])";

}  // namespace

QString PlotBoard::renumbering() {
    const auto value = core::SettingsManager::getInstance().get<std::string>(RENUMBER_SETTING,
                                                                             RENUMBER_ASK);
    if (value == RENUMBER_AUTO || value == RENUMBER_NEVER) {
        return QString::fromStdString(value);
    }
    return QString::fromLatin1(RENUMBER_ASK);
}

std::optional<QString> PlotBoard::numberedTitle(const QString& title,
                                                const core::ElementKind& kind,
                                                const QString& language, int number) {
    // The default title of the kind around its number: "Chapter " and ""
    const QString pattern = kind.title.isEmpty() ? kind.name.text(language)
                                                 : kind.title.text(language);
    const qsizetype mark = pattern.indexOf(NUMBER_MARK);
    if (mark < 0) {
        return std::nullopt;
    }
    const QString before = pattern.left(mark);
    const QString after = pattern.mid(mark + NUMBER_MARK.size());
    const QRegularExpression given(
        QStringLiteral("^%1(%2)%3%4")
            .arg(QRegularExpression::escape(before),
                 QLatin1String(kind.numbering == core::TitleNumbering::Roman ? ROMAN_NUMBER
                                                                             : ARABIC_NUMBER),
                 QRegularExpression::escape(after), QLatin1String(NUMBER_END)));
    const QRegularExpressionMatch match = given.match(title);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    // The number as the program writes it in a new title
    const QString full = kind.defaultTitle(language, number);
    const QString digits = full.mid(before.size(), full.size() - before.size() - after.size());
    QString renumbered = title;
    renumbered.replace(match.capturedStart(1), match.capturedLength(1), digits);
    return renumbered;
}

QList<PlotBoard::TitleChange> PlotBoard::renumbered(const core::ProjectManager& book) {
    QList<TitleChange> changes;
    const core::ProjectBook* first = book.book();
    if (first == nullptr) {
        return changes;
    }
    QHash<const core::ElementKind*, int> numbers;
    const std::function<void(const QList<core::ProjectElement>&)> collect =
        [&](const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                const core::ElementForm form = book.formOf(element);
                if (form == core::ElementForm::Group) {
                    collect(element.elements);
                    continue;
                }
                const core::KindRef kind = book.kindOf(element);
                if (form != core::ElementForm::Text || !kind) {
                    continue;
                }
                const int number = ++numbers[kind.kind];
                const std::optional<QString> title =
                    numberedTitle(element.title, *kind.kind, first->language, number);
                if (title && *title != element.title) {
                    changes << TitleChange{.id = element.id, .title = *title};
                }
            }
        };
    collect(first->mainElements);
    return changes;
}

}  // namespace kalahari::gui
