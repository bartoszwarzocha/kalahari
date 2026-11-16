// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file localized_text.h
/// @brief Text in several languages, as the packages give their names and descriptions
///
/// Its own header, so that the data of the packages (book_type_package.h) and the templates
/// of the texts for publishers (publishing_texts.h) can both have it. text() is in
/// book_type_package.cpp.

#ifndef KALAHARI_CORE_LOCALIZED_TEXT_H
#define KALAHARI_CORE_LOCALIZED_TEXT_H

#include <QMap>
#include <QString>

namespace kalahari::core {

/// @brief Text in several languages
struct LocalizedText {
    QMap<QString, QString> values;  ///< Language code ("en", "pl") -> text

    /// @brief Text in @p language: its exact code ("pl_PL"), then the language alone ("pl"),
    /// then English
    [[nodiscard]] QString text(const QString& language) const;

    /// @brief Whether it has no text in any language
    [[nodiscard]] bool isEmpty() const { return values.isEmpty(); }

    bool operator==(const LocalizedText&) const = default;
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_LOCALIZED_TEXT_H
