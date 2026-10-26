/// @file font_dialog.h
/// @brief Format > Font...: the font, style, size, colors, effects and letter spacing of the
///        selected text

#pragma once

#include "kalahari/editor/character_format.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QFont>
#include <QFrame>
#include <QList>
#include <QString>

#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QPaintEvent;
class QPushButton;
class QTabWidget;

namespace kalahari {
namespace gui {

class ColorMenu;

namespace dialogs {

/// @brief A sample of text in a character format, as the text would show it
class FontPreview : public QFrame {
    Q_OBJECT

public:
    explicit FontPreview(QWidget* parent = nullptr);

    /// @brief Show the sample in a format; a part without a value as the editor has it
    void showFormat(const editor::CharacterFormat& format, const QString& sample);

    /// @brief The text shown
    [[nodiscard]] const QString& sample() const { return m_sample; }

    /// @brief The font the sample is drawn in
    [[nodiscard]] QFont sampleFont() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    editor::CharacterFormat m_format;
    QString m_sample;
};

/// @brief Format > Font... (Ctrl+D): the look of the selected text, the text typed next
///        without a selection; the paragraph style stays as it is
///
/// The tab Font has the font, its style and size, picked from their lists or typed, the
/// color of the letters and of the highlight in the submenus of the Format menu, and the
/// effects; the tab Advanced the letter spacing. A part not the same in all of the
/// selection is empty (a checkbox half checked): it stays as it is unless the writer changes
/// it. Both tabs show a preview.
class FontDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window for text with a formatting
    /// @param shown The formatting of the selected text (BookEditor::shownCharacterFormat())
    /// @param sample The text the preview shows (the start of the selection); empty: a
    ///        sentence
    /// @param parent Parent widget
    FontDialog(const editor::CharacterFormat& shown, const QString& sample = QString(),
               QWidget* parent = nullptr);

    /// @brief The parts the writer changed, for BookEditor::applyCharacterFormat(): the others
    ///        have no value
    [[nodiscard]] editor::CharacterFormat changes() const;

    /// @brief The formatting as the window shows it now
    [[nodiscard]] const editor::CharacterFormat& shownFormat() const { return m_format; }

    /// @brief The sizes offered in the list, in points
    static const QList<qreal>& offeredSizes();

    /// @brief The parts of the window (for tests)
    [[nodiscard]] QTabWidget* tabs() const { return m_tabs; }
    [[nodiscard]] QLineEdit* familyField() const { return m_familyField; }
    [[nodiscard]] QListWidget* familyList() const { return m_familyList; }
    [[nodiscard]] QLineEdit* styleField() const { return m_styleField; }
    [[nodiscard]] QListWidget* styleList() const { return m_styleList; }
    [[nodiscard]] QLineEdit* sizeField() const { return m_sizeField; }
    [[nodiscard]] QListWidget* sizeList() const { return m_sizeList; }
    [[nodiscard]] QPushButton* colorButton() const { return m_colorButton; }
    [[nodiscard]] QPushButton* highlightButton() const { return m_highlightButton; }
    [[nodiscard]] ColorMenu* colorMenu() const { return m_colorMenu; }
    [[nodiscard]] ColorMenu* highlightMenu() const { return m_highlightMenu; }
    [[nodiscard]] QCheckBox* underlineBox() const { return m_underline; }
    [[nodiscard]] QCheckBox* strikethroughBox() const { return m_strikethrough; }
    [[nodiscard]] QCheckBox* superscriptBox() const { return m_superscript; }
    [[nodiscard]] QCheckBox* subscriptBox() const { return m_subscript; }
    [[nodiscard]] QCheckBox* smallCapsBox() const { return m_smallCaps; }
    [[nodiscard]] QCheckBox* allCapsBox() const { return m_allCaps; }
    [[nodiscard]] QComboBox* spacingBox() const { return m_spacingBox; }
    [[nodiscard]] QDoubleSpinBox* spacingField() const { return m_spacingField; }
    [[nodiscard]] FontPreview* preview() const { return m_preview; }
    [[nodiscard]] FontPreview* advancedPreview() const { return m_advancedPreview; }

protected:
    /// @brief Up and Down in the field of a list move in the list
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The styles of the list of styles, in its order
    enum StyleRow { Regular, Italic, Bold, BoldItalic };

    /// @brief Build the tab Font, and the tab Advanced
    QWidget* createFontTab();
    QWidget* createAdvancedTab();

    /// @brief A checkbox of an effect: half checked when the selection has it in part
    QCheckBox* createEffect(const QString& text, std::optional<bool> checked);

    /// @brief The writer typed in a field over a list: the first item that starts with the
    ///        text is chosen in the list
    void findInList(QListWidget* list, const QString& text);

    /// @brief The effects changed: the position and the capitals from their checkboxes
    void readEffects();

    /// @brief The letter spacing from its list and field
    void readSpacing();

    /// @brief Show a color on its button: its swatch and name
    void showColor(QPushButton* button, const std::optional<QColor>& color, bool highlight);

    /// @brief Show the format in the previews
    void updatePreviews();

    editor::CharacterFormat m_shown;   ///< As the text has it
    editor::CharacterFormat m_format;  ///< As the window shows it
    QString m_sample;
    bool m_updating = false;
    QTabWidget* m_tabs = nullptr;
    QLineEdit* m_familyField = nullptr;
    QListWidget* m_familyList = nullptr;
    QLineEdit* m_styleField = nullptr;
    QListWidget* m_styleList = nullptr;
    QLineEdit* m_sizeField = nullptr;
    QListWidget* m_sizeList = nullptr;
    QPushButton* m_colorButton = nullptr;
    QPushButton* m_highlightButton = nullptr;
    ColorMenu* m_colorMenu = nullptr;
    ColorMenu* m_highlightMenu = nullptr;
    QCheckBox* m_underline = nullptr;
    QCheckBox* m_strikethrough = nullptr;
    QCheckBox* m_superscript = nullptr;
    QCheckBox* m_subscript = nullptr;
    QCheckBox* m_smallCaps = nullptr;
    QCheckBox* m_allCaps = nullptr;
    QComboBox* m_spacingBox = nullptr;
    QDoubleSpinBox* m_spacingField = nullptr;
    FontPreview* m_preview = nullptr;
    FontPreview* m_advancedPreview = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
