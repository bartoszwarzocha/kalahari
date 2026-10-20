/// @file paragraph_panel.h
/// @brief The Paragraph and Styles panel: the paragraph at the cursor of the text in front,
/// the buttons of its formatting and the palette of the book's styles

#pragma once

#include <QWidget>

class QButtonGroup;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QTimer;
class QToolBar;
class QToolButton;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari {
namespace gui {

/// @brief The paragraph at the cursor of the text in front, and the styles of its book
///
/// A band names the paragraph at the cursor ("paragraph 3 of 12") or the selected ones ("4 of
/// 12"). Under it the buttons of bold, italic and underline, Clear Formatting and the
/// alignments are the commands of the Format menu, checked and greyed out as they are there.
/// The palette lists the styles of the book by their names or in their look, with the style of
/// the paragraphs selected; a click on a style gives it to the paragraphs and the keys back to
/// the text.
///
/// The panel is a panel of its own (DockCoordinator::paragraphDock()) or the lower part of the
/// Properties panel, which folds to its band (setFoldable()).
class ParagraphPanel : public QWidget {
    Q_OBJECT

public:
    explicit ParagraphPanel(QWidget* parent = nullptr);
    ~ParagraphPanel() override;

    /// @brief Follow the text in front; nullptr without one (the Dashboard, no tab)
    void setEditor(editor::BookEditor* editor);

    /// @brief The text the panel follows (nullptr without one)
    editor::BookEditor* editor() const;

    /// @brief Give the band the button that folds the panel to the band (in the Properties
    ///        panel) or not
    void setFoldable(bool foldable);

    /// @brief Whether only the band shows
    bool isFolded() const { return m_folded; }

    /// @brief Show only the band, or all of the panel
    void setFolded(bool folded);

    /// @brief Whether the palette shows the styles in their look, not by their names only
    bool showsPreview() const;

    /// @brief Show the styles in their look, or by their names (remembered for both places of
    ///        the panel: panels.stylePreview)
    void setShowsPreview(bool preview);

    /// @brief Show the paragraph at the cursor and the styles of its text now
    void refresh();

signals:
    /// @brief The writer folded the panel to its band, or unfolded it (setFolded())
    void foldedChanged(bool folded);

protected:
    /// @brief A panel shown again shows the paragraph it missed while hidden
    void showEvent(QShowEvent* event) override;

    /// @brief A new font of the panel gives the look of the styles new sizes
    void changeEvent(QEvent* event) override;

private:
    /// @brief The band: its title and the place of the paragraphs in the text
    QWidget* createBand();

    /// @brief The rows of the paragraph: its kind (an object), its formatting, its alignment
    QWidget* createParagraphRows();

    /// @brief The styles: Names and Preview, and the list
    QWidget* createPalette();

    /// @brief The label of a row of the paragraph, in the middle of the row's height
    QLabel* rowLabel(const QString& text);

    /// @brief A toolbar of the panel with the actions of these commands (a null id: a line)
    QToolBar* commandBar(const QList<const char*>& commandIds);

    /// @brief Refresh soon (the cursor may move on); now when hidden is not the case
    void scheduleRefresh();

    /// @brief Show the band and the rows of the paragraph at the cursor
    void showParagraph();

    /// @brief Show the styles of the text, the style of its paragraphs selected
    void showStyles();

    /// @brief Give the paragraphs of the cursor or the selection the style of @p item, and the
    ///        keys back to the text
    void applyStyle(QListWidgetItem* item);

    /// @brief Give the items of the list their look, or the list's font (by names)
    void showStyleLook();

    /// @brief The icons of the buttons in the size of the toolbars
    void applyIconSize();

    editor::BookEditor* m_editor = nullptr;  ///< The text followed (setEditor())
    QTimer* m_refreshTimer = nullptr;        ///< Refreshes after the cursor stops (100 ms)
    bool m_stale = false;   ///< A change came while the panel was hidden (showEvent())
    bool m_updating = false;  ///< The list is being filled: its signals are not the writer's
    bool m_folded = false;    ///< Only the band shows

    QToolButton* m_foldButton = nullptr;  ///< Folds the panel to the band (setFoldable())
    QLabel* m_titleLabel = nullptr;       ///< "Current Paragraph", "Selected Paragraphs"
    QLabel* m_placeLabel = nullptr;       ///< "paragraph 3 of 12", "4 of 12"
    QLabel* m_emptyLabel = nullptr;       ///< Without a text in front: why nothing shows
    QWidget* m_rows = nullptr;            ///< The rows of the paragraph
    QLabel* m_kindTitle = nullptr;        ///< "Kind:" of an object at the cursor
    QLabel* m_kindLabel = nullptr;        ///< "Horizontal Line", "Scene Break", "Page Break"
    QLabel* m_noteLabel = nullptr;        ///< Where the look of a scene break is set
    QToolBar* m_formatBar = nullptr;      ///< Bold, italic, underline | Clear Formatting
    QToolBar* m_alignBar = nullptr;       ///< The four alignments
    QWidget* m_palette = nullptr;         ///< The styles
    QButtonGroup* m_modeGroup = nullptr;  ///< Names (0), Preview (1)
    QListWidget* m_styleList = nullptr;   ///< The styles of the text (id in Qt::UserRole)
};

} // namespace gui
} // namespace kalahari
