// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file polish.h
/// @brief The program in Polish for one test: the translation it ships and the names of the
/// kinds

#ifndef KALAHARI_TESTS_POLISH_H
#define KALAHARI_TESTS_POLISH_H

#include "kalahari/gui/kind_names.h"

#include <QCoreApplication>
#include <QString>
#include <QTranslator>

namespace kalahari::test {

/// @brief The program in Polish while it lives: the translation it ships
/// (":/i18n/kalahari_pl.qm") and the names of the kinds (gui::programLanguage())
class Polish {
public:
    Polish() : m_before(gui::programLanguage()) {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
        gui::setProgramLanguage(QStringLiteral("pl"));
    }
    ~Polish() {
        QCoreApplication::removeTranslator(&m_translator);
        gui::setProgramLanguage(m_before);
    }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;
    Polish(Polish&&) = delete;
    Polish& operator=(Polish&&) = delete;

    /// @brief Whether the translation was there
    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    QString m_before;
    bool m_loaded = false;
};

}  // namespace kalahari::test

#endif  // KALAHARI_TESTS_POLISH_H
