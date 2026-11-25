// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file list_dialogs.h
/// @brief The windows of the lists: Format > Bullets and Numbering... and Set Numbering
///        Value... of the context menu of a list item

#ifndef KALAHARI_GUI_DIALOGS_LIST_DIALOGS_H
#define KALAHARI_GUI_DIALOGS_LIST_DIALOGS_H

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/editor/text_lists.h"
#include "kalahari/gui/list_sets.h"

#include <QSpinBox>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <optional>

class QAbstractButton;
class QButtonGroup;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPaintEvent;
class QRadioButton;
class QScrollArea;
class QTabWidget;
class QVBoxLayout;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

class LengthSpinBox;
class ListSetTile;

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
    [[nodiscard]] QString textFromValue(int value) const override;
    [[nodiscard]] int valueFromText(const QString& text) const override;
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

    /// @brief Show items of the first three levels: one under the other, and back
    void showLevels(const editor::ListDefinition& definition);

    /// @brief Show one item with its number
    void showItem(const editor::ListLevel& level, int number);

    /// @brief The marks shown, from the top: "1.", "2.", "a)"
    [[nodiscard]] QStringList shownMarks() const;

    [[nodiscard]] QSize sizeHint() const override;

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

/// @brief The name of a set of list formats and where it is kept: Save as Set... and Rename...
///        of the tab Sets of Format > Bullets and Numbering...
class ListSetNameDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param name The name shown first
    /// @param askPlace Whether the window asks where the set is kept (a new set in a text of
    ///        the book: in the book or in the program)
    ListSetNameDialog(const QString& name, bool askPlace, QWidget* parent = nullptr);

    /// @brief The name typed, without the spaces at its ends
    [[nodiscard]] QString name() const;

    /// @brief Where the set is kept: in the book or in the program
    [[nodiscard]] ListSetPlace place() const;

    /// @brief Choose where the set is kept (ListSetPlace::Book or Program)
    void setPlace(ListSetPlace place);

    /// @brief The field of the name
    [[nodiscard]] QLineEdit* nameField() const { return m_name; }

private:
    QLineEdit* m_name = nullptr;
    QRadioButton* m_inBook = nullptr;
    QRadioButton* m_inProgram = nullptr;
};

/// @brief Format > Bullets and Numbering...: the formats of the nine levels of a list
///
/// The tab Sets, the first, shows the sets of all nine levels at once (gui/list_sets.h): the
/// program's, the book's and the program's of the writer, by groups. A set chosen gives the
/// list its formats; its context menu renames and deletes the writer's sets and makes a set the
/// default of F12 or Shift+F12. Save as Set... keeps the formats of the window as a set of the
/// writer, in the book or in the program.
///
/// The tabs Numbering and Bullets show the level chosen in the list at their left: its
/// numbers or its bullet among the tiles, the text before and after the number and the numbers
/// of the levels above it the level shows (1.1.), where its number or bullet stands and where
/// its text starts, and a preview. A tile gives the level its format, so a level of a numbered
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

    /// @brief Whether the tab Numbering is shown (else Bullets or Sets)
    [[nodiscard]] bool isNumberingShown() const;

    /// @brief Whether the tab Sets is shown
    [[nodiscard]] bool isSetsShown() const;

    /// @brief Show the tab Numbering or Bullets
    void showNumbering(bool numbering);

    /// @brief Show the tab Sets
    void showSets();

    /// @brief Replace the Special Character window that Character... opens
    void setCharacterChooser(CharacterChooser chooser);

    /// @brief The book of the text, whose sets the tab Sets shows and Save as Set... can keep
    ///        the formats in; null: a text outside the book (the program's sets alone)
    void setBook(core::ProjectManager* book);

    /// @brief The keys of Numbering and Bullets, as the context menu of a set names them
    void setListKeys(const QString& numbering, const QString& bullets);

    /// @brief The sets the tab Sets shows, in its order
    [[nodiscard]] const QList<ListSet>& sets() const { return m_sets; }

    /// @brief Choose a set: the list gets its formats
    void chooseSet(const QString& id);

    /// @brief The set whose formats the window has; empty: none (changed by hand)
    [[nodiscard]] QString chosenSet() const;

    /// @brief Save as Set...: keep the formats of the window as a set of the writer
    /// @return Whether it was kept
    bool saveAsSet();

    /// @brief Rename...: a new name of a set of the writer
    /// @return Whether it was renamed
    bool renameSet(const QString& id);

    /// @brief Delete: take a set of the writer out (the lists keep their formats)
    /// @return Whether it was taken out
    bool deleteSet(const QString& id);

    /// @brief Set as Default: F12 or Shift+F12 give the set (of its kind)
    void makeDefault(const QString& id);

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
        QLineEdit* prefix = nullptr;         ///< Numbering only: the text before the number
        QLineEdit* suffix = nullptr;         ///< Numbering only: the text after the number
        QComboBox* shownLevels = nullptr;    ///< Numbering only: the numbers of the levels above
        QLabel* character = nullptr;         ///< Bullets only: a bullet of no tile
    };

    /// @brief Build the tab of numbers or of bullets
    QWidget* createPage(bool numbering, Page& page);

    /// @brief Build the fields of the tab Numbering under its tiles
    void createNumberFields(QWidget* tab, QGridLayout* fields, Page& page);

    /// @brief Build the tab Sets
    QWidget* createSetsPage();

    /// @brief Fill the tab Sets with the sets as they are now
    void fillSets();

    /// @brief Show which set the window's formats are
    void refreshSets();

    /// @brief The context menu of a set at @p globalPos
    void showSetMenu(const QString& id, const QPoint& globalPos);

    /// @brief Keep the writer's sets changed in their place
    /// @return Whether they were kept
    bool keepSets(ListSetPlace place, const QList<ListSet>& sets);

    /// @brief The writer's sets of a place, as they are kept now
    [[nodiscard]] QList<ListSet> setsOf(ListSetPlace place) const;

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
    core::ProjectManager* m_book = nullptr;  ///< setBook()
    QList<ListSet> m_sets;                   ///< The sets of the tab Sets
    QScrollArea* m_setsScroll = nullptr;     ///< Scrolls the groups up and down, never sideways
    QWidget* m_setsGrid = nullptr;           ///< The groups of the tiles of the sets
    QVBoxLayout* m_setsLayout = nullptr;
    QList<ListSetTile*> m_setTiles;          ///< In the order of m_sets
    ListPreview* m_setsPreview = nullptr;
    QString m_numberingKeys = QStringLiteral("F12");
    QString m_bulletsKeys = QStringLiteral("Shift+F12");
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
} // namespace kalahari::gui

#endif  // KALAHARI_GUI_DIALOGS_LIST_DIALOGS_H
