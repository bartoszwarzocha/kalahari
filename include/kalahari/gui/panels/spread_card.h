// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file spread_card.h
/// @brief The card of a spread: its two pages side by side as in an open book, its settings
///        beside them and its place among the pages of the book
///
/// The card opens on a tab of its own when the writer opens a spread (core::SpreadData) in
/// the Navigator. The settings change the spread at once (ProjectManager::setSpread()); a
/// picture that fills its area moves with the mouse or the arrows (Shift: farther), Ctrl+]
/// and Ctrl+[ make it larger and smaller, Home puts it in the middle. The band of the spine
/// and the frame of the area of the picture show only on the card (Settings > Editor >
/// Colors); the paper is the editor's.

#ifndef KALAHARI_GUI_PANELS_SPREAD_CARD_H
#define KALAHARI_GUI_PANELS_SPREAD_CARD_H

#include "kalahari/core/spread.h"
#include "kalahari/editor/spread_look.h"

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QString>
#include <QWidget>

#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QTimer;
class QToolButton;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

class BookPageNumbers;
class LengthSpinBox;

/// @brief The two pages of a spread side by side, as an open book shows them
class SpreadPagesView : public QWidget {
    Q_OBJECT

public:
    /// @brief How far an arrow moves a picture that fills its area, in millimetres (Shift:
    ///        FAST_STEP_MM)
    static constexpr double STEP_MM = 1.0;
    static constexpr double FAST_STEP_MM = 10.0;

    /// @brief The colors of the paper and of the hints of the card
    struct Colors {
        QColor paper = Qt::white;
        QColor text = Qt::black;
        QColor fold;   ///< The band of the spine; invalid: none
        QColor guide;  ///< The frame of the area; invalid: none
    };

    explicit SpreadPagesView(QWidget* parent = nullptr);

    /// @brief What the pages show: the look of the spread on the page of the book
    void setLook(const editor::SpreadLook& look, const editor::SpreadPage& page);
    [[nodiscard]] const editor::SpreadLook& look() const { return m_look; }

    /// @brief Its picture as turned, perhaps smaller than its file; null: none
    void setPicture(const QImage& picture);

    void setColors(const Colors& colors);
    [[nodiscard]] const Colors& colors() const { return m_colors; }

    /// @brief The font of the book's text: of the caption and the numbers of the pages, at
    ///        its size on paper
    void setTextFont(const QFont& font);

    /// @brief The band of the spine and the frame of the area, shown or not
    void setFoldShown(bool shown);
    void setGuidesShown(bool shown);
    [[nodiscard]] bool foldShown() const { return m_fold; }
    [[nodiscard]] bool guidesShown() const { return m_guides; }

    /// @brief The real size of the pages (100%); false: the pages fit the view
    void setActualSize(bool actual);
    [[nodiscard]] bool actualSize() const { return m_actual; }

    /// @brief The number of its left page (the right one has the next); 0: unknown
    void setFirstPage(int number);

    /// @brief Whether the picture moves in its area: it fills it, and there is one
    [[nodiscard]] bool movable() const;

    /// @brief Where the pages are drawn, in the view's pixels
    [[nodiscard]] QRectF pagesRect() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    /// @brief The pages fit the width (its height follows), or have their real size
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

signals:
    /// @brief The writer moved the picture in its area: its new place (SpreadLook::cropX/Y);
    ///        a drag when it ends
    void cropMoved(double cropX, double cropY);
    /// @brief Ctrl+] (+1) or Ctrl+[ (-1): the picture a step larger or smaller
    void zoomStepped(int step);
    /// @brief Home: the picture in the middle, without zoom
    void centerAsked();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    /// @brief The pixels of an inch of the pages as drawn
    [[nodiscard]] double drawnDpi() const;

    /// @brief The size of the pages at 100%, in the view's pixels
    [[nodiscard]] QSizeF actualPagesSize() const;

    /// @brief The picture moved by @p deltaMm from where the drag started, or from where it is
    void moveBy(const QPointF& deltaMm, bool fromDragStart);

    /// @brief The accessible name and the cursor: the spread, its pages, the keys
    void updateHints();

    /// @brief The height of the line of the pages' names under them
    [[nodiscard]] int labelsHeight() const;

    editor::SpreadLook m_look;
    editor::SpreadPage m_page;
    QImage m_picture;
    QSize m_pictureSize;  ///< Of the file as turned: the geometry's proportions
    Colors m_colors;
    QFont m_font;
    bool m_fold = true;
    bool m_guides = true;
    bool m_actual = false;
    int m_firstPage = 0;
    bool m_dragging = false;
    QPointF m_dragStart;           ///< Where the drag started, in pixels
    QPointF m_dragCrop;            ///< The crop then
};

/// @brief The pages around a spread, in pairs as in an open book, with its own two marked
class SpreadStrip : public QWidget {
    Q_OBJECT

public:
    /// @brief A page of the book
    struct Page {
        int number = 0;      ///< From 1
        QString title;       ///< What is on it; empty: an empty page
        bool own = false;    ///< A page of this spread
        bool spread = false; ///< A page of a spread
        bool right = false;  ///< The right half of the spread's picture
    };

    explicit SpreadStrip(QWidget* parent = nullptr);

    /// @brief The pages shown in reading order, from a left page (or page 1, a right one);
    ///        empty: none counted yet
    void setPages(const std::vector<Page>& pages);
    [[nodiscard]] const std::vector<Page>& pages() const { return m_pages; }

    /// @brief A small picture of the spread, for its two pages
    void setPicture(const QImage& picture);

    void setColors(const QColor& paper, const QColor& text, const QColor& mark);

    /// @brief The height of a page to its width
    void setPageRatio(double ratio);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief The size of a small page
    [[nodiscard]] QSize miniSize() const;

    std::vector<Page> m_pages;
    double m_ratio = 1.42;
    QImage m_picture;
    QColor m_paper = Qt::white;
    QColor m_text = Qt::black;
    QColor m_mark;
};

/// @brief The card of a spread on its tab
class SpreadCard : public QWidget {
    Q_OBJECT

public:
    /// @brief How much Ctrl+] and Ctrl+[ change the zoom of a picture that fills its area
    static constexpr int ZOOM_STEP = 5;

    /// @brief What the dots per inch of the picture are good for (Quality for Print)
    static constexpr int PRINT_DPI = 300;
    static constexpr int HOME_PRINT_DPI = 200;

    /// @brief How long after the last change of a drag, an arrow or a key of the caption the
    ///        spread is saved (ms)
    static constexpr int SAVE_DELAY_MS = 600;

    /// @brief The captions of the book: the context of the spread's caption (its number) and
    ///        the labels to choose from (CaptionCoordinator)
    struct Captions {
        std::function<editor::CaptionContext(const QString& elementId)> context;
        std::function<QStringList()> labels;
    };

    /// @param book The book of its window
    /// @param pages The pages of the book (its place among them); nullptr: none shown
    /// @param elementId The spread
    SpreadCard(core::ProjectManager& book, const BookPageNumbers* pages,
               const QString& elementId, QWidget* parent = nullptr);
    ~SpreadCard() override;

    SpreadCard(const SpreadCard&) = delete;
    SpreadCard& operator=(const SpreadCard&) = delete;

    [[nodiscard]] QString elementId() const { return m_elementId; }

    /// @brief Show the spread as the book has it now, its pictures and its pages
    void refresh();

    /// @brief Give the keys to the picture's list (the bar's Picture...)
    void focusPicture();

    /// @brief Where the numbers of the captions and their labels come from; without them the
    ///        caption counts from 1 and the labels are those of the Settings
    void setCaptions(const Captions& captions);

    /// @brief Save at once what waits to be saved (a drag, the text of the caption)
    void saveNow();

    /// @name The parts of the card (tests)
    /// @{
    [[nodiscard]] SpreadPagesView* view() const { return m_view; }
    [[nodiscard]] SpreadStrip* strip() const { return m_strip; }
    [[nodiscard]] QComboBox* pictureBox() const { return m_pictureBox; }
    [[nodiscard]] QRadioButton* edgeButton() const { return m_edgeButton; }
    [[nodiscard]] QRadioButton* marginsButton() const { return m_marginsButton; }
    [[nodiscard]] QRadioButton* fillButton() const { return m_fillButton; }
    [[nodiscard]] QRadioButton* wholeButton() const { return m_wholeButton; }
    [[nodiscard]] LengthSpinBox* gutterBox() const { return m_gutterBox; }
    [[nodiscard]] QCheckBox* captionBox() const { return m_captionBox; }
    [[nodiscard]] QComboBox* labelBox() const { return m_labelBox; }
    [[nodiscard]] QLineEdit* captionField() const { return m_captionField; }
    [[nodiscard]] QCheckBox* numbersBox() const { return m_numbersBox; }
    [[nodiscard]] QCheckBox* contentsBox() const { return m_contentsBox; }
    [[nodiscard]] QToolButton* foldButton() const { return m_foldButton; }
    [[nodiscard]] QToolButton* guidesButton() const { return m_guidesButton; }
    [[nodiscard]] QToolButton* wholeSpreadButton() const { return m_wholeSpreadButton; }
    [[nodiscard]] QToolButton* actualSizeButton() const { return m_actualSizeButton; }
    [[nodiscard]] QPushButton* earlierButton() const { return m_earlierButton; }
    [[nodiscard]] QPushButton* laterButton() const { return m_laterButton; }
    [[nodiscard]] QPushButton* centerButton() const { return m_centerButton; }
    /// @brief The line of the quality for print: "Quality for print: 312 dpi – ..."; empty
    ///        without a picture
    [[nodiscard]] QString qualityText() const;
    /// @brief The line of the place in the book: "pages 86–87 · ..."
    [[nodiscard]] QString placeText() const;
    /// @brief The title on the bar: "Map · spread · pages 86–87"
    [[nodiscard]] QString headText() const;
    /// @}

signals:
    /// @brief File > Print... (Ctrl+P) from the bar
    void printRequested();
    /// @brief The gear: Settings > Editor > Images, the group Spreads
    void settingsRequested();
    /// @brief Earlier or Later: the spread moves in its list as Move Up and Move Down do
    void moveRequested(const QString& elementId, int direction);
    /// @brief A picture from the disk went to the Materials
    void materialAdded(const QString& materialId);
    /// @brief A line for the status bar
    void statusMessage(const QString& text);

private:
    QWidget* createBar();
    QWidget* createSettings();
    void addPictureRows(QFormLayout* form, QWidget* parent);
    void addLookRows(QFormLayout* form, QWidget* parent);
    void addCaptionRows(QFormLayout* form, QWidget* parent);
    QWidget* createStripBox(QWidget* parent);
    void connectFields();

    /// @brief The pictures of the Materials and of the cover in the picture's list
    void fillPictures();
    /// @brief The labels of the captions of the book in the label's list
    void fillLabels();

    /// @brief The fields, the pages and the strip as the spread is
    void showData();
    void showPicture();
    void showPlace();
    void showQuality();
    void showColors();
    void showMoves();

    /// @brief Give the spread the settings @p spread (the book saves them); @p later: in
    ///        SAVE_DELAY_MS
    void store(const core::SpreadData& spread, bool later = false);

    /// @brief The look and the page shown, as m_data has them
    void showLook();

    /// @brief The look of the pages from the settings @p spread
    [[nodiscard]] editor::SpreadLook lookOf(const core::SpreadData& spread) const;

    /// @brief Choose a picture on the disk: it goes to Materials > Pictures and the spread
    ///        shows it
    void pictureFromDisk();

    core::ProjectManager& m_book;
    const BookPageNumbers* m_pages;
    QString m_elementId;
    core::SpreadData m_data;      ///< As the card shows it
    QString m_pictureFile;        ///< The file of its picture; empty: none
    QSize m_pictureSize;          ///< Of the file as turned
    QImage m_picture;             ///< As the pages show it (smaller than the file)
    int m_pictureRotation = 0;    ///< The turn of the file read
    QString m_pictureList;        ///< What the list of pictures has (fillPictures())
    QString m_labelList;          ///< What the list of labels has (fillLabels())
    editor::SpreadPage m_page;    ///< The page of the book (the Settings)
    Captions m_captions;
    bool m_loading = false;
    int m_settingsListener = -1;
    QTimer* m_settingsTimer = nullptr;
    QTimer* m_saveTimer = nullptr;

    QLabel* m_headIcon = nullptr;
    QLabel* m_head = nullptr;
    QToolButton* m_wholeSpreadButton = nullptr;
    QToolButton* m_actualSizeButton = nullptr;
    QToolButton* m_foldButton = nullptr;
    QToolButton* m_guidesButton = nullptr;
    QScrollArea* m_desk = nullptr;
    QWidget* m_stripBox = nullptr;
    QScrollArea* m_stripArea = nullptr;
    QWidget* m_areaRow = nullptr;     ///< The radios of the area (its label's buddy)
    QWidget* m_fitRow = nullptr;      ///< The radios of the fit
    SpreadPagesView* m_view = nullptr;
    SpreadStrip* m_strip = nullptr;
    QLabel* m_place = nullptr;
    QPushButton* m_earlierButton = nullptr;
    QPushButton* m_laterButton = nullptr;
    QComboBox* m_pictureBox = nullptr;
    QPushButton* m_diskButton = nullptr;
    QLabel* m_fileInfo = nullptr;
    QRadioButton* m_marginsButton = nullptr;
    QRadioButton* m_edgeButton = nullptr;
    QRadioButton* m_wholeButton = nullptr;
    QRadioButton* m_fillButton = nullptr;
    QLabel* m_cropHint = nullptr;
    QPushButton* m_centerButton = nullptr;
    LengthSpinBox* m_gutterBox = nullptr;
    QCheckBox* m_captionBox = nullptr;
    QComboBox* m_labelBox = nullptr;
    QLineEdit* m_captionField = nullptr;
    QLabel* m_edgeCaptionHint = nullptr;
    QCheckBox* m_numbersBox = nullptr;
    QCheckBox* m_contentsBox = nullptr;
    QLabel* m_qualityIcon = nullptr;
    QLabel* m_quality = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_SPREAD_CARD_H
