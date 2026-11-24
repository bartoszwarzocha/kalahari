// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file collapsible_section.h
/// @brief A section of fields under a header that opens and closes it, with a summary of the
///        fields in the header while it is closed

#ifndef KALAHARI_GUI_WIDGETS_COLLAPSIBLE_SECTION_H
#define KALAHARI_GUI_WIDGETS_COLLAPSIBLE_SECTION_H

#include <QAbstractButton>
#include <QSize>
#include <QString>
#include <QWidget>

class QEvent;
class QGridLayout;
class QKeyEvent;
class QPaintEvent;

namespace kalahari::gui {

/// @brief The header of a CollapsibleSection: an arrow, the title in bold with its access key,
///        and the summary of the fields at the right while the section is closed
///
/// A checkable button: checked, the section is open. A click, Space or Enter opens and
/// closes it; Alt and the access key of the title open it and go to its first field. The
/// colors are those of the palette of the theme.
class SectionHeader : public QAbstractButton {
    Q_OBJECT

public:
    /// @brief Create the header
    /// @param title The title, with its access key ("&Printer")
    /// @param parent Parent widget
    explicit SectionHeader(const QString& title, QWidget* parent = nullptr);

    /// @brief The summary shown at the right while the section is closed; also the
    ///        description of the header for screen readers
    void setSummary(const QString& summary);
    [[nodiscard]] QString summary() const { return m_summary; }

    /// @brief Where the title starts: the fields of the section start there too
    [[nodiscard]] int titleIndent() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    /// @brief Alt and the access key of the title were pressed
    void accessKeyPressed();

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief The room around the title, in pixels
    [[nodiscard]] int padding() const;

    /// @brief The width of the arrow, in pixels
    [[nodiscard]] int arrowSize() const;

    /// @brief The width of the title in bold, without its access key mark
    [[nodiscard]] int titleWidth() const;

    QString m_summary;
};

/// @brief Fields under a header that opens and closes them
///
/// The fields are on the grid of the body, whose second column stretches; they start where
/// the title of the header starts. A line in the color of the palette ends the section.
class CollapsibleSection : public QWidget {
    Q_OBJECT

public:
    /// @brief Create the section, open
    /// @param title The title, with its access key ("&Printer")
    /// @param parent Parent widget
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    /// @brief The header
    [[nodiscard]] SectionHeader* header() const { return m_header; }

    /// @brief The fields, shown while the section is open
    [[nodiscard]] QWidget* body() const { return m_body; }

    /// @brief The grid of the fields
    [[nodiscard]] QGridLayout* grid() const { return m_grid; }

    /// @brief Whether the fields show
    [[nodiscard]] bool isExpanded() const;

    /// @brief Show or hide the fields, as a click on the header does
    void setExpanded(bool expanded);

    /// @brief The summary of the fields in the header while the section is closed
    void setSummary(const QString& summary);
    [[nodiscard]] QString summary() const;

    /// @brief The first field of the section that Tab reaches (of a group of radio buttons
    ///        the one checked); nullptr: none
    [[nodiscard]] QWidget* firstField() const;

    /// @brief Open the section and go to its first field, as Alt and the access key of the
    ///        title do
    void openAndFocus();

signals:
    /// @brief The fields were shown or hidden
    void expandedChanged(bool expanded);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    SectionHeader* m_header = nullptr;
    QWidget* m_body = nullptr;
    QGridLayout* m_grid = nullptr;
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_COLLAPSIBLE_SECTION_H
