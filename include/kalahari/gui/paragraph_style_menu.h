/// @file paragraph_style_menu.h
/// @brief Format > Text Style: the paragraph styles of the text in front

#pragma once

#include <QList>
#include <QObject>

#include <array>
#include <functional>

class QAction;
class QActionGroup;
class QMenu;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

/// @brief A fixed command of Format > Text Style and the style it puts paragraphs in
struct StyleCommand {
    const char* commandId;  ///< In the registry: "format.style.heading1"
    const char* styleId;    ///< Of editor/paragraph_styles.h: "heading_1"
};

/// @brief The fixed commands of Format > Text Style, in the order of the menu: the styles
///        every book has (the package "Base")
const std::array<StyleCommand, 7>& styleCommands();

/// @brief Format > Text Style shows the paragraph styles of the text in front
///
/// The fixed commands have the styles every book has; after Code the menu adds the other
/// styles of the text (Caption, Footnote, those of the book's type), as their package names
/// them, each time it opens. The style of the paragraph at the cursor is checked, one at a
/// time, as on the Styles toolbar; none on a line or a break.
class ParagraphStyleMenu : public QObject {
public:
    /// @brief The text in front (nullptr: none)
    using EditorInFront = std::function<editor::BookEditor*()>;

    /// @param menu The Text Style submenu, with the fixed commands
    /// @param editorInFront The text whose styles the menu shows and changes
    /// @param parent Owner
    ParagraphStyleMenu(QMenu* menu, EditorInFront editorInFront, QObject* parent);

    /// @brief The items of the text's other styles, as the menu shows them now
    [[nodiscard]] const QList<QAction*>& bookStyleActions() const { return m_bookStyles; }

    /// @brief Put the items of the text's other styles in the menu again (as it opens)
    void rebuild();

private:
    QMenu* m_menu;
    EditorInFront m_editorInFront;
    QActionGroup* m_group;          ///< The fixed commands and the other styles: one checked
    QList<QAction*> m_bookStyles;   ///< The items of the text's other styles
};

}  // namespace kalahari::gui
