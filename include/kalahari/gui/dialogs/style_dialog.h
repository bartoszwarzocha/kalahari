/// @file style_dialog.h
/// @brief New Style... and Modify Style...: the window of a paragraph style of the book, on
///        tabs or short with a wide preview

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/style_draft.h"

#include <QFrame>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <array>
#include <optional>
#include <utility>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPaintEvent;
class QPainter;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QToolButton;

namespace kalahari::gui {
class LengthSpinBox;
class WrappingLabel;
}  // namespace kalahari::gui

namespace kalahari::gui::dialogs {

/// @brief The paragraph of a style between the paragraph before it, in Normal, and the one
///        after it, in its next style; the two around it in grey
///
/// The letters keep the proportions of the styles, a little smaller than in the text; the
/// indents and the spacing are smaller still, so that the text stays readable in a narrow
/// preview.
class StylePreview : public QFrame {
    Q_OBJECT

public:
    explicit StylePreview(QWidget* parent = nullptr);

    /// @brief Show the paragraphs, each in the look of a style (StyleDraft::look())
    void showStyle(const QVariantMap& before, const QVariantMap& style, const QVariantMap& after);

    /// @brief The look of the paragraph of the style
    [[nodiscard]] const QVariantMap& styleLook() const { return m_style; }

    /// @brief The font the paragraph of the style is drawn in
    [[nodiscard]] QFont styleFont() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief Pixels of a point of the letters, and of an indent or a space
    [[nodiscard]] qreal letterScale() const;
    [[nodiscard]] qreal spaceScale() const;

    /// @brief The font of a look in the preview
    [[nodiscard]] QFont fontOf(const QVariantMap& look) const;

    /// @brief Draw a paragraph in a look from @p top, within @p area
    /// @return The top of the paragraph after it
    qreal drawParagraph(QPainter& painter, const QString& text, const QVariantMap& look,
                        qreal top, const QRectF& area) const;

    QVariantMap m_before;
    QVariantMap m_style;
    QVariantMap m_after;
};

/// @brief New Style... and Modify Style...: the name of a paragraph style, its base and next
///        style, and the look it gives its paragraphs
///
/// The window shows the style in one of two ways, switched with a button under it; the
/// program remembers the choice (ui.styleWindow):
/// - on tabs (by default): General (the name, the base and next style, a sentence of what the
///   style is), Font, Indents and Spacing, Breaks and Spelling, with the preview on the right;
///   a bold label marks a setting of the style's own, the other fields show its base style's;
/// - short: the name, the base and next style, the font, its size, bold, italic, the
///   alignment and the line spacing, a wide preview with the sentence under it; Font... and
///   Paragraph... open Format > Font and Format > Paragraph for the style.
///
/// Both change one draft (StyleDraft), so a switch keeps what was changed. Ctrl+B, Ctrl+I,
/// Ctrl+L, Ctrl+E, Ctrl+R and Ctrl+J work as in the text. OK is greyed out, with the reason
/// under the name, while the style has no name or the name of another style of the book.
class StyleDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The two ways the window shows a style
    enum class Variant {
        Tabs,  ///< All the settings on four tabs, the preview on the right
        Short  ///< What changes most often; the rest in the windows Font and Paragraph
    };

    /// @brief The rows of the list of line spacings on the tab Indents and Spacing
    enum LineSpacingRow { Single, OneAndHalf, Double, Multiple };

    /// @brief The rows of the list of special indents
    enum SpecialRow { NoSpecial, FirstLine, Hanging };

    /// @brief The window of a style
    /// @param draft The style (StyleDraft::newStyle() or StyleDraft::existing())
    /// @param typeName The name of the book's type ("Novel"), for a style of the type
    /// @param offerApply Whether to offer giving the new style to the paragraph at the cursor
    /// @param parent Parent widget
    StyleDialog(const StyleDraft& draft, const QString& typeName, bool offerApply,
                QWidget* parent = nullptr);

    /// @brief The style as the window changed it
    [[nodiscard]] const StyleDraft& draft() const { return m_draft; }

    /// @brief Whether the new style goes to the paragraph at the cursor
    [[nodiscard]] bool applyToText() const;

    /// @brief The way the window shows the style now
    [[nodiscard]] Variant variant() const { return m_variant; }

    /// @brief Show the style the other way, and remember it for the next window
    void setVariant(Variant variant);

    /// @brief The way the last window showed a style (ui.styleWindow); on tabs at first
    [[nodiscard]] static Variant savedVariant();

    // =========================================================================
    // The parts of the window (for tests); without a variant, of the one shown
    // =========================================================================

    [[nodiscard]] QPushButton* variantButton() const { return m_variantButton; }
    [[nodiscard]] QLineEdit* nameField(std::optional<Variant> variant = std::nullopt) const;
    [[nodiscard]] QComboBox* baseBox(std::optional<Variant> variant = std::nullopt) const;
    [[nodiscard]] QComboBox* nextBox(std::optional<Variant> variant = std::nullopt) const;
    [[nodiscard]] QLabel* problemLabel(std::optional<Variant> variant = std::nullopt) const;
    [[nodiscard]] WrappingLabel* summaryLabel(std::optional<Variant> variant = std::nullopt) const;
    [[nodiscard]] QPushButton* resetButton(std::optional<Variant> variant = std::nullopt) const;
    [[nodiscard]] QCheckBox* spellCheckBox(std::optional<Variant> variant = std::nullopt) const;
    [[nodiscard]] StylePreview* preview(std::optional<Variant> variant = std::nullopt) const;
    [[nodiscard]] QCheckBox* applyBox() const { return m_applyBox; }

    // On tabs
    [[nodiscard]] QTabWidget* tabs() const { return m_tabs; }
    [[nodiscard]] QLineEdit* familyField() const { return m_familyField; }
    [[nodiscard]] QListWidget* familyList() const { return m_familyList; }
    [[nodiscard]] QLineEdit* typefaceField() const { return m_typefaceField; }
    [[nodiscard]] QListWidget* typefaceList() const { return m_typefaceList; }
    [[nodiscard]] QLineEdit* sizeField() const { return m_sizeField; }
    [[nodiscard]] QListWidget* sizeList() const { return m_sizeList; }
    [[nodiscard]] QCheckBox* underlineBox() const { return m_underline; }
    [[nodiscard]] QLabel* screenNote() const { return m_screenNote; }
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

    // Short
    [[nodiscard]] QComboBox* shortFontBox() const { return m_shortFont; }
    [[nodiscard]] QComboBox* shortSizeBox() const { return m_shortSize; }
    [[nodiscard]] QToolButton* boldButton() const { return m_bold; }
    [[nodiscard]] QToolButton* italicButton() const { return m_italic; }
    [[nodiscard]] QToolButton* alignmentButton(Qt::Alignment alignment) const;
    [[nodiscard]] QComboBox* shortLineSpacingBox() const { return m_shortLineSpacing; }
    [[nodiscard]] QPushButton* fontButton() const { return m_fontButton; }
    [[nodiscard]] QPushButton* paragraphButton() const { return m_paragraphButton; }

    /// @brief Whether the label or checkbox of a setting is bold: the style's own setting
    [[nodiscard]] bool isMarkedOwn(const QString& property) const;

public slots:
    /// @brief Font...: Format > Font for the style (the short window)
    void editFont();

    /// @brief Paragraph...: Format > Paragraph for the style (the short window)
    void editParagraph();

protected:
    /// @brief Up and Down in the field over a list move in the list
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The parts both ways of showing the style have
    struct Common {
        QLineEdit* name = nullptr;
        QLabel* problem = nullptr;
        QComboBox* base = nullptr;
        QComboBox* next = nullptr;
        WrappingLabel* summary = nullptr;
        QPushButton* reset = nullptr;
        QCheckBox* spellCheck = nullptr;
        StylePreview* preview = nullptr;
    };

    /// @brief The parts of a variant
    [[nodiscard]] const Common& partsOf(std::optional<Variant> variant) const;

    /// @brief Build the name, base and next style of a variant, in a grid
    void createNameFields(Common& parts, QWidget* page, QGridLayout* grid, bool sideBySide);

    /// @brief Build the sentence of what the style is, with Reset, of a variant
    QWidget* createSummary(Common& parts, QWidget* page);

    /// @brief Build the window on tabs and its tabs
    QWidget* createTabsPage();
    QWidget* createGeneralTab();
    QWidget* createFontTab();
    QWidget* createIndentsTab();
    QWidget* createBreaksTab();

    /// @brief Build the short window
    QWidget* createShortPage();

    /// @brief Connect the parts both variants have to the draft
    void connectCommon(Common& parts);

    /// @brief The keys of the text: Ctrl+B, Ctrl+I and the alignments
    void addShortcuts();

    /// @brief Change a setting of the style, and show it
    void setValue(const QString& name, const QVariant& value);
    void setValues(const QVariantMap& values);

    /// @brief A setting as the style has it
    [[nodiscard]] QVariant value(const QString& name) const;

    /// @brief The look of a style of the book in the preview; the draft for itself or none
    [[nodiscard]] QVariantMap lookOf(const QString& id) const;

    /// @brief Show the draft in every part of both variants
    void refresh();

    /// @brief Show the draft in the parts of one variant
    void refreshCommon(const Common& parts);
    void refreshTabs();
    void refreshShort();

    /// @brief Bold the label of settings while the style has one of them as its own
    void markOwn(QWidget* label, const QStringList& properties);

    /// @brief The writer typed in a field over a list: the first item that starts with the
    ///        text is chosen in the list
    void findInList(QListWidget* list, const QString& text);

    /// @brief Fill a list of styles: the base ones or the next ones
    void fillStyleBox(QComboBox* box, const QStringList& ids, const QString& emptyText);

    /// @brief Show the variant's page; the other takes no room
    void showVariant();

    StyleDraft m_draft;
    QString m_typeName;
    bool m_offerApply;
    Variant m_variant = Variant::Tabs;
    bool m_updating = false;
    std::array<Common, 2> m_parts;
    QList<std::pair<QWidget*, QStringList>> m_ownMarks;  ///< Labels and their settings
    QStackedWidget* m_stack = nullptr;
    QPushButton* m_variantButton = nullptr;
    QCheckBox* m_applyBox = nullptr;

    // On tabs
    QTabWidget* m_tabs = nullptr;
    QLineEdit* m_familyField = nullptr;
    QListWidget* m_familyList = nullptr;
    QLineEdit* m_typefaceField = nullptr;
    QListWidget* m_typefaceList = nullptr;
    QLineEdit* m_sizeField = nullptr;
    QListWidget* m_sizeList = nullptr;
    QCheckBox* m_underline = nullptr;
    QLabel* m_screenNote = nullptr;
    WrappingLabel* m_ownHint = nullptr;
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

    // Short
    QComboBox* m_shortFont = nullptr;
    QComboBox* m_shortSize = nullptr;
    QToolButton* m_bold = nullptr;
    QToolButton* m_italic = nullptr;
    QButtonGroup* m_alignments = nullptr;
    QComboBox* m_shortLineSpacing = nullptr;
    QPushButton* m_fontButton = nullptr;
    QPushButton* m_paragraphButton = nullptr;
};

}  // namespace kalahari::gui::dialogs
