// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cover_plan.h
/// @brief The plan of the cover: the cover unfolded (the flaps, the back, the spine, the
/// front), with the design chosen behind it and the materials of the cover in their places

#pragma once

#include <QFrame>
#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace kalahari {
namespace gui {

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

/// @brief A material on the plan: its text or picture, what it is and its state
class CoverPlanCard : public QFrame {
    Q_OBJECT

public:
    CoverPlanCard(const CoverPlanItem& item, bool narrow, QWidget* parent = nullptr);

    QString id() const { return m_id; }
    void setCurrent(bool current);
    bool isCurrent() const { return m_current; }

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

/// @brief The plan of the cover of the list of the cover (MaterialListPanel)
///
/// The back flap, the back, the spine, the front and the front flap side by side, with the
/// design chosen (or the one picked above the plan) behind the front, and the descriptions,
/// taglines and opinions in their places; under the plan the materials without a place. Only
/// what goes on a cover is there: the designs above the plan, not the reference pictures and
/// the notes. The flaps and the materials rejected are shown on request.
///
/// Every material is a card that takes the keys: the arrows go to the cards above, below and
/// beside it, Enter opens it as the list does, the menu key and Shift+F10 open its menu.
class CoverPlan : public QWidget {
    Q_OBJECT

public:
    explicit CoverPlan(QWidget* parent = nullptr);

    /// @brief Show @p items, keeping the material chosen when it is among them
    void setItems(const QList<CoverPlanItem>& items);

    /// @brief The material chosen; empty: none
    QString current() const { return m_current; }

    /// @brief Choose @p id on the plan, without currentChanged()
    void setCurrent(const QString& id);

    /// @brief Give the keys to the material chosen, else to the first one
    void focusCurrent();

    /// @brief Whether the plan has a card of @p id
    bool hasCard(const QString& id) const { return m_cards.contains(id); }

    /// @brief The place whose column shows @p id; empty: under the plan, without a place
    QString columnOf(const QString& id) const { return m_columnOf.value(id); }

    /// @brief The design behind the plan; empty: none
    QString designShown() const { return m_design; }

    /// @brief The box that shows the flaps
    QCheckBox* flapsBox() const { return m_flaps; }

    /// @brief The box that shows the materials rejected
    QCheckBox* rejectedBox() const { return m_rejected; }

signals:
    /// @brief The writer chose @p id on the plan
    void currentChanged(const QString& id);

    /// @brief Enter or a double-click on @p id
    void activated(const QString& id);

    /// @brief The menu of @p id (the right mouse button, the menu key, Shift+F10)
    void menuRequested(const QString& id, const QPoint& globalPos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void rebuild();
    void choose(const QString& id, bool focus);
    /// @brief The card beside @p card in @p direction (Qt::Key_Left...); nullptr: none
    CoverPlanCard* neighbour(const CoverPlanCard* card, int key) const;

    QCheckBox* m_flaps = nullptr;
    QCheckBox* m_rejected = nullptr;
    QLabel* m_designsLabel = nullptr;
    QHBoxLayout* m_designsLayout = nullptr;
    QButtonGroup* m_designButtons = nullptr;
    QWidget* m_sheet = nullptr;
    QHBoxLayout* m_sheetLayout = nullptr;
    QLabel* m_unplacedLabel = nullptr;
    QWidget* m_unplaced = nullptr;
    FlowLayout* m_unplacedLayout = nullptr;

    QList<CoverPlanItem> m_items;
    QString m_current;
    QString m_design;        ///< The design shown behind the plan
    bool m_designPicked = false;  ///< The writer picked the design above the plan
    QHash<QString, CoverPlanCard*> m_cards;
    QHash<QString, QString> m_columnOf;
    QList<QList<CoverPlanCard*>> m_columns;  ///< The cards of the columns shown, then those
                                            ///< without a place, in the order of the keys
};

} // namespace gui
} // namespace kalahari
