// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file settings_search.h
/// @brief The search of the Settings dialog: the words typed, the options of a page they
///        find and the marks under those options

#ifndef KALAHARI_GUI_SETTINGS_SETTINGS_SEARCH_H
#define KALAHARI_GUI_SETTINGS_SETTINGS_SEARCH_H

#include <QColor>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <vector>

namespace kalahari::gui {

/// @brief The words of a search of the settings
///
/// A text matches when it holds every word, anywhere, without regard to case, accents and
/// the access keys of the names ("Pokaż &obrazy" matches "pokaz obr").
class SettingsSearch {
public:
    SettingsSearch() = default;
    explicit SettingsSearch(const QString& text);

    /// @brief No word typed
    [[nodiscard]] bool isEmpty() const { return m_words.isEmpty(); }

    /// @brief Whether a text holds every word
    [[nodiscard]] bool matches(const QString& text) const;

    /// @brief A text as the search compares it: small letters without accents, without the
    ///        marks of the access keys and the tags of rich text, single spaces
    [[nodiscard]] static QString folded(const QString& text);

private:
    QStringList m_words;  ///< Folded
};

/// @brief The options of a page a search finds, in the order of the page
///
/// The texts of an option are those the writer reads: of labels and notes, check boxes, radio
/// buttons and buttons, the titles of groups, the items of lists to choose from and the
/// placeholder texts of fields. The values typed into fields and the rows of the lists of
/// words are not options. Only the widgets the page shows count, and those of its advanced
/// settings while they are closed (SettingsPage::advancedSettings()).
[[nodiscard]] std::vector<QWidget*> optionsFound(QWidget* page, const SettingsSearch& search);

/// @brief The texts a widget gives the search (none: not an option)
[[nodiscard]] QStringList optionTexts(const QWidget* widget);

/// @brief The marks under the options a search found on a page
///
/// A child of the page over its widgets, transparent to the mouse: a frame with a light fill
/// around each option, through which its text is read, in the color of the search's hits of
/// the editor (Settings > Editor > Colors).
class SearchMarks : public QWidget {
    Q_OBJECT

public:
    /// @param page The page whose options it marks
    explicit SearchMarks(QWidget* page);

    /// @brief Mark these options of the page (none: no marks)
    void setOptions(const std::vector<QWidget*>& options);

    /// @brief The options marked, of the page still
    [[nodiscard]] std::vector<QWidget*> options() const;

    /// @brief The color of the marks (the search's hits of the editor)
    [[nodiscard]] static QColor markColor();

    /// @brief Where an option's mark is, in the page
    [[nodiscard]] QRect markOf(const QWidget* option) const;

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    std::vector<QPointer<QWidget>> m_options;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_SETTINGS_SETTINGS_SEARCH_H
