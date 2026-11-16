// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file dialog_checks.cpp
/// @brief Checks of the windows of the tests: the keys of their tabs, their size on a small
///        screen, the font of another system, the windows in Polish

#include "dialog_checks.h"

#include <catch2/catch_test_macros.hpp>

#include <QAbstractButton>
#include <QApplication>
#include <QCoreApplication>
#include <QGroupBox>
#include <QLabel>
#include <QStackedWidget>
#include <QStringList>
#include <QTabWidget>

#include <map>

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
        if (!shown) {
            continue;
        }
        if (const auto* label = qobject_cast<const QLabel*>(widget);
            label != nullptr && label->buddy() != nullptr) {
            texts << label->text();
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            texts << button->text();
        } else if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
            texts << group->title();
        }
    }
    return texts;
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
        std::map<QChar, QStringList> texts;
        for (const QString& text : keyTexts(dialog, tabs, tab)) {
            const QChar key = accessKey(text);
            if (key.isNull()) {
                continue;
            }
            texts[key] << text;
            if (key.unicode() >= 0x80 || !key.isLetterOrNumber()) {
                problems << QStringLiteral("not a Latin key: ") + text;
            }
        }
        for (const auto& [key, each] : texts) {
            if (each.size() > 1) {
                problems << each.join(QStringLiteral(" / "));
            }
        }
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

Polish::Polish() {
    m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
    if (m_loaded) {
        QCoreApplication::installTranslator(&m_translator);
    }
}

Polish::~Polish() {
    QCoreApplication::removeTranslator(&m_translator);
}

}  // namespace kalahari::test
