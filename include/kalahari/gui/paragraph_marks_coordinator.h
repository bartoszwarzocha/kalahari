// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file paragraph_marks_coordinator.h
/// @brief The paperclips beside the paragraphs of the chapters that materials are linked with,
/// and the signs of the cards linked with them

#pragma once

#include <kalahari/editor/editor_types.h>

#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class QTabWidget;
class QTimer;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class EditorPanel;

/// @brief Shows a paperclip beside each paragraph of an open chapter that a material is
/// linked with (core::ElementLink::paragraph), and beside it the sign of the kind of the cards
/// of characters, locations and items linked with it
///
/// The paragraph of a link is found in the text of the editor (core::LinkRoles::paragraphOf()),
/// also when paragraphs were added or taken out before it. A click on the paperclip, or its
/// item in the context menu of the paragraph, shows the material in the Properties panel; a
/// card chosen at its sign opens. The marks follow the links, the setting of whether links
/// name paragraphs (core::LinkRoles::PARAGRAPHS_SETTING), the setting of the signs of the cards
/// (CARD_MARKS_SETTING), a chapter that opens and the book.
class ParagraphMarksCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Shows a material in the Properties panel and brings the panel to the front
    using MaterialShower = std::function<void(const QString& materialId)>;
    /// @brief Opens the full card of a character, a location or an item
    using CardOpener = std::function<void(const QString& cardId)>;

    /// @brief The setting of whether a paragraph linked with cards has the signs of their
    ///        kinds beside it (true, the usual one)
    static constexpr const char* CARD_MARKS_SETTING = "cards.paragraphMarks";

    /// @brief Whether a paragraph linked with cards has their signs (CARD_MARKS_SETTING)
    [[nodiscard]] static bool cardMarksShown();

    /// @brief Constructor
    /// @param book The book of its window
    /// @param centralTabs The tabs of the documents
    /// @param showMaterial What a chosen material does
    /// @param openCard What a chosen card does
    /// @param parent Parent object
    ParagraphMarksCoordinator(core::ProjectManager& book, QTabWidget* centralTabs,
                              MaterialShower showMaterial, CardOpener openCard = {},
                              QObject* parent = nullptr);

    /// @brief Destructor
    ~ParagraphMarksCoordinator() override;

    ParagraphMarksCoordinator(const ParagraphMarksCoordinator&) = delete;
    ParagraphMarksCoordinator& operator=(const ParagraphMarksCoordinator&) = delete;

    /// @brief The marks of chapter @p elementId of @p book in @p editor: a paperclip for each
    /// paragraph that materials are linked with, then a sign for each kind of the cards linked
    /// with it (editor::ParagraphMark::icon, the id of the icon of the kind), naming them in
    /// the order of the Workshop, unless the cards have no signs (cardMarksShown()); none when
    /// links name no paragraphs (the setting)
    static QList<editor::ParagraphMark> marksOf(const core::ProjectManager& book,
                                                const QString& elementId,
                                                const editor::BookEditor& editor);

    /// @brief Give every open chapter its paperclips again
    void refresh();

private:
    /// @brief Give the chapter of @p panel its paperclips again
    void refreshPanel(EditorPanel* panel);

    /// @brief A material or a card was chosen at a mark beside a paragraph
    void open(const QString& id) const;

    QTabWidget* m_centralTabs;      ///< Not owned
    MaterialShower m_showMaterial;  ///< What a chosen material does
    CardOpener m_openCard;          ///< What a chosen card does
    QTimer* m_refreshTimer;         ///< refresh() once for many changes
    int m_settingsListener = 0;     ///< core::SettingsManager::subscribe() id
    core::ProjectManager& m_book;  ///< The book of its window
};

}  // namespace kalahari::gui
