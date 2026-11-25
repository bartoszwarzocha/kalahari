// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file print_book_dialog.h
/// @brief File > Print: a large preview of the sheets and the sections of the printer, the
///        texts, the pages, the sheet, the page of the book and the look of the text, and the
///        progress and the result of the print in one window

#pragma once

#include "kalahari/editor/book_print.h"
#include "kalahari/gui/dialogs/book_output_dialog.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMarginsF>
#include <QPageSize>
#include <QSize>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class QAbstractButton;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QScrollBar;
class QSpinBox;
class QTimer;
class QToolButton;

namespace kalahari {
namespace editor {
class BookPages;
} // namespace editor

namespace gui {
class CollapsibleSection;
class LengthSpinBox;

namespace dialogs {

class SheetPreview;

/// @brief Print of the book, of some of its texts or of the text in front
///
/// The window is the program's own; only Printer Properties... opens the window of the
/// printer's driver, which the system has (Windows). At the left a large preview shows the
/// sheets as they come out of the printer, the whole sheet or as wide as the preview; Page Up
/// and Page Down change them. At the right the settings are in sections that open and close,
/// in the order of the choices, each closed one with a summary in its header:
/// - the printer (the printers of the system, the default one first; its state, the copies,
///   its paper source and quality when the system lists them, Printer Properties...);
/// - what to print (the texts of the export: BookOutputDialog; the chosen ones in the small
///   window of Choose Texts...);
/// - the pages (all, the current one or a range; the odd or the even ones; in the reverse
///   order or not);
/// - the sheet (the paper, the color, both sides; a page, two pages side by side or a booklet
///   folded in half; a page fitted to the sheet or of its own size, with crop marks);
/// - the page of the book (its size and margins: the settings of Settings > Editor > Pages and
///   Margins, which the editor and the export share);
/// - the look of the text (the layout of the export).
/// The lists offer only what the printer can do. Without a printer that prints on both sides,
/// the print on both sides goes by hand: the fronts of the sheets, then the window asks to
/// turn the stack, then the backs. The progress and the result show under the preview and the
/// sections. The texts the window keeps in the settings (print.content), the sheets, the
/// sections open and the zoom of the preview too (print.*), the layout with the export
/// (export.*).
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

    /// @brief The sections of the window, in their order
    enum class Section { Printer, Content, Pages, Sheet, BookPage, Look };

    /// @brief How the preview shows a sheet
    enum class Zoom {
        WholeSheet,  ///< The whole sheet in the preview
        Width,       ///< As wide as the preview; the sheet scrolls
    };

    /// @brief A margin of the page of the book
    enum class Margin { Top, Bottom, Left, Right, Inner, Outer };

    /// @brief A paper source of a printer (a tray), as its driver names it
    struct PaperSource {
        int id = 0;    ///< As the driver knows it
        QString name;  ///< As the driver names it
    };

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
        /// Its paper sources, as the system lists them; empty: the system does not say (the
        /// driver chooses)
        QList<PaperSource> sources;
        int source = -1;  ///< The id of its source at first; -1: the first one
        /// Its qualities: the dots per inch across and down; empty: the system does not say
        QList<QSize> qualities;
        QSize quality;  ///< Its quality at first; invalid: the driver's own
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
        bool reverse = false;   ///< The last sheet first, for a printer that lays them face up
        Duplex duplex = Duplex::None;
        bool color = true;      ///< false: in shades of gray
        QPageSize paper;        ///< The paper of the printer
        editor::SheetOptions sheet;  ///< How the pages lie on a sheet
        int source = -1;        ///< The id of the paper source; -1: the driver's own
        QSize quality;          ///< The dots per inch; invalid: the driver's own
        /// The settings of the window of the driver of the printer (on Windows its DEVMODE),
        /// which the print takes with the choices of the window; empty: none
        QByteArray driverSettings;
    };

    /// @brief What the window of the driver of the printer chose: all its settings, and those
    ///        the fields of the window show
    struct DriverChoice {
        QByteArray settings;  ///< All its settings, for the print
        QPageSize paper;      ///< Invalid: it did not say
        int copies = 0;       ///< 0: it did not say
        std::optional<bool> collate;
        std::optional<bool> color;
        std::optional<Duplex> duplex;  ///< None, LongEdge or ShortEdge
        int source = -1;  ///< The id of the paper source; -1: it did not say
        QSize quality;    ///< Invalid: it did not say
    };

    /// @brief Shows the window of the driver of the printer of @p choice over @p parent, with
    /// the choices of the window: what it chose, or nothing when the writer cancelled it
    using DriverWindow =
        std::function<std::optional<DriverChoice>(const Choice& choice, QWidget* parent)>;

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

    /// @brief A section of the window
    [[nodiscard]] CollapsibleSection* section(Section which) const;

    /// @brief Where the window shows the window of the driver of the printer; empty: Printer
    /// Properties... waits (a system without such a window)
    void setDriverWindow(DriverWindow window);

    /// @brief Show the window of the driver of the printer, as Printer Properties... does:
    /// its choices go to the fields of the window and to the print
    void showDriverWindow();

    /// @brief The fields of the printer
    QComboBox* printerBox() const { return m_printerBox; }
    QSpinBox* copiesBox() const { return m_copiesBox; }
    QCheckBox* collateBox() const { return m_collateBox; }
    QRadioButton* allPagesButton() const { return m_allButton; }
    QRadioButton* currentPageButton() const { return m_currentButton; }
    QRadioButton* rangeButton() const { return m_rangeButton; }
    QLineEdit* rangeField() const { return m_rangeField; }
    QComboBox* parityBox() const { return m_parityBox; }
    QCheckBox* reverseBox() const { return m_reverseBox; }
    QComboBox* duplexBox() const { return m_duplexBox; }
    QComboBox* colorBox() const { return m_colorBox; }
    QComboBox* paperBox() const { return m_paperBox; }
    [[nodiscard]] QPushButton* driverButton() const { return m_driverButton; }
    [[nodiscard]] QComboBox* sourceBox() const { return m_sourceBox; }
    [[nodiscard]] QComboBox* qualityBox() const { return m_qualityBox; }

    /// @brief The line under the printer: its state, its color, its sides; or that the system
    ///        has no printer
    QString printerNote() const { return sectionNote(m_printerNote); }

    /// @brief The line under the pages: why the range is not one
    QString rangeNote() const { return sectionNote(m_rangeNote); }

    /// @brief The line under both sides: how the print by hand goes, or how a booklet prints
    QString duplexNote() const { return sectionNote(m_duplexNote); }

    /// @brief The line under the pages on a sheet: where the halves of a spread on the side
    ///        shown lie
    QString spreadNote() const { return sectionNote(m_spreadNote); }

    /// @brief Under the preview: to the sheet of the spread in front, else of the next spread
    [[nodiscard]] QToolButton* spreadButton() const { return m_spreadButton; }

    /// @brief Show the side with the spread in front (its first page the page of the cursor),
    ///        else the next spread after the side shown, else the first one
    void showSpread();

    /// @brief The fields of the sheets
    QRadioButton* sheetButton(editor::SheetLayout layout) const;
    QRadioButton* fitButton() const { return m_fitButton; }
    QRadioButton* actualSizeButton() const { return m_actualSizeButton; }
    QCheckBox* cropMarksBox() const { return m_cropMarksBox; }

    /// @brief The fields of the page of the book: those of Settings > Editor > Pages and
    ///        Margins
    [[nodiscard]] QComboBox* pageSizeBox() const { return m_pageSizeBox; }
    [[nodiscard]] LengthSpinBox* pageWidthBox() const { return m_pageWidthBox; }
    [[nodiscard]] LengthSpinBox* pageHeightBox() const { return m_pageHeightBox; }
    [[nodiscard]] LengthSpinBox* marginBox(Margin margin) const;
    [[nodiscard]] QCheckBox* mirrorBox() const { return m_mirrorBox; }

    /// @brief How the preview shows a sheet
    [[nodiscard]] Zoom zoom() const { return m_zoom; }
    void setZoom(Zoom zoom);
    [[nodiscard]] QAbstractButton* zoomButton(Zoom zoom) const;

    /// @brief The scroll bar of a sheet as wide as the preview
    [[nodiscard]] QScrollBar* previewScrollBar() const { return m_previewScroll; }

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

    /// @brief The sentence under the preview and the sections before the print: how many
    ///        pages, on how many sheets; or why there is nothing to print
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

    /// @brief The pages follow the layout, and the fields of the page its settings
    void layoutChanged() override;

private:
    /// @brief A new section at the end of the column of the sections: its grid
    QGridLayout* newSection(const QString& title);

    /// @brief The text of @p note while it shows in its section, open or closed
    [[nodiscard]] QString sectionNote(const FieldNote& note) const;

    /// @brief The sections of the printer, of the pages and of the sheet, and their fields
    void addPrinterSection();
    void addPagesSection();
    void addSheetSection();

    /// @brief The section of the page of the book: its fields write the settings of the
    ///        editor
    void addBookPageSection();

    /// @brief The preview with its zoom and the buttons of the sheets
    /// @return Its widget, for the left of the window
    QWidget* newPreview();

    /// @brief The fields of the page of the book as the settings have them
    void showBookPage();

    /// @brief The summaries in the headers of the sections
    void showSummaries();

    /// @brief The paper sources and the qualities of the printer chosen
    void showSourcesAndQualities(const Printer* printer);

    /// @brief Put what the window of the driver chose into the fields
    void showDriverChoice(const DriverChoice& chosen);

    /// @brief Keep which sections are open
    void rememberSections() const;

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

    /// @brief The line under the pages on a sheet for side @p shown: where the halves of a
    ///        spread lie; and the button to the spreads, when the pages chosen have one
    void showSpreadNote(const editor::SheetSide& shown, editor::SheetLayout layout);

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

    DriverWindow m_driverWindow;
    QByteArray m_driverSettings;  ///< What the window of the driver chose, for its printer
    Zoom m_zoom = Zoom::WholeSheet;

    QList<CollapsibleSection*> m_sections;  ///< In the order of Section
    QWidget* m_sectionsColumn = nullptr;     ///< The content of the column of the sections
    QWidget* m_sectionsArea = nullptr;       ///< The column of the sections, which scrolls
    QComboBox* m_printerBox = nullptr;
    QPushButton* m_driverButton = nullptr;
    QComboBox* m_sourceBox = nullptr;
    QComboBox* m_qualityBox = nullptr;
    FieldNote m_printerNote;
    QSpinBox* m_copiesBox = nullptr;
    QCheckBox* m_collateBox = nullptr;
    QCheckBox* m_reverseBox = nullptr;
    QRadioButton* m_allButton = nullptr;
    QRadioButton* m_currentButton = nullptr;
    QRadioButton* m_rangeButton = nullptr;
    QLineEdit* m_rangeField = nullptr;
    FieldNote m_rangeNote;
    QComboBox* m_parityBox = nullptr;
    QComboBox* m_duplexBox = nullptr;
    Duplex m_ownDuplex = Duplex::None;  ///< The writer's choice, which a booklet sets aside
    FieldNote m_duplexNote;
    FieldNote m_spreadNote;
    QComboBox* m_colorBox = nullptr;
    QComboBox* m_paperBox = nullptr;

    QRadioButton* m_onePageButton = nullptr;
    QRadioButton* m_twoPagesButton = nullptr;
    QRadioButton* m_bookletButton = nullptr;
    QRadioButton* m_fitButton = nullptr;
    QRadioButton* m_actualSizeButton = nullptr;
    QCheckBox* m_cropMarksBox = nullptr;

    QComboBox* m_pageSizeBox = nullptr;
    LengthSpinBox* m_pageWidthBox = nullptr;
    LengthSpinBox* m_pageHeightBox = nullptr;
    QList<LengthSpinBox*> m_marginBoxes;  ///< In the order of Margin
    QCheckBox* m_mirrorBox = nullptr;

    SheetPreview* m_preview = nullptr;
    QScrollBar* m_previewScroll = nullptr;
    QToolButton* m_wholeSheetButton = nullptr;
    QToolButton* m_widthButton = nullptr;
    QLabel* m_previewLabel = nullptr;
    QToolButton* m_previousButton = nullptr;
    QToolButton* m_nextButton = nullptr;
    QToolButton* m_spreadButton = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
