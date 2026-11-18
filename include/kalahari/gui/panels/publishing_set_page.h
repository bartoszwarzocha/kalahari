// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_set_page.h
/// @brief The set for a publisher in the Properties panel: its contents, the part chosen and
///        its export

#ifndef KALAHARI_GUI_PANELS_PUBLISHING_SET_PAGE_H
#define KALAHARI_GUI_PANELS_PUBLISHING_SET_PAGE_H

#include <QKeyCombination>
#include <QString>
#include <QWidget>

class QGroupBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QToolButton;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

class LengthMeter;

/// @brief The page of the Properties panel beside the set for a publisher of the list of the
/// texts for publishers (PublishingSetView)
///
/// Its name with the parts in it and their length in all. Its contents: its parts in its
/// order, each checked when it is in the set; Space includes the part chosen or leaves it out,
/// Alt+Up and Alt+Down (and the arrows beside the list) move it, saved with the book at once
/// (core::ProjectManager::setPublishingSet()). The part chosen: its version, where it comes
/// from, its length against its target and Open (Enter). Copy All, Export Set... and the gear
/// of the settings of the texts for publishers.
class PublishingSetPage : public QWidget {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param parent Parent widget
    explicit PublishingSetPage(core::ProjectManager& book, QWidget* parent = nullptr);

    /// @brief Show the set of the book with part @p part chosen; the first part for an
    /// unknown one
    void showSet(const QString& part);

    /// @brief The set again, as the book has it now
    void refresh();

    /// @brief Give the keys to the contents of the set
    void focusContents();

    /// @brief The part chosen; empty: none
    [[nodiscard]] QString part() const { return m_part; }

    /// @brief The parts in the set and their length in all
    [[nodiscard]] QLabel* summaryLabel() const { return m_summary; }

    /// @brief The contents of the set: one checkable row per part, in its order
    [[nodiscard]] QListWidget* contentsList() const { return m_contents; }

    /// @brief Moves the part chosen up (Alt+Up)
    [[nodiscard]] QToolButton* upButton() const { return m_upButton; }

    /// @brief Moves the part chosen down (Alt+Down)
    [[nodiscard]] QToolButton* downButton() const { return m_downButton; }

    /// @brief The group of the part chosen, named after it
    [[nodiscard]] QGroupBox* partGroup() const { return m_partGroup; }

    /// @brief The title of the version of the part chosen
    [[nodiscard]] QLabel* versionLabel() const { return m_version; }

    /// @brief Where the text of the part chosen comes from
    [[nodiscard]] QLabel* sourceLabel() const { return m_source; }

    /// @brief The length of the part chosen against its target
    [[nodiscard]] LengthMeter* meter() const { return m_meter; }

    /// @brief Opens the version of the part chosen (Enter)
    [[nodiscard]] QPushButton* openButton() const { return m_openButton; }

    /// @brief Copy All
    [[nodiscard]] QPushButton* copyAllButton() const { return m_copyAllButton; }

    /// @brief Export Set...
    [[nodiscard]] QPushButton* exportButton() const { return m_exportButton; }

    /// @brief The gear: the settings of the texts for publishers
    [[nodiscard]] QToolButton* settingsButton() const { return m_settingsButton; }

signals:
    /// @brief The writer chose part @p part in the contents: the page of the set follows
    void partChosen(const QString& part);

    /// @brief Open element @p elementId, the version of the part chosen
    void requestOpen(const QString& elementId);

    /// @brief Export the set (Export Set...)
    void requestExportSet();

    /// @brief Open the settings of the texts for publishers (the gear)
    void requestSettings();

    /// @brief What the status bar says: "Copied the set for the publisher"
    void message(const QString& text);

protected:
    /// @brief Alt+Up and Alt+Down move a part of the contents, Enter opens it
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* createContents(QWidget* parent);
    QWidget* createPart(QWidget* parent);
    QWidget* createExport(QWidget* parent);
    /// @brief The rows of the contents after the set of the book, the part chosen current
    void fillContents();
    /// @brief The group of the part chosen after it
    void showPart();
    /// @brief The icons of its buttons after the theme
    void refreshIcons();
    /// @brief Up and Down after the part chosen and the book
    void updateMoveButtons();
    /// @brief The row of the contents @p row was chosen
    void onCurrentRowChanged(int row);
    /// @brief The check of row @p item changed: the part is in the set or left out of it
    void onItemChanged(const QListWidgetItem* item);
    /// @brief Move the part chosen by @p step places (-1: up, 1: down)
    void movePart(int step);
    /// @brief Open the version of the part chosen
    void openPart();
    /// @brief Include the part chosen in the set or leave it out (Space)
    void toggleCurrent();
    /// @brief Whether @p keys are the contents': Alt+Up, Alt+Down, Space, Enter
    /// @param run Do what they do
    bool runContentsKey(QKeyCombination keys, bool run);
    void copyAll();

    core::ProjectManager& m_book;
    QString m_part;
    QLabel* m_icon = nullptr;
    QLabel* m_summary = nullptr;
    QListWidget* m_contents = nullptr;
    QToolButton* m_upButton = nullptr;
    QToolButton* m_downButton = nullptr;
    QGroupBox* m_partGroup = nullptr;
    QWidget* m_details = nullptr;  ///< The version, the source, the length and Open
    QLabel* m_version = nullptr;
    QLabel* m_source = nullptr;
    LengthMeter* m_meter = nullptr;
    QLabel* m_noVersion = nullptr;
    QPushButton* m_openButton = nullptr;
    QPushButton* m_copyAllButton = nullptr;
    QPushButton* m_exportButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PUBLISHING_SET_PAGE_H
