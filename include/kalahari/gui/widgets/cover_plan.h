// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file cover_plan.h
/// @brief The plan of the cover: its parts on sheets – the cover unfolded (the flaps, the back,
/// the spine, the front), the jacket unfolded and the two endpapers – with the designs chosen
/// behind them and the materials of the cover in their places

#ifndef KALAHARI_GUI_WIDGETS_COVER_PLAN_H
#define KALAHARI_GUI_WIDGETS_COVER_PLAN_H

#include "kalahari/core/cover_material.h"

#include <QFrame>
#include <QHash>
#include <QImage>
#include <QList>
#include <QString>
#include <QWidget>

#include <array>
#include <functional>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabBar;
class QToolButton;
class QVBoxLayout;

namespace kalahari::gui {

class FlowLayout;

/// @brief A material of the cover as the plan shows it
struct CoverPlanItem {
    QString id;
    QString part;     ///< core::CoverMaterial::BLURB...
    QString place;    ///< core::CoverMaterial::FRONT...; empty: none yet
    QString state;    ///< core::CoverMaterial::CHOSEN, REJECTED; empty: a draft
    QString title;
    QString text;     ///< Its text; empty: a picture
    QString author;
    QString picture;  ///< Absolute path of its picture; empty: none
    int rotation = 0;
    QString iconId;   ///< Its icon, as the list shows it
};

/// @brief A picture of the book an endpaper can be filled with: of a material of the cover or
/// of another kind with pictures ("Pictures")
struct CoverPlanPicture {
    QString id;       ///< The material
    QString title;
    QString group;    ///< The name of its list: "Cover", "Pictures"
    QString picture;  ///< Absolute path of its picture
    int rotation = 0;

    bool operator==(const CoverPlanPicture&) const = default;
};

/// @brief A material on the plan: its text or picture, what it is and its state
class CoverPlanCard : public QFrame {
    Q_OBJECT

public:
    /// @param remark What it says under its kind: why it waits under the plan ("jacket turned
    /// off"); empty: nothing
    CoverPlanCard(const CoverPlanItem& item, bool narrow, QWidget* parent = nullptr,
                  const QString& remark = QString());

    [[nodiscard]] QString id() const { return m_id; }
    void setCurrent(bool current);
    [[nodiscard]] bool isCurrent() const { return m_current; }

signals:
    void clicked(const QString& id);
    void doubleClicked(const QString& id);
    void menuRequested(const QString& id, const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;

private:
    QString m_id;
    bool m_current = false;
    bool m_rejected = false;
};

/// @brief An endpaper as a spread of two pages: the one pasted to the cover and the free leaf,
/// with its fill and the fold between them
class EndpaperSpread : public QWidget {
    Q_OBJECT

public:
    explicit EndpaperSpread(QWidget* parent = nullptr);

    /// @brief Show @p endpaper, with the picture @p picture of its fill (PICTURE, TILE)
    void setEndpaper(const core::Endpaper& endpaper, const QImage& picture);

    [[nodiscard]] const core::Endpaper& endpaper() const { return m_endpaper; }

    void setCurrent(bool current);
    [[nodiscard]] bool isCurrent() const { return m_current; }

    [[nodiscard]] bool hasHeightForWidth() const override { return true; }
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    core::Endpaper m_endpaper;
    QImage m_picture;
    bool m_current = false;
};

/// @brief The plan of the cover of the list of the cover (MaterialListPanel)
///
/// Above the plan the parts of the cover of the book, each turned on on its own: the flaps of
/// the cover, the jacket and the endpapers (core::CoverParts, saved with the book by the
/// list). Each part has its sheet on a tab: the cover, the jacket, the endpapers; Ctrl+Page
/// Down and Ctrl+Page Up go to the next and the previous one.
///
/// The sheet of the cover or of the jacket has the back flap, the back, the spine, the front
/// and the front flap side by side, with the design chosen (or the one picked above the
/// sheet) behind the front, and the descriptions, taglines, opinions, notes about the author
/// and photos of the author in their places; under the sheet the materials without a place,
/// and those of a part turned off, which say so. Only what goes on a cover is there: the
/// designs above the sheet, not the reference pictures and the notes. The materials rejected
/// are shown on request.
///
/// The sheet of the endpapers has the front and the back endpaper, each a spread with its
/// settings beside it: a picture over both pages, a picture repeated, a pattern of the program
/// in two colors, one color or none; the back one can be as the front one.
///
/// Every material is a card that takes the keys: the arrows go to the cards above, below and
/// beside it, Enter opens it as the list does, the menu key and Shift+F10 open its menu.
class CoverPlan : public QWidget {
    Q_OBJECT

public:
    /// @name The sheets of the plan, on its tabs
    /// @{
    static constexpr const char* COVER_SHEET = "cover";
    static constexpr const char* JACKET_SHEET = "jacket";
    static constexpr const char* ENDPAPERS_SHEET = "endpapers";
    /// @}

    /// @name The sides of the endpapers
    /// @{
    static constexpr const char* FRONT_ENDPAPER = "front";
    static constexpr const char* BACK_ENDPAPER = "back";
    /// @}

    /// @name The colors of the theme ("editor" section) a new pattern and an endpaper without
    /// a fill take
    /// @{
    static constexpr const char* PATTERN_COLOR_KEY = "coverEndpaperPattern";
    static constexpr const char* PAPER_COLOR_KEY = "coverEndpaperPaper";
    static constexpr const char* BLANK_COLOR_KEY = "coverEndpaperBlank";
    /// @}

    explicit CoverPlan(QWidget* parent = nullptr);

    /// @brief Show @p items, keeping the material chosen when it is among them
    void setItems(const QList<CoverPlanItem>& items);

    /// @brief Show the parts of the cover @p parts (the boxes above the plan, the sheets, the
    /// endpapers), without partsChanged()
    void setParts(const core::CoverParts& parts);

    /// @brief The parts of the cover the plan shows
    [[nodiscard]] const core::CoverParts& parts() const { return m_parts; }

    /// @brief The pictures of the book an endpaper can be filled with
    void setPictures(const QList<CoverPlanPicture>& pictures);

    /// @brief Whether the parts and the endpapers can be changed: not in a book open read-only
    void setEditable(bool editable);

    /// @brief The material chosen; empty: none
    [[nodiscard]] QString current() const { return m_current; }

    /// @brief Choose @p id on the plan, without currentChanged()
    void setCurrent(const QString& id);

    /// @brief Give the keys to the material chosen, else to the first one
    void focusCurrent();

    /// @brief Whether the plan has a card of @p id
    [[nodiscard]] bool hasCard(const QString& id) const { return m_cards.contains(id); }

    /// @brief The place whose column shows @p id; empty: under the plan, without a place
    [[nodiscard]] QString columnOf(const QString& id) const { return m_columnOf.value(id); }

    /// @brief The design behind the sheet shown; empty: none
    [[nodiscard]] QString designShown() const { return m_design; }

    /// @brief The sheet shown: COVER_SHEET...
    [[nodiscard]] QString sheet() const { return m_sheetShown; }

    /// @brief Show @p sheet, when the book has its part
    /// @return false for a sheet of a part turned off
    bool showSheet(const QString& sheet);

    /// @brief The sheets of the parts the book has, in the order of their tabs
    [[nodiscard]] QStringList sheets() const;

    /// @brief The spread of the endpaper chosen: FRONT_ENDPAPER, BACK_ENDPAPER; empty: none
    [[nodiscard]] QString currentEndpaper() const { return m_currentEndpaper; }

    /// @brief The boxes of the parts of the cover above the plan
    [[nodiscard]] QCheckBox* flapsBox() const { return m_flaps; }
    [[nodiscard]] QCheckBox* jacketBox() const { return m_jacket; }
    [[nodiscard]] QCheckBox* endpapersBox() const { return m_endpapers; }

    /// @brief The box that shows the materials rejected
    [[nodiscard]] QCheckBox* rejectedBox() const { return m_rejected; }

    /// @brief The tabs of the sheets
    [[nodiscard]] QTabBar* sheetTabs() const { return m_tabs; }

    /// @brief The spread of the endpaper of @p side
    [[nodiscard]] EndpaperSpread* spread(const QString& side) const;

    /// @brief The box of the fill of the endpaper of @p side
    [[nodiscard]] QComboBox* fillBox(const QString& side) const;

    /// @brief The box of the picture of the endpaper of @p side
    [[nodiscard]] QComboBox* pictureBox(const QString& side) const;

    /// @brief The field of the size of the picture repeated on the endpaper of @p side
    [[nodiscard]] QSpinBox* scaleBox(const QString& side) const;

    /// @brief The button of @p pattern of the endpaper of @p side
    [[nodiscard]] QToolButton* patternButton(const QString& side, const QString& pattern) const;

    /// @brief The box of the back endpaper: filled as the front one
    [[nodiscard]] QCheckBox* sameAsFrontBox() const;

    /// @brief The color a pattern of @p endpaper is drawn in: its own, else the theme's
    [[nodiscard]] static QColor patternColorOf(const core::Endpaper& endpaper);

    /// @brief The color under a pattern of @p endpaper: its own, else the theme's
    [[nodiscard]] static QColor paperColorOf(const core::Endpaper& endpaper);

    /// @brief The one color of @p endpaper: its own, else the theme's paper
    [[nodiscard]] static QColor colorOf(const core::Endpaper& endpaper);

    /// @brief The color of an endpaper without a fill: the theme's
    [[nodiscard]] static QColor blankColor();

signals:
    /// @brief The writer chose @p id on the plan
    void currentChanged(const QString& id);

    /// @brief Enter or a double-click on @p id
    void activated(const QString& id);

    /// @brief The menu of @p id (the right mouse button, the menu key, Shift+F10)
    void menuRequested(const QString& id, const QPoint& globalPos);

    /// @brief The writer turned a part of the cover on or off, or changed an endpaper: the
    /// book gets @p parts
    void partsChanged(const core::CoverParts& parts);

    /// @brief The writer chose the spread of the endpaper of @p side
    void endpaperChosen(const QString& side);

    /// @brief The writer asks for a picture from the disk for the endpaper of @p side
    void endpaperPictureRequested(const QString& side);

    /// @brief What the status bar says after a change of the plan
    void message(const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    /// @brief The fields of the settings of one endpaper, beside its spread
    struct EndpaperControls {
        EndpaperSpread* spread = nullptr;
        QWidget* settings = nullptr;
        QFormLayout* form = nullptr;
        QCheckBox* same = nullptr;  ///< Of the back one: as the front one
        QComboBox* fill = nullptr;
        QComboBox* picture = nullptr;
        QPushButton* fromDisk = nullptr;
        QSpinBox* scale = nullptr;
        QWidget* patterns = nullptr;
        QButtonGroup* patternButtons = nullptr;
        QWidget* colors = nullptr;
        QToolButton* patternColor = nullptr;
        QToolButton* paperColor = nullptr;
        QToolButton* color = nullptr;
    };

    void rebuild();
    void rebuildTabs();
    void rebuildDesigns(const QList<const CoverPlanItem*>& designs);
    void rebuildSheet();
    void updateEndpapers();
    void showEndpaperSettings(const QString& side);
    QWidget* newEndpaperRow(const QString& side);
    [[nodiscard]] EndpaperControls& controlsOf(const QString& side);
    [[nodiscard]] const EndpaperControls& controlsOf(const QString& side) const;
    [[nodiscard]] core::Endpaper& endpaperOf(const QString& side);
    /// @brief Change the endpaper of @p side with @p change, show it and save it
    void changeEndpaper(const QString& side, const std::function<void(core::Endpaper&)>& change);
    void changePart(bool core::CoverParts::*part, bool on);
    void pickColor(const QString& side, QString core::Endpaper::*field, const QColor& shown);
    void chooseEndpaper(const QString& side, bool focus);
    void choose(const QString& id, bool focus);
    /// @brief The card beside @p card in @p direction (Qt::Key_Left...); nullptr: none
    CoverPlanCard* neighbour(const CoverPlanCard* card, int key) const;
    [[nodiscard]] QImage pictureOf(const QString& id, const QSize& size) const;
    [[nodiscard]] QString pictureTitleOf(const QString& id) const;

    QCheckBox* m_flaps = nullptr;
    QCheckBox* m_jacket = nullptr;
    QCheckBox* m_endpapers = nullptr;
    QCheckBox* m_rejected = nullptr;
    QTabBar* m_tabs = nullptr;
    QWidget* m_designs = nullptr;
    QLabel* m_designsLabel = nullptr;
    QLabel* m_noDesign = nullptr;
    QHBoxLayout* m_designsLayout = nullptr;
    QButtonGroup* m_designButtons = nullptr;
    QWidget* m_sheet = nullptr;
    QHBoxLayout* m_sheetLayout = nullptr;
    QWidget* m_endpaperSheet = nullptr;
    QLabel* m_unplacedLabel = nullptr;
    QWidget* m_unplaced = nullptr;
    FlowLayout* m_unplacedLayout = nullptr;
    QLabel* m_note = nullptr;
    std::array<EndpaperControls, 2> m_endpaperControls;

    QList<CoverPlanItem> m_items;
    QList<CoverPlanPicture> m_pictures;
    mutable QHash<QString, QImage> m_pictureCache;  ///< The pictures of the spreads, by id
    core::CoverParts m_parts;
    QString m_sheetShown = QString::fromLatin1(COVER_SHEET);
    QString m_current;
    QString m_currentEndpaper;
    QString m_design;        ///< The design shown behind the sheet
    bool m_designPicked = false;  ///< The writer picked the design above the sheet
    bool m_editable = true;
    bool m_updating = false;  ///< The fields of the endpapers are filled
    QHash<QString, CoverPlanCard*> m_cards;
    QHash<QString, QString> m_columnOf;
    QList<QList<CoverPlanCard*>> m_columns;  ///< The cards of the columns shown, then those
                                            ///< without a place, in the order of the keys
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_COVER_PLAN_H
