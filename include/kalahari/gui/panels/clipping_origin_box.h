// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file clipping_origin_box.h
/// @brief The group Origin of a clipping in the Properties panel: where it was cut from, when,
///        when it was inserted, with Go to Its Place and Restore to Its Place

#ifndef KALAHARI_GUI_PANELS_CLIPPING_ORIGIN_BOX_H
#define KALAHARI_GUI_PANELS_CLIPPING_ORIGIN_BOX_H

#include <QGroupBox>
#include <QString>

class QFormLayout;
class QLabel;
class QPushButton;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief Where a clipping comes from, in the Properties panel
///
/// From: the title of the text it was cut from (a link that goes to the place), "(deleted)"
/// after it for a text deleted since, or "added by hand"; Place: the paragraph and the last
/// words before it; Cut: when; Inserted: when it was inserted last, or "no" (shown when a
/// clipping inserted stays in the Workshop, or when it was inserted). Go to Its Place opens the
/// text at the place, Restore to Its Place inserts the clipping there; both are greyed out
/// without the text, Restore also in a book open read-only.
class ClippingOriginBox : public QGroupBox {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param parent Parent widget
    explicit ClippingOriginBox(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Show where element @p elementId comes from when it is a clipping; else hide
    void showClipping(const QString& elementId);

    /// @brief The clipping shown; empty: none
    [[nodiscard]] QString clippingId() const { return m_clippingId; }

    /// @brief Go to Its Place
    [[nodiscard]] QPushButton* goButton() const { return m_go; }

    /// @brief Restore to Its Place
    [[nodiscard]] QPushButton* restoreButton() const { return m_restore; }

    /// @brief The text it comes from
    [[nodiscard]] QLabel* fromLabel() const { return m_from; }

    /// @brief The place in that text
    [[nodiscard]] QLabel* placeLabel() const { return m_place; }

    /// @brief When it was cut
    [[nodiscard]] QLabel* cutLabel() const { return m_cut; }

    /// @brief When it was inserted last
    [[nodiscard]] QLabel* insertedLabel() const { return m_inserted; }

signals:
    /// @brief Restore clipping @p elementId to the place it was cut from
    void requestRestore(const QString& elementId);

    /// @brief Open the text clipping @p elementId was cut from at its place
    void requestShowOrigin(const QString& elementId);

private:
    /// @brief A label of a value, its text selectable with the mouse and the keys
    QLabel* addValue(const QString& label, const char* name);

    core::ProjectManager& m_book;    ///< The book of its window
    QString m_clippingId;            ///< The clipping shown
    QFormLayout* m_layout = nullptr; ///< The rows
    QLabel* m_from = nullptr;        ///< From:
    QLabel* m_place = nullptr;       ///< Place:
    QLabel* m_cut = nullptr;         ///< Cut:
    QLabel* m_inserted = nullptr;    ///< Inserted:
    QPushButton* m_go = nullptr;     ///< Go to Its Place
    QPushButton* m_restore = nullptr;  ///< Restore to Its Place
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_CLIPPING_ORIGIN_BOX_H
