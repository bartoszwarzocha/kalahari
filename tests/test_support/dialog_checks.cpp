// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file dialog_checks.cpp
/// @brief Checks of the windows of the tests: the keys of their tabs and sections, their size
///        on a small screen, the font of another system, the windows in Polish

#include "dialog_checks.h"

#include "kalahari/gui/widgets/collapsible_section.h"

#include <catch2/catch_test_macros.hpp>

#include <QAbstractButton>
#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QStackedWidget>
#include <QStringList>
#include <QTabWidget>

#include <algorithm>
#include <map>
#include <vector>

namespace kalahari::test {

namespace {

/// The access key of a text: the letter after a single "&"
QChar accessKey(const QString& text) {
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            if (text.at(i + 1) == QLatin1Char('&')) {
                ++i;
                continue;
            }
            return text.at(i + 1).toLower();
        }
    }
    return QChar();
}

/// The text of @p widget that can have an access key: of a label of a field, a button or a
/// group; empty for another widget
QString keyTextOf(const QWidget* widget) {
    if (const auto* label = qobject_cast<const QLabel*>(widget);
        label != nullptr && label->buddy() != nullptr) {
        return label->text();
    }
    if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
        return button->text();
    }
    if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
        return group->title();
    }
    return {};
}

/// The texts with an access key while tab @p tab is in front: the titles of the tabs shown,
/// and the labels of fields, the buttons and the groups that show on that tab or outside the
/// tabs
QStringList keyTexts(const QWidget& dialog, const QTabWidget& tabs, int tab) {
    QStringList texts;
    for (int i = 0; i < tabs.count(); ++i) {
        if (tabs.isTabVisible(i)) {
            texts << tabs.tabText(i);
        }
    }
    for (const QWidget* widget : dialog.findChildren<QWidget*>()) {
        const QWidget* page = tabPageOf(widget);
        const bool shown = page == nullptr ? widget->isVisibleTo(&dialog)
                                           : page == tabs.widget(tab) && onItsTab(widget);
        if (shown) {
            texts << keyTextOf(widget);
        }
    }
    return texts;
}

/// Add to @p problems the texts of @p texts with the same access key, or with a key that is
/// not a Latin letter or digit
void addKeyProblems(const QStringList& texts, QStringList& problems) {
    std::map<QChar, QStringList> byKey;
    for (const QString& text : texts) {
        const QChar key = accessKey(text);
        if (key.isNull()) {
            continue;
        }
        byKey[key] << text;
        if (key.unicode() >= 0x80 || !key.isLetterOrNumber()) {
            problems << QStringLiteral("not a Latin key: ") + text;
        }
    }
    for (const auto& [key, each] : byKey) {
        if (each.size() > 1) {
            problems << each.join(QStringLiteral(" / "));
        }
    }
}

}  // namespace

const QWidget* tabPageOf(const QWidget* widget) {
    for (const QWidget* each = widget; each != nullptr; each = each->parentWidget()) {
        if (qobject_cast<const QStackedWidget*>(each->parentWidget()) != nullptr &&
            qobject_cast<const QTabWidget*>(each->parentWidget()->parentWidget()) != nullptr) {
            return each;
        }
    }
    return nullptr;
}

bool onItsTab(const QWidget* widget) {
    const QWidget* page = tabPageOf(widget);
    REQUIRE(page != nullptr);
    return widget == page || widget->isVisibleTo(page);
}

QString keyProblems(const QWidget& dialog, const QTabWidget& tabs) {
    QStringList problems;
    for (int tab = 0; tab < tabs.count(); ++tab) {
        if (!tabs.isTabVisible(tab)) {
            continue;
        }
        addKeyProblems(keyTexts(dialog, tabs, tab), problems);
    }
    problems.removeDuplicates();
    return problems.join(QStringLiteral("; "));
}

QString sectionKeyProblems(const QWidget& dialog) {
    const QList<gui::CollapsibleSection*> sections =
        dialog.findChildren<gui::CollapsibleSection*>();
    // The headers and what is outside the sections show whichever section is open
    QStringList shared;
    std::vector<QStringList> fields(static_cast<size_t>(sections.size()));
    for (const QWidget* widget : dialog.findChildren<QWidget*>()) {
        if (widget->window() != &dialog) {
            continue;  // Another window of the window, such as a list of its own
        }
        if (const auto* header = qobject_cast<const gui::SectionHeader*>(widget)) {
            if (header->isVisibleTo(&dialog)) {
                shared << header->text();
            }
            continue;
        }
        const QString text = keyTextOf(widget);
        if (text.isEmpty()) {
            continue;
        }
        const auto in = std::find_if(sections.cbegin(), sections.cend(),
                                     [widget](const gui::CollapsibleSection* section) {
                                         return section->body()->isAncestorOf(widget);
                                     });
        if (in == sections.cend()) {
            if (widget->isVisibleTo(&dialog)) {
                shared << text;
            }
        } else if (widget->isVisibleTo((*in)->body())) {
            fields.at(static_cast<size_t>(in - sections.cbegin())) << text;
        }
    }
    QStringList problems;
    addKeyProblems(shared, problems);
    for (const QStringList& each : fields) {
        addKeyProblems(shared + each, problems);
    }
    problems.removeDuplicates();
    return problems.join(QStringLiteral("; "));
}

void checkFitsSmallScreen(const QWidget& dialog) {
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 640;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    CHECK(dialog.sizeHint().width() <= 911);
    CHECK(dialog.sizeHint().height() <= MAX_HEIGHT);
}

AppFontOfPixels::AppFontOfPixels(int pixels) : m_font(QApplication::font()) {
    QFont font = m_font;
    font.setPixelSize(pixels);
    QApplication::setFont(font);
}

AppFontOfPixels::~AppFontOfPixels() {
    QApplication::setFont(m_font);
}

}  // namespace kalahari::test
