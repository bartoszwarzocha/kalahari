/// @file paragraph_dialog.h
/// @brief Format > Paragraph...: the alignment, indents, spacing, line spacing and the breaks
///        of lines and pages of the selected paragraphs

#pragma once

#include "kalahari/editor/paragraph_format.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QList>
#include <QRectF>
#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPaintEvent;
class QTabWidget;

namespace kalahari {
namespace gui {

class LengthSpinBox;

namespace dialogs {

/// @brief A page in small: a paragraph in a formatting between grey ones before and after it
class ParagraphPreview : public QWidget {
    Q_OBJECT

public:
    explicit ParagraphPreview(QWidget* parent = nullptr);

    /// @brief Show a paragraph with a formatting and an alignment
    void showParagraph(const editor::ParagraphFormat& format, Qt::Alignment alignment);

    /// @brief The lines of the paragraph as drawn, in the widget (for tests)
    [[nodiscard]] QList<QRectF> paragraphLines() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief The page, without its frame
    [[nodiscard]] QRectF pageRect() const;

    editor::ParagraphFormat m_format;
    Qt::Alignment m_alignment = Qt::AlignLeft;
};

/// @brief Format > Paragraph...: the formatting of the paragraph at the cursor, given to the
///        paragraph or to the selected ones; their style stays as it is
///
/// The tab Indents and Spacing has the alignment, the left and right indents, the first
/// line or hanging indent, the space before and after and the line spacing; the tab Line
/// and Page Breaks the widow and orphan control, keep with next, keep lines together and
/// page break before. Only what the writer changes is given to the paragraphs, so selected
/// paragraphs in other formats keep the rest of theirs. The indents of a list item are those
/// of its level (Bullets and Numbering): their fields are greyed out for one.
class ParagraphDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The rows of the list of line spacings, in its order
    enum LineSpacingRow { Single, OneAndHalf, Double, AtLeast, Exactly, Multiple };

    /// @brief The rows of the list of special indents, in its order
    enum SpecialRow { NoSpecial, FirstLine, Hanging };

    /// @brief The window for a paragraph
    /// @param shown Its formatting (BookEditor::shownParagraphFormat())
    /// @param alignment Its alignment (BookEditor::currentAlignment())
    /// @param listItem Whether it is a list item: the left and special indents are its level's
    /// @param parent Parent widget
    ParagraphDialog(const editor::ParagraphFormat& shown, Qt::Alignment alignment,
                    bool listItem = false, QWidget* parent = nullptr);

    /// @brief The parts the writer changed, for BookEditor::applyParagraphFormat(): the
    ///        others have no value
    [[nodiscard]] editor::ParagraphFormat changes() const;

    /// @brief The alignment, if the writer changed it
    [[nodiscard]] std::optional<Qt::Alignment> alignmentChange() const;

    /// @brief The formatting as the window shows it now
    [[nodiscard]] editor::ParagraphFormat shownFormat() const;

    /// @brief The alignment as the window shows it now
    [[nodiscard]] Qt::Alignment shownAlignment() const;

    /// @brief The parts of the window (for tests)
    [[nodiscard]] QTabWidget* tabs() const { return m_tabs; }
    [[nodiscard]] QComboBox* alignmentBox() const { return m_alignmentBox; }
    [[nodiscard]] LengthSpinBox* leftField() const { return m_left; }
    [[nodiscard]] LengthSpinBox* rightField() const { return m_right; }
    [[nodiscard]] QComboBox* specialBox() const { return m_specialBox; }
    [[nodiscard]] LengthSpinBox* specialField() const { return m_special; }
    [[nodiscard]] QDoubleSpinBox* beforeField() const { return m_before; }
    [[nodiscard]] QDoubleSpinBox* afterField() const { return m_after; }
    [[nodiscard]] QComboBox* lineSpacingBox() const { return m_lineSpacingBox; }
    [[nodiscard]] QDoubleSpinBox* lineSpacingField() const { return m_lineSpacing; }
    [[nodiscard]] QCheckBox* widowControlBox() const { return m_widowControl; }
    [[nodiscard]] QCheckBox* keepWithNextBox() const { return m_keepWithNext; }
    [[nodiscard]] QCheckBox* keepTogetherBox() const { return m_keepTogether; }
    [[nodiscard]] QCheckBox* pageBreakBeforeBox() const { return m_pageBreakBefore; }
    [[nodiscard]] ParagraphPreview* preview() const { return m_preview; }
    [[nodiscard]] ParagraphPreview* breaksPreview() const { return m_breaksPreview; }

private:
    /// @brief Build the tab Indents and Spacing, and the tab Line and Page Breaks
    QWidget* createIndentsTab();
    QWidget* createBreaksTab();

    /// @brief The field of the line spacing for its row: greyed out for the fixed ones, in
    ///        points or lines
    void showLineSpacingRow(int row, bool keepValue);

    /// @brief The field of the special indent for its row
    void showSpecialRow(int row);

    /// @brief Show the formatting in the previews
    void updatePreviews();

    editor::ParagraphFormat m_shown;
    Qt::Alignment m_alignment;
    bool m_listItem;
    bool m_updating = false;
    QTabWidget* m_tabs = nullptr;
    QComboBox* m_alignmentBox = nullptr;
    LengthSpinBox* m_left = nullptr;
    LengthSpinBox* m_right = nullptr;
    QComboBox* m_specialBox = nullptr;
    LengthSpinBox* m_special = nullptr;
    QDoubleSpinBox* m_before = nullptr;
    QDoubleSpinBox* m_after = nullptr;
    QComboBox* m_lineSpacingBox = nullptr;
    QDoubleSpinBox* m_lineSpacing = nullptr;
    QCheckBox* m_widowControl = nullptr;
    QCheckBox* m_keepWithNext = nullptr;
    QCheckBox* m_keepTogether = nullptr;
    QCheckBox* m_pageBreakBefore = nullptr;
    ParagraphPreview* m_preview = nullptr;
    ParagraphPreview* m_breaksPreview = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
