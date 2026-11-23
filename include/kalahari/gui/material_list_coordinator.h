// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list_coordinator.h
/// @brief The lists of the kinds of materials on their tabs in the middle of the window

#ifndef KALAHARI_GUI_MATERIAL_LIST_COORDINATOR_H
#define KALAHARI_GUI_MATERIAL_LIST_COORDINATOR_H

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class QStatusBar;
class QTabWidget;
class QTimer;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

class MaterialListPanel;
class NavigatorCoordinator;

/// @brief Opens the lists of the materials (MaterialListPanel) on tabs in the middle of the
/// window and does what their commands ask
///
/// A kind of the Materials clicked in the Navigator, or Materials itself (All Materials), shows
/// its list on a tab of its own, or brings the tab to the front; or, as the settings say
/// (MaterialList::TABS_SETTING), the one tab of the lists shows it. The lists follow the book:
/// a material added, changed, renamed, deleted or linked, the theme, and the Navigator built
/// again. A list behind another tab is filled again when it comes to the front.
///
/// The commands of a list are those of the Navigator (NavigatorCoordinator): a material's card,
/// the editor of a working note, Open, Rename, Delete, Add; Link... opens the Link window, and
/// a material chosen shows in the Properties panel. Relation... of a card opens the window
/// Relation, and a list of cards is filled again when a card or the book's dictionary changes.
class MaterialListCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Brings the Properties panel to the front; @p keys: with the keys in its first
    /// field (Alt+Enter)
    using PropertiesShower = std::function<void(bool keys)>;

    /// @brief Constructor
    /// @param book The book of its window
    /// @param centralTabs The tabs in the middle of the window
    /// @param navigator The commands of the elements of the book
    /// @param statusBar Where the lists say what they did
    /// @param showProperties Brings the Properties panel to the front
    /// @param parent Parent QObject
    MaterialListCoordinator(core::ProjectManager& book, QTabWidget* centralTabs,
                            NavigatorCoordinator* navigator, QStatusBar* statusBar,
                            PropertiesShower showProperties, QObject* parent = nullptr);
    ~MaterialListCoordinator() override;

    MaterialListCoordinator(const MaterialListCoordinator&) = delete;
    MaterialListCoordinator& operator=(const MaterialListCoordinator&) = delete;

    /// @brief Show list @p list (MaterialList) on its tab, as MaterialList::TABS_SETTING says
    /// @param keys The list gets the keys
    /// @return The list shown; none without an open book
    MaterialListPanel* openList(const QString& list, bool keys);

    /// @brief The lists open on the tabs, in the order of the tabs
    [[nodiscard]] QList<MaterialListPanel*> lists() const;

    /// @brief The list on the tab in front; none when another tab is in front
    [[nodiscard]] MaterialListPanel* currentList() const;

    /// @brief Show material @p elementId in the list of its kind, the cover
    void showInCover(const QString& elementId);

public slots:
    /// @brief Fill the lists again from the open book: the one in front at once, the others
    /// when they come to the front
    void refresh();

    /// @brief Hide kind @p list (MaterialList) in the Navigator of every book; its materials
    /// stay in the book
    void hideKind(const QString& list);

    /// @brief Show kind @p list in the Navigator again
    void showKind(const QString& list);

signals:
    /// @brief A list asked for the settings of the materials (Settings > Navigator)
    void settingsRequested();

    /// @brief A list asked to restore clipping @p elementId to the place it was cut from
    void clippingRestoreRequested(const QString& elementId);

    /// @brief A list asked to open the text clipping @p elementId was cut from at its place
    void clippingOriginRequested(const QString& elementId);

    /// @brief A list of cards asked for the settings of the cards (Settings > Cards)
    void cardSettingsRequested();

    /// @brief The set for a publisher asked for the window of the export with the set
    /// (Export Set...)
    void exportSetRequested();

private slots:
    /// @brief What the Materials of the Navigator show changed (NavigatorPanel::
    /// MATERIALS_TREE_SETTING): the Navigator is built again
    void onTreeSettingChanged();

    /// @brief The kinds hidden (MaterialList::HIDDEN_SETTING) changed: the Navigator and the
    /// lists are built again
    void onHiddenSettingChanged();

private:
    void refreshNow();
    /// @brief The set for a publisher in the Properties panel does what it asks
    void connectSetPage();
    void connectList(MaterialListPanel* panel);
    void updateTab(MaterialListPanel* panel);
    /// @brief The list in front is filled again if it missed a change
    void onCurrentTabChanged();
    void edit(const QString& elementId, bool text);
    void open(const QString& elementId);
    void rename(const QString& elementId);
    void link(const QString& elementId);
    /// @param coverPart A material of the cover, a text for publishers: the part chosen at the
    /// start
    void add(MaterialListPanel* panel, const QString& list, const QString& coverPart = {});
    /// @brief Give material @p elementId of the cover the state @p state
    void setCoverState(const QString& elementId, const QString& state);
    /// @brief Give text @p elementId for publishers the state @p state
    void setPublishingState(const QString& elementId, const QString& state);
    /// @brief Make description or tagline @p elementId of the cover the version of its part
    /// in the set for publishers
    void pickForSet(const QString& elementId);
    /// @brief Show the set for a publisher with part @p part chosen in the Properties panel,
    /// brought to the front as for a material chosen
    void showSetProperties(const QString& part);
    void showInFolder(const QString& elementId);
    [[nodiscard]] QWidget* window() const;

    QTabWidget* m_centralTabs;
    NavigatorCoordinator* m_navigator;
    QStatusBar* m_statusBar;
    PropertiesShower m_showProperties;
    QTimer* m_refreshTimer = nullptr;
    int m_settingsListener = 0;  ///< core::SettingsManager::subscribe() id
    core::ProjectManager& m_book;  ///< The book of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_MATERIAL_LIST_COORDINATOR_H
