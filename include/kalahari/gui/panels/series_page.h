// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file series_page.h
/// @brief The series in the Properties panel: its name and its volumes

#ifndef KALAHARI_GUI_PANELS_SERIES_PAGE_H
#define KALAHARI_GUI_PANELS_SERIES_PAGE_H

#include <QKeyCombination>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The page of the Properties panel of the node of a series in the Navigator
///
/// The name of the series, saved when the field is left or Enter is pressed (empty: the title
/// of the first volume, core::ProjectManager::setSeriesName()), and the table of the volumes:
/// their numbers, titles, words and main texts (chapters, stories...) of their main sections.
/// New Volume..., Earlier and Later (Alt+Up, Alt+Down in the table, saved with the book at
/// once) and Open (Enter, a double click); the words of the whole series under the table.
class SeriesPage : public QWidget {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param parent Parent widget
    explicit SeriesPage(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief The series again, as the book has it now; the volume chosen stays chosen, the
    /// current volume is chosen at first
    void refresh();

    /// @brief Give the keys to the name of the series
    void focusName();

    /// @brief The id of the volume chosen in the table; empty: none
    [[nodiscard]] QString chosenVolume() const;

    /// @brief The name of the series
    [[nodiscard]] QLineEdit* nameEdit() const { return m_nameEdit; }

    /// @brief The table of the volumes: one row per volume, in their order
    [[nodiscard]] QTreeWidget* volumeList() const { return m_volumes; }

    /// @brief New Volume...
    [[nodiscard]] QPushButton* newButton() const { return m_newButton; }

    /// @brief Earlier: the volume chosen goes one place up (Alt+Up)
    [[nodiscard]] QPushButton* earlierButton() const { return m_earlierButton; }

    /// @brief Later: the volume chosen goes one place down (Alt+Down)
    [[nodiscard]] QPushButton* laterButton() const { return m_laterButton; }

    /// @brief Open: the first chapter of the volume chosen (Enter)
    [[nodiscard]] QPushButton* openButton() const { return m_openButton; }

    /// @brief The words of the whole series, under the table
    [[nodiscard]] QLabel* summaryLabel() const { return m_summary; }

signals:
    /// @brief New Volume...: the window of a new volume
    void requestNewVolume();

    /// @brief Open: the volume @p bookId becomes the current one, with its first chapter open
    void requestOpenVolume(const QString& bookId);

    /// @brief The name or the order of the volumes changed: the Navigator and the title of the
    /// window show them
    void seriesChanged();

protected:
    /// @brief Alt+Up, Alt+Down and Enter in the table of the volumes
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The group of the name of the series
    QWidget* createSeriesGroup(QWidget* parent);

    /// @brief The group of the volumes: the table, its buttons and the words of the series
    QWidget* createVolumesGroup(QWidget* parent);

    /// @brief Save the name typed when it differs from the name the series has
    void saveName();

    /// @brief Move the volume chosen @p step places (-1 up, 1 down) and save the book
    void moveChosen(int step);

    /// @brief Open the volume chosen
    void openChosen();

    /// @brief The buttons work on the volume chosen, in a book that can change
    void updateButtons();

    /// @brief Run the keys @p keys of the table when @p run; whether they are the table's
    bool runVolumeKey(QKeyCombination keys, bool run);

    core::ProjectManager& m_book;
    QLineEdit* m_nameEdit = nullptr;
    QTreeWidget* m_volumes = nullptr;
    QPushButton* m_newButton = nullptr;
    QPushButton* m_earlierButton = nullptr;
    QPushButton* m_laterButton = nullptr;
    QPushButton* m_openButton = nullptr;
    QLabel* m_summary = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_SERIES_PAGE_H
