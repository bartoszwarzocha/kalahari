// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file print_book_dialog.h
/// @brief File > Print: the printer, the texts of the book, their layout and the sheets, with
///        a preview of the sheets, and the progress and the result of the print in one window

#pragma once

#include "kalahari/editor/book_print.h"
#include "kalahari/gui/dialogs/book_output_dialog.h"

#include <QHash>
#include <QList>
#include <QMarginsF>
#include <QPageSize>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QTimer;
class QToolButton;

namespace kalahari {
namespace editor {
class BookPages;
} // namespace editor

namespace gui {
namespace dialogs {

class SheetPreview;

/// @brief Print of the book, of some of its texts or of the text in front
///
/// The window is the program's own, not the system's. Its tabs are as high as the highest, so
/// the window keeps its size and nothing in it scrolls: the printer (the printers of the system,
/// the default one first; the copies, the pages, the odd or the even ones, both sides, the
/// color, the paper), the texts and the layout (those of the export: BookOutputDialog), and the
/// sheets (a page, two pages side by side or a booklet folded in half; a page fitted to the
/// sheet or of its own size, with crop marks). The lists offer only what the printer can do. At
/// the right the preview shows the sheets as they come out of the printer; Page Up and Page
/// Down change them. Without a printer that prints on both sides, the print on both sides goes
/// by hand: the fronts of the sheets, then the window asks to turn the stack, then the backs.
/// The progress and the result show under the tabs. The texts the window keeps in the settings
/// (print.content), the sheets too (print.*), the layout with the export (export.*).
class PrintBookDialog : public BookOutputDialog {
    Q_OBJECT

public:
    /// @brief How the sheets print on both sides
    enum class Duplex {
        None,       ///< On their fronts only
        LongEdge,   ///< By the printer, the sheet turned along its long edge
        ShortEdge,  ///< By the printer, the sheet turned along its short edge
        Manual,     ///< By hand: the fronts, then the backs on the stack the writer turns
    };

    /// @brief Which pages print
    enum class PageRange {
        All,      ///< Every page of the texts
        Current,  ///< The page of the cursor in the text in front
        Typed,    ///< The pages typed in the field of the range
    };

    /// @brief The state of a printer, as the system says it
    enum class PrinterState { Ready, Printing, Stopped, Error };

    /// @brief A printer of the system
    struct Printer {
        QString name;            ///< As the system names it
        bool isDefault = false;  ///< The default printer of the system
        PrinterState state = PrinterState::Ready;
        bool color = true;       ///< It prints in color
        bool duplex = false;     ///< It prints on both sides of a sheet by itself
        QList<QPageSize> papers;  ///< The sizes of paper it takes
        QPageSize paper;         ///< Its paper at first
        /// Where it does not print on @p paper, lying sideways or not, in points from the edges
        /// of the sheet as it lies; empty: it prints to the edges
        std::function<QMarginsF(const QPageSize& paper, bool sideways)> margins;
    };

    /// @brief The choices of the window: those of the texts and the layout, of the printer and
    ///        of the sheets
    struct Choice : Layout {
        QString printer;        ///< The name of the printer; empty: none
        int copies = 1;
        bool collate = true;    ///< Each copy whole before the next; false: the copies of each
                                ///< sheet together
        PageRange range = PageRange::All;
        QString rangeText;      ///< PageRange::Typed: the pages as typed
        editor::PageParity parity = editor::PageParity::All;
        Duplex duplex = Duplex::None;
        bool color = true;      ///< false: in shades of gray
        QPageSize paper;        ///< The paper of the printer
        editor::SheetOptions sheet;  ///< How the pages lie on a sheet
    };

    /// @brief The pages of the texts of a layout, laid out as for a PDF
    struct LaidOut {
        std::shared_ptr<const editor::BookPages> pages;  ///< Null: no texts
        int current = -1;  ///< The page of the cursor in the text in front, from 0; -1: it is
                           ///< not among them
    };

    /// @brief Lays out the pages of the texts of @p layout
    using PageMaker = std::function<LaidOut(const Layout& layout)>;

    /// @brief Prints @p sides of @p pages on the printer of @p choice, a side a page of the
    /// printer, telling @p progress the sides printed so far (false stops the print); false,
    /// with what went wrong in @p problem, when it cannot. Duplex::LongEdge and ShortEdge: the
    /// printer turns the sheets; Duplex::Manual: the window gives the fronts and the backs one
    /// after the other, each on one side
    using Printing = std::function<bool(const Choice& choice, const editor::BookPages& pages,
                                        const std::vector<editor::SheetSide>& sides,
                                        const std::function<bool(int printed)>& progress,
                                        QStringList* problem)>;

    /// @brief Asks the writer to turn the stack of the @p sheets sheets printed on their fronts
    /// and to put it back into the printer: true prints the backs
    using TurnQuestion = std::function<bool(int sheets)>;

    /// @brief Create the window
    /// @param book What the window says about the book
    /// @param printers The printers of the system
    /// @param pageMaker Lays out the pages
    /// @param printing Prints the sheets
    /// @param turnQuestion Asks to turn the stack; empty: the window's own question
    /// @param parent Parent widget
    PrintBookDialog(const Book& book, const QList<Printer>& printers, PageMaker pageMaker,
                    Printing printing, TurnQuestion turnQuestion = {}, QWidget* parent = nullptr);

    ~PrintBookDialog() override;

    /// @brief The choices of the window now
    Choice choice() const;

    // tabs(): the printer, the texts, the layout and the sheets

    /// @brief The tabs of the printer and of the sheets
    QWidget* printerTab() const { return m_printerTab; }
    QWidget* sheetTab() const { return m_sheetTab; }

    /// @brief The fields of the printer
    QComboBox* printerBox() const { return m_printerBox; }
    QSpinBox* copiesBox() const { return m_copiesBox; }
    QCheckBox* collateBox() const { return m_collateBox; }
    QRadioButton* allPagesButton() const { return m_allButton; }
    QRadioButton* currentPageButton() const { return m_currentButton; }
    QRadioButton* rangeButton() const { return m_rangeButton; }
    QLineEdit* rangeField() const { return m_rangeField; }
    QComboBox* parityBox() const { return m_parityBox; }
    QComboBox* duplexBox() const { return m_duplexBox; }
    QComboBox* colorBox() const { return m_colorBox; }
    QComboBox* paperBox() const { return m_paperBox; }

    /// @brief The line under the printer: its state, its color, its sides; or that the system
    ///        has no printer
    QString printerNote() const { return noteText(m_printerNote); }

    /// @brief The line under the pages: why the range is not one
    QString rangeNote() const { return noteText(m_rangeNote); }

    /// @brief The line under both sides: how the print by hand goes, or how a booklet prints
    QString duplexNote() const { return noteText(m_duplexNote); }

    /// @brief The fields of the sheets
    QRadioButton* sheetButton(editor::SheetLayout layout) const;
    QRadioButton* fitButton() const { return m_fitButton; }
    QRadioButton* actualSizeButton() const { return m_actualSizeButton; }
    QCheckBox* cropMarksBox() const { return m_cropMarksBox; }

    /// @brief The pages chosen to print, from 0, in the order of the print
    const std::vector<int>& chosenPages() const { return m_chosenPages; }

    /// @brief The sides of the sheets of one copy, in the order of the print
    const std::vector<editor::SheetSide>& sheetSides() const { return m_sides; }

    /// @brief The sheets of one copy
    int sheetCount() const;

    /// @brief The side the preview shows, from 0; -1: none
    int shownSide() const { return m_shownSide; }

    /// @brief Show side @p side of the sheets in the preview (from 0; within the sides), as
    /// the buttons under it and Page Up and Page Down in it do
    void showSide(int side);

    /// @brief The preview
    QWidget* preview() const;

    /// @brief The line under the preview: "Sheet 1 of 2, front · page 23"
    QString previewText() const;

    /// @brief What the preview says when it shows no sheet: "The range has no page"; empty
    ///        while it shows one
    QString previewMessage() const;

    /// @brief The sentence under the tabs before the print: how many pages, on how many
    ///        sheets; or why there is nothing to print
    QString statusText() const { return m_status; }

    /// @brief Lay the pages out now, when a change of the texts or of the layout waits for it
    void layOutNow();

protected:
    /// @brief Print the sheets
    void run() override;

    /// @brief The description, and the pages follow the texts
    void contentChanged() override;

    /// @brief The pages follow the texts chosen in the list
    void updateFields() override;

    /// @brief The pages follow the layout
    void layoutChanged() override;

private:
    /// @brief The fields of the printer chosen in the list: its state, its sides, its color
    ///        and its papers
    void showPrinter();

    /// @brief The ways of both sides @p printer has: by the printer, or by hand
    void showDuplexChoices(const Printer* printer);

    /// @brief The duplex follows the sheets: a booklet prints on both sides, along the short
    ///        edge
    void showDuplex();

    /// @brief The options of the size follow the pages on a sheet
    void showSheetOptions();

    /// @brief The pages chosen, the sides of the sheets, the preview, the status and the
    ///        accept button
    void showPages();

    /// @brief Lay the pages out when the events of the change are handled: a change of many
    ///        texts at once lays them out once
    void scheduleLayout();

    /// @brief The printer chosen in the list; null: none
    const Printer* currentPrinter() const;

    /// @brief Whether the sheets print on both sides: by the printer or by hand
    bool twoSided() const;

    /// @brief The size of the paper as it lies, and where the printer prints on it, in points
    QSizeF paperSize() const;
    QRectF printableRect() const;

    /// @brief Ask the writer to turn the stack, in the window's own question
    bool askToTurn(int sheets);

    /// @brief Keep the choices of the window for the next print
    void rememberChoices() const;

    QList<Printer> m_printers;
    PageMaker m_pageMaker;
    Printing m_printing;
    TurnQuestion m_turnQuestion;

    LaidOut m_laidOut;                     ///< The pages of the texts and the layout
    std::vector<int> m_chosenPages;        ///< The pages chosen to print
    std::vector<editor::SheetSide> m_sides;  ///< The sides of one copy
    int m_shownSide = -1;
    QString m_status;
    QTimer* m_layoutTimer = nullptr;
    mutable QHash<QString, QMarginsF> m_margins;  ///< Of the printers, by printer and paper

    QWidget* m_printerTab = nullptr;
    QComboBox* m_printerBox = nullptr;
    FieldNote m_printerNote;
    QSpinBox* m_copiesBox = nullptr;
    QCheckBox* m_collateBox = nullptr;
    QRadioButton* m_allButton = nullptr;
    QRadioButton* m_currentButton = nullptr;
    QRadioButton* m_rangeButton = nullptr;
    QLineEdit* m_rangeField = nullptr;
    FieldNote m_rangeNote;
    QComboBox* m_parityBox = nullptr;
    QComboBox* m_duplexBox = nullptr;
    Duplex m_ownDuplex = Duplex::None;  ///< The writer's choice, which a booklet sets aside
    FieldNote m_duplexNote;
    QComboBox* m_colorBox = nullptr;
    QComboBox* m_paperBox = nullptr;

    QWidget* m_sheetTab = nullptr;
    QRadioButton* m_onePageButton = nullptr;
    QRadioButton* m_twoPagesButton = nullptr;
    QRadioButton* m_bookletButton = nullptr;
    QRadioButton* m_fitButton = nullptr;
    QRadioButton* m_actualSizeButton = nullptr;
    QCheckBox* m_cropMarksBox = nullptr;

    SheetPreview* m_preview = nullptr;
    QLabel* m_previewLabel = nullptr;
    QToolButton* m_previousButton = nullptr;
    QToolButton* m_nextButton = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
