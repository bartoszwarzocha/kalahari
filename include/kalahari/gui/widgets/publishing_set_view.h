// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_set_view.h
/// @brief The set for a publisher as a page: the book's title, then its parts one under
///        another, each with its version, its length, Copy and Open, and its text

#ifndef KALAHARI_GUI_WIDGETS_PUBLISHING_SET_VIEW_H
#define KALAHARI_GUI_WIDGETS_PUBLISHING_SET_VIEW_H

#include <QFrame>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QKeyEvent;
class QLabel;
class QToolButton;
class QVBoxLayout;

namespace kalahari::gui {

/// @brief A version of a part of the set, as the list of its versions shows it
struct PublishingSetVersion {
    QString id;
    QString title;  ///< Its title with what marks it: "Sentence – version 1 ★"
};

/// @brief A part of the set as the page shows it
struct PublishingSetItem {
    QString part;       ///< core::PublishingTexts::setParts()
    QString name;       ///< Its name: "Sentence about the book"
    QString elementId;  ///< The version in the set; empty: the book has none
    bool on = true;     ///< Whether it is in the set
    bool fromCover = false;  ///< A description or a tagline, which the cover has
    bool italic = false;     ///< Its text in italics: a sentence, a tagline
    QList<PublishingSetVersion> versions;  ///< What can be its version, in their order
    QString text;       ///< Its text; empty: none yet
    QString length;     ///< Its length with its target: "165 / 200 ch."
};

/// @brief A part of the set on its page: its name, its version, its length, Copy, Open and
/// its text; one left out of the set is pale
class PublishingSetCard : public QFrame {
    Q_OBJECT

public:
    explicit PublishingSetCard(const PublishingSetItem& item, QWidget* parent = nullptr);

    /// @brief The part it shows
    [[nodiscard]] QString part() const { return m_part; }

    /// @brief Its versions; disabled while it has fewer than two
    [[nodiscard]] QComboBox* versionBox() const { return m_versions; }

    /// @brief Copy: its text to the clipboard
    [[nodiscard]] QToolButton* copyButton() const { return m_copy; }

    /// @brief Open: its version where it is written
    [[nodiscard]] QToolButton* openButton() const { return m_open; }

    /// @brief Its text as shown
    [[nodiscard]] QLabel* textLabel() const { return m_text; }

    /// @brief Show it as the part chosen
    void setCurrent(bool current);

    /// @brief Whether it is the part chosen
    [[nodiscard]] bool isCurrent() const { return m_current; }

signals:
    void clicked(const QString& part);
    void doubleClicked(const QString& part);
    void menuRequested(const QString& part, const QPoint& globalPos);
    /// @brief The writer chose @p elementId in the list of its versions
    void versionChosen(const QString& part, const QString& elementId);
    void copyRequested(const QString& part);
    void openRequested(const QString& part);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    /// @brief The row of its name, the cover's mark, its versions and its length
    QWidget* createHeading(const PublishingSetItem& item);
    /// @brief The row of Copy and Open
    QWidget* createButtons(const PublishingSetItem& item);

    QString m_part;
    bool m_on = true;
    bool m_current = false;
    QComboBox* m_versions = nullptr;
    QToolButton* m_copy = nullptr;
    QToolButton* m_open = nullptr;
    QLabel* m_text = nullptr;
};

/// @brief The set for a publisher of the list of the texts for publishers (MaterialListPanel)
///
/// The book's title and what it is, then each part of the set in its order: those left out are
/// pale. A part is a card that takes the keys: Up and Down go to the parts above and below it,
/// Enter opens its version, Space includes it in the set or leaves it out, the menu key and
/// Shift+F10 open its menu; Tab goes on to its versions, Copy and Open.
class PublishingSetView : public QWidget {
    Q_OBJECT

public:
    explicit PublishingSetView(QWidget* parent = nullptr);

    /// @brief Show the book's @p title and @p about (its author, its type, its length)
    void setHeader(const QString& title, const QString& about);

    /// @brief Show @p items, keeping the part chosen when it is among them
    void setItems(const QList<PublishingSetItem>& items);

    /// @brief The part chosen; empty: none
    [[nodiscard]] QString current() const { return m_current; }

    /// @brief Choose @p part, without currentChanged()
    void setCurrent(const QString& part);

    /// @brief Give the keys to the part chosen, else to the first one
    void focusCurrent();

    /// @brief The card of @p part; nullptr: none
    [[nodiscard]] PublishingSetCard* card(const QString& part) const {
        return m_cards.value(part);
    }

    /// @brief The parts shown, in their order
    [[nodiscard]] QStringList parts() const { return m_order; }

    /// @brief The title of the book
    [[nodiscard]] QLabel* titleLabel() const { return m_title; }

signals:
    /// @brief The writer chose @p part
    void currentChanged(const QString& part);
    /// @brief Enter, a double-click or Open on @p part
    void activated(const QString& part);
    /// @brief Space on @p part: include it in the set or leave it out
    void toggled(const QString& part);
    /// @brief The writer chose version @p elementId of @p part
    void versionChosen(const QString& part, const QString& elementId);
    /// @brief Copy of @p part
    void copyRequested(const QString& part);
    /// @brief The menu of @p part (the right mouse button, the menu key, Shift+F10)
    void menuRequested(const QString& part, const QPoint& globalPos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Choose @p part, giving it the keys if @p focus, with currentChanged()
    void choose(const QString& part, bool focus);
    /// @brief What a key pressed on card @p card does; false: nothing
    bool cardKey(const PublishingSetCard* card, const QKeyEvent& key);
    void connectCard(PublishingSetCard* card);

    QFrame* m_page = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_about = nullptr;
    QVBoxLayout* m_partsLayout = nullptr;
    QHash<QString, PublishingSetCard*> m_cards;
    QStringList m_order;
    QString m_current;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_PUBLISHING_SET_VIEW_H
