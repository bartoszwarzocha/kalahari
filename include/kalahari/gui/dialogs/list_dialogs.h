/// @file list_dialogs.h
/// @brief The windows of the lists: Format > Bullets and Numbering... and Set Numbering
///        Value... of the context menu of a list item

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/editor/text_lists.h"

#include <QSpinBox>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <optional>

class QButtonGroup;
class QComboBox;
class QLabel;
class QListWidget;
class QPaintEvent;
class QTabWidget;

namespace kalahari {
namespace gui {

class LengthSpinBox;

namespace dialogs {

/// @brief A field for the number of a list item, written as the level writes its numbers:
///        "3", "c", "iii" (digits are read in every format)
class ListNumberSpinBox : public QSpinBox {
    Q_OBJECT

public:
    explicit ListNumberSpinBox(QWidget* parent = nullptr);

    /// @brief Write the numbers in a format (digits for the bullets)
    void setNumberFormat(editor::ListNumberFormat format);

protected:
    QString textFromValue(int value) const override;
    int valueFromText(const QString& text) const override;
    QValidator::State validate(QString& input, int& position) const override;

private:
    editor::ListNumberFormat m_format = editor::ListNumberFormat::Decimal;
};

/// @brief A few items of a list as its levels show them: two items of a level with two of
///        the next level between them
class ListPreview : public QWidget {
    Q_OBJECT

public:
    explicit ListPreview(QWidget* parent = nullptr);

    /// @brief Show the items of @p level (from 0) and of the level above it, or of the next
    ///        one for the first level
    void showList(const editor::ListDefinition& definition, int level);

    /// @brief Show one item with its number
    void showItem(const editor::ListLevel& level, int number);

    /// @brief The marks shown, from the top: "1.", "2.", "a)"
    [[nodiscard]] QStringList shownMarks() const;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Row {
        QString mark;
        QString text;
        editor::ListLevel level;
    };
    QList<Row> m_rows;
};

/// @brief Format > Bullets and Numbering...: the formats of the nine levels of a list
///
/// The tabs Numbering and Bullets show the level chosen in the list at their left: its
/// numbers or its bullet among the tiles, where its number or bullet stands and where its
/// text starts, and a preview. A tile gives the level its format, so a level of a numbered
/// list may have bullets; Character... gives it any character of the Special Character window.
/// Restore Defaults gives every level the format of a new list of the tab's kind.
class BulletsNumberingDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Chooses a character for a bullet (the Special Character window); nothing when
    ///        the writer chose none
    using CharacterChooser = std::function<std::optional<QString>()>;

    /// @brief The window of a list
    /// @param definition The formats of its levels
    /// @param level The level shown first (0 to 8), on the tab of its kind
    BulletsNumberingDialog(const editor::ListDefinition& definition, int level,
                           QWidget* parent = nullptr);

    /// @brief The formats as the writer left them
    [[nodiscard]] editor::ListDefinition definition() const { return m_definition; }

    /// @brief The level shown (0 to 8)
    [[nodiscard]] int level() const { return m_level; }

    /// @brief Show a level (0 to 8)
    void setLevel(int level);

    /// @brief Whether the tab Numbering is shown (else Bullets)
    [[nodiscard]] bool isNumberingShown() const;

    /// @brief Show the tab Numbering or Bullets
    void showNumbering(bool numbering);

    /// @brief Replace the Special Character window that Character... opens
    void setCharacterChooser(CharacterChooser chooser);

private:
    /// @brief The widgets of a tab
    struct Page {
        QListWidget* levels = nullptr;
        QButtonGroup* tiles = nullptr;
        LengthSpinBox* markerIndent = nullptr;
        LengthSpinBox* textIndent = nullptr;
        ListPreview* preview = nullptr;
        ListNumberSpinBox* start = nullptr;  ///< Numbering only
        QComboBox* alignment = nullptr;      ///< Numbering only
        QLabel* character = nullptr;         ///< Bullets only: a bullet of no tile
    };

    /// @brief Build the tab of numbers or of bullets
    QWidget* createPage(bool numbering, Page& page);

    /// @brief Show the formats in both tabs
    void refresh();

    /// @brief Change the format of the level shown and show it
    void changeLevel(const std::function<void(editor::ListLevel&)>& change);

    /// @brief Character...: a bullet of the Special Character window
    void chooseCharacter();

    editor::ListDefinition m_definition;
    int m_level = 0;
    QTabWidget* m_tabs = nullptr;
    Page m_numbering;
    Page m_bullets;
    CharacterChooser m_chooseCharacter;
    bool m_refreshing = false;  ///< The fields are being filled, not changed by the writer
};

/// @brief Set Numbering Value...: the number of a list item; the items after it go on from it
class NumberingValueDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param level The format of the item's level
    /// @param number The number it has now
    NumberingValueDialog(const editor::ListLevel& level, int number, QWidget* parent = nullptr);

    /// @brief The number the writer gave
    [[nodiscard]] int value() const;

    /// @brief Give the number, as typed in the field
    void setValue(int value);

private:
    editor::ListLevel m_level;
    ListNumberSpinBox* m_value = nullptr;
    ListPreview* m_preview = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
