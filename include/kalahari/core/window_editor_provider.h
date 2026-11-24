// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file window_editor_provider.h
/// @brief The editors of the window kinds that modules of the program bring: mind maps and
/// timelines

#ifndef KALAHARI_CORE_WINDOW_EDITOR_PROVIDER_H
#define KALAHARI_CORE_WINDOW_EDITOR_PROVIDER_H

#include <kalahari/core/extension_points.h>

#include <QByteArray>
#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class QWidget;

namespace kalahari::gui {
class CommandRegistry;
}  // namespace kalahari::gui

namespace kalahari::core {

class ProjectManager;

/// @brief What the window gives the editor of an element of a module
struct WindowEditorContext {
    ProjectManager& book;            ///< The book of the window
    gui::CommandRegistry& commands;  ///< The commands of the window
    QString elementId;               ///< The element that the editor edits
    /// Open the module's page of Settings, as the gear of the editor does
    std::function<void()> openSettings;
    /// Open element @p elementId of the book as the Navigator does: a chapter in its editor,
    /// a card, a material, another element of a module on its tab
    std::function<void(const QString& elementId)> openElement;
};

/// @brief A command of a module in the Book menu, and in the menu of the Navigator where
/// elements of the kinds of its editor are added: "New Relation Map..."; the module registers
/// its icon under its id
struct WindowEditorCommand {
    std::string id;     ///< The id of the command: "book.newRelationMap"
    QString menuText;   ///< In the language of the program, with its access key
    int menuOrder = 0;  ///< Its place in the Book menu
    /// Run it in the book of @p context (its elementId empty), its windows over @p parent
    std::function<void(const WindowEditorContext& context, QWidget* parent)> run;
};

/// @brief A module that edits the elements of the window kinds whose editor
/// (ElementKind::editor) is its editorId(): their new files, their tabs and its Settings
///
/// The program knows such a module only by this interface, so that the module can leave the
/// program as an add-on of its own: ProjectManager writes the file of a new element with
/// newFile(), the window opens an element on a tab that createEditor() makes, and Settings
/// shows the page of createSettingsPage(). A module registers its provider in the
/// ExtensionPointRegistry (registerPlugin()); windowEditorProvider() finds it.
class IWindowEditorProvider : public IPlugin {
public:
    ~IWindowEditorProvider() override = default;

    /// @brief The id of its editor, as booktype.json names it: "mindmap"
    [[nodiscard]] virtual QString editorId() const = 0;

    /// @brief The suffix of the files of its elements, without the dot: "kmap"
    [[nodiscard]] virtual QString fileSuffix() const = 0;

    /// @brief The content of the file of a new element, as the settings make it
    [[nodiscard]] virtual QByteArray newFile() const = 0;

    /// @brief The page of a tab of the window that edits element context.elementId
    ///
    /// The tab owns the page. The page saves the changes to the element's file itself, as
    /// they are made; the window gives it the property "elementId", finds the tab of an element
    /// by it and closes the tab when the element is deleted.
    [[nodiscard]] virtual QWidget* createEditor(const WindowEditorContext& context,
                                                QWidget* parent) = 0;

    /// @brief The title of its page of Settings, in the language of the program; empty: it
    /// has none
    [[nodiscard]] virtual QString settingsTitle() const { return QString(); }

    /// @brief Its page of Settings (a gui::SettingsPage), owned by the window of Settings
    [[nodiscard]] virtual QWidget* createSettingsPage() { return nullptr; }

    /// @brief Its commands in the Book menu and in the Navigator
    [[nodiscard]] virtual std::vector<WindowEditorCommand> commands() const { return {}; }

    /// @brief What the module offers after relation @p relationId of card @p cardId with card
    /// @p otherId was added: a bar the card shows under its relations, a child of @p parent
    /// that closes itself
    /// @return nullptr: nothing
    [[nodiscard]] virtual QWidget* relationAdded(const WindowEditorContext& context,
                                                 const QString& cardId, const QString& otherId,
                                                 const QString& relationId, QWidget* parent) {
        (void)context;
        (void)cardId;
        (void)otherId;
        (void)relationId;
        (void)parent;
        return nullptr;
    }

protected:
    IWindowEditorProvider() = default;
    IWindowEditorProvider(const IWindowEditorProvider&) = default;
    IWindowEditorProvider& operator=(const IWindowEditorProvider&) = default;
    IWindowEditorProvider(IWindowEditorProvider&&) = default;
    IWindowEditorProvider& operator=(IWindowEditorProvider&&) = default;
};

/// @brief The provider of editor @p editorId; nullptr when no module brings it
[[nodiscard]] std::shared_ptr<IWindowEditorProvider> windowEditorProvider(
    const QString& editorId);

/// @brief The providers of all the editors that the modules bring, in the order of their ids
[[nodiscard]] std::vector<std::shared_ptr<IWindowEditorProvider>> windowEditorProviders();

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_WINDOW_EDITOR_PROVIDER_H
