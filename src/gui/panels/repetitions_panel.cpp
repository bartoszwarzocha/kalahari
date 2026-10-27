/// @file repetitions_panel.cpp
/// @brief The panel Repetitions

#include "kalahari/gui/panels/repetitions_panel.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTextLayout>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <functional>
#include <memory>

namespace kalahari::gui {

namespace {

/// @brief The data of a card in the list
constexpr int WORD_ROLE = Qt::UserRole;
constexpr int DETAIL_ROLE = Qt::UserRole + 1;
constexpr int CONTEXT_ROLE = Qt::UserRole + 2;
constexpr int MARKS_ROLE = Qt::UserRole + 3;  ///< QVariantList of QPoint(start, length)

/// @brief Space around a card, inside it, and its bar in the color of the repeated words
constexpr int CARD_MARGIN = 4;
constexpr int CARD_PADDING = 7;
constexpr int CARD_BAR = 3;
constexpr int CARD_GAP = 4;       ///< Between the lines of the card and its buttons
constexpr double CARD_RADIUS = 5.0;

/// @brief How opaque the background of the repeated words of a card is
constexpr float MARK_ALPHA = 0.35F;

/// @brief The words of the list of the most frequent ones
enum WordColumn { WORD_COLUMN = 0, COUNT_COLUMN = 1, SHARE_COLUMN = 2 };

/// @brief Lay out a text in lines of a width
/// @return Its height
double layOut(QTextLayout& layout, double width) {
    double height = 0.0;
    layout.beginLayout();
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(width);
        line.setPosition(QPointF(0.0, height));
        height += line.height();
    }
    layout.endLayout();
    return height;
}

}  // namespace

/// @brief Paints the cards of the words too close: the word with the line about it, the text
///        around its words with them marked, and the buttons Go To and Skip This Word
class RepetitionCardDelegate : public QStyledItemDelegate {
public:
    /// @brief A button of a card was clicked: its row, and 0 for Go To, 1 for Skip This Word
    using Clicked = std::function<void(int row, int button)>;

    RepetitionCardDelegate(QListWidget* view, Clicked clicked)
        : QStyledItemDelegate(view), m_view(view), m_clicked(std::move(clicked)) {}

    void setButtonTexts(const QString& goTo, const QString& skip) { m_buttons = {goTo, skip}; }
    void setMarkColor(const QColor& color) { m_markColor = color; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        const Card card = cardOf(option, index);
        const QRectF frame = QRectF(option.rect).adjusted(CARD_MARGIN, CARD_MARGIN / 2.0,
                                                          -CARD_MARGIN, -CARD_MARGIN / 2.0);
        const bool selected = (option.state & QStyle::State_Selected) != 0;
        const QPalette& palette = option.palette;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QPainterPath shape;
        shape.addRoundedRect(frame.adjusted(0.5, 0.5, -0.5, -0.5), CARD_RADIUS, CARD_RADIUS);
        painter->fillPath(shape, palette.color(QPalette::Base));
        // The bar in the color of the repeated words, along the left edge
        painter->save();
        painter->setClipPath(shape);
        painter->fillRect(QRectF(frame.left(), frame.top(), CARD_BAR, frame.height()),
                          m_markColor);
        painter->restore();
        const bool keys = m_view->hasFocus();
        painter->setPen(QPen(selected ? palette.color(QPalette::Highlight)
                                      : palette.color(QPalette::Mid),
                             selected && keys ? 2.0 : 1.0));
        painter->drawPath(shape);

        painter->setPen(palette.color(QPalette::Text));
        card.title->draw(painter, card.titleAt);
        card.context->draw(painter, card.contextAt);
        for (int i = 0; i < 2; ++i) {
            QStyleOptionButton button;
            button.initFrom(m_view);
            button.state = QStyle::State_Enabled | QStyle::State_Raised;
            button.rect = card.buttons.at(i);
            button.text = m_buttons.at(i);
            m_view->style()->drawControl(QStyle::CE_PushButton, &button, painter, m_view);
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem atWidth = option;
        atWidth.rect = QRect(0, 0, m_view->viewport()->width(), 0);
        return QSize(atWidth.rect.width(), cardOf(atWidth, index).height);
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override {
        if (event->type() == QEvent::MouseButtonRelease) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                const Card card = cardOf(option, index);
                for (int i = 0; i < 2; ++i) {
                    if (card.buttons.at(i).contains(mouse->position().toPoint())) {
                        m_clicked(index.row(), i);
                        return true;
                    }
                }
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

private:
    /// @brief A card laid out in a rect
    struct Card {
        std::shared_ptr<QTextLayout> title;
        QPointF titleAt;
        std::shared_ptr<QTextLayout> context;
        QPointF contextAt;
        std::array<QRect, 2> buttons;
        int height = 0;
    };

    Card cardOf(const QStyleOptionViewItem& option, const QModelIndex& index) const {
        Card card;
        const QRect frame = option.rect.adjusted(CARD_MARGIN, CARD_MARGIN / 2, -CARD_MARGIN,
                                                 -CARD_MARGIN / 2);
        const int left = frame.left() + CARD_BAR + CARD_PADDING;
        const double width = std::max(frame.right() - CARD_PADDING - left, 40);
        const QFont font = option.font;
        QFont bold = font;
        bold.setBold(true);

        // The word, bold, and the line about it, muted
        const QString word = index.data(WORD_ROLE).toString();
        const QString detail = index.data(DETAIL_ROLE).toString();
        card.title = std::make_shared<QTextLayout>(word + QStringLiteral("  ") + detail, font);
        QTextCharFormat wordFormat;
        wordFormat.setFont(bold);
        QTextCharFormat detailFormat;
        detailFormat.setForeground(option.palette.color(QPalette::PlaceholderText));
        card.title->setFormats(
            {{0, static_cast<int>(word.size()), wordFormat},
             {static_cast<int>(word.size()) + 2, static_cast<int>(detail.size()), detailFormat}});
        double y = frame.top() + CARD_PADDING;
        card.titleAt = QPointF(left, y);
        y += layOut(*card.title, width) + CARD_GAP;

        // The text around the words, with them marked
        card.context = std::make_shared<QTextLayout>(index.data(CONTEXT_ROLE).toString(), font);
        QTextCharFormat markFormat;
        QColor background = m_markColor;
        background.setAlphaF(MARK_ALPHA);
        markFormat.setBackground(background);
        QList<QTextLayout::FormatRange> marks;
        for (const QVariant& mark : index.data(MARKS_ROLE).toList()) {
            const QPoint range = mark.toPoint();
            marks.append({range.x(), range.y(), markFormat});
        }
        card.context->setFormats(marks);
        card.contextAt = QPointF(left, y);
        y += layOut(*card.context, width) + CARD_GAP;

        // The buttons in a row, the second under the first when the card is narrow
        const QFontMetrics metrics(font);
        int x = left;
        int rowHeight = 0;
        for (int i = 0; i < 2; ++i) {
            QStyleOptionButton button;
            button.initFrom(m_view);
            button.text = m_buttons.at(i);
            const QSize size = m_view->style()->sizeFromContents(
                QStyle::CT_PushButton, &button,
                QSize(metrics.horizontalAdvance(button.text), metrics.height()), m_view);
            if (x > left && x + size.width() > left + width) {
                x = left;
                y += rowHeight + CARD_GAP;
                rowHeight = 0;
            }
            card.buttons.at(i) = QRect(QPoint(x, static_cast<int>(y)), size);
            x += size.width() + CARD_GAP;
            rowHeight = std::max(rowHeight, size.height());
        }
        y += rowHeight + CARD_PADDING;
        card.height = static_cast<int>(std::ceil(y)) - frame.top() + CARD_MARGIN;
        return card;
    }

    QListWidget* m_view;
    Clicked m_clicked;
    std::array<QString, 2> m_buttons;
    QColor m_markColor{230, 130, 40};
};

RepetitionsPanel::RepetitionsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("repetitionsPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // The scope, and Recount for the whole book
    auto* scopeRow = new QHBoxLayout();
    scopeRow->setSpacing(0);
    m_scopeGroup = new QButtonGroup(this);
    m_scopeGroup->setExclusive(true);
    auto* chapter = new QToolButton(this);
    //: The scope of the panel Repetitions: the text in front. In Polish: Rozdział
    chapter->setText(tr("Chapter"));
    m_bookButton = new QToolButton(this);
    //: The scope of the panel Repetitions: all the texts of the book. In Polish: Cała książka
    m_bookButton->setText(tr("Whole Book"));
    for (QToolButton* button : {chapter, m_bookButton}) {
        button->setCheckable(true);
        button->setAutoRaise(false);
        scopeRow->addWidget(button);
    }
    chapter->setObjectName(QStringLiteral("repetitionsChapterScope"));
    m_bookButton->setObjectName(QStringLiteral("repetitionsBookScope"));
    m_scopeGroup->addButton(chapter, static_cast<int>(Scope::Chapter));
    m_scopeGroup->addButton(m_bookButton, static_cast<int>(Scope::Book));
    chapter->setChecked(true);
    scopeRow->addSpacing(6);
    //: Counts the repetitions of the whole book again. In Polish: Przelicz
    m_recount = new QPushButton(tr("Recount"), this);
    m_recount->setObjectName(QStringLiteral("repetitionsRecount"));
    m_recount->setVisible(false);
    scopeRow->addWidget(m_recount);
    scopeRow->addStretch();
    layout->addLayout(scopeRow);
    connect(m_scopeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        const auto scope = static_cast<Scope>(id);
        if (scope != m_scope) {
            m_scope = scope;
            updateParts();
            emit scopeChanged(scope);
        }
    });
    connect(m_recount, &QPushButton::clicked, this, &RepetitionsPanel::recountRequested);

    // The message in place of the lists, or the pages
    m_pages = new QStackedWidget(this);
    m_messageLabel = new QLabel(m_pages);
    m_messageLabel->setObjectName(QStringLiteral("repetitionsMessage"));
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_messageLabel->setTextFormat(Qt::PlainText);
    m_pages->addWidget(m_messageLabel);
    m_tabs = new QTabWidget(m_pages);
    m_tabs->setDocumentMode(true);
    m_pages->addWidget(m_tabs);
    layout->addWidget(m_pages, 1);

    const auto makePage = [this](QLabel*& summary, QStackedWidget*& stack, QLabel*& empty,
                             QWidget* list) {
        auto* widget = new QWidget(m_tabs);
        auto* column = new QVBoxLayout(widget);
        column->setContentsMargins(0, 6, 0, 0);
        column->setSpacing(4);
        summary = new QLabel(widget);
        summary->setWordWrap(true);
        summary->setTextFormat(Qt::PlainText);
        column->addWidget(summary);
        stack = new QStackedWidget(widget);
        empty = new QLabel(stack);
        empty->setWordWrap(true);
        empty->setTextFormat(Qt::PlainText);
        empty->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        stack->addWidget(empty);
        list->setParent(stack);
        stack->addWidget(list);
        column->addWidget(stack, 1);
        return widget;
    };

    // The cards of the words too close
    m_cardList = new QListWidget();
    m_cardList->setObjectName(QStringLiteral("repetitionsCards"));
    m_cardList->setAccessibleName(tr("Words close together"));
    m_cardList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_cardList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_cardList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cardList->setResizeMode(QListView::Adjust);
    m_cardList->setUniformItemSizes(false);
    m_cardList->setFrameShape(QFrame::NoFrame);
    m_cardList->viewport()->setBackgroundRole(QPalette::Window);
    m_cardList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_cardDelegate = new RepetitionCardDelegate(m_cardList, [this](int row, int button) {
        m_cardList->setCurrentRow(row);
        if (button == 0) {
            emit cardActivated(row);
        } else {
            emit skipWordRequested(row);
        }
    });
    //: The button of a card of the panel Repetitions: the cursor to its words. In Polish: Przejdź
    const QString goTo = tr("Go To");
    //: The button of a card of the panel Repetitions: the word is not counted any more. In
    //: Polish: Pomijaj to słowo
    const QString skip = tr("Skip This Word");
    m_cardDelegate->setButtonTexts(goTo, skip);
    m_cardList->setItemDelegate(m_cardDelegate);
    //: The page of the panel Repetitions with the words that come back too close. In Polish:
    //: Blisko siebie
    m_tabs->addTab(makePage(m_closeSummaryLabel, m_closeStack, m_closeEmpty, m_cardList),
                   tr("Close Together"));

    // The most frequent words
    m_wordList = new QTreeWidget();
    m_wordList->setObjectName(QStringLiteral("repetitionsWords"));
    m_wordList->setAccessibleName(tr("Most frequent words"));
    m_wordList->setColumnCount(3);
    //: The columns of the most frequent words: the word, how many times, times in a thousand
    //: words. In Polish: Słowo, Razy, Na 1000
    m_wordList->setHeaderLabels({tr("Word"), tr("Times"), tr("Per 1000")});
    m_wordList->setRootIsDecorated(false);
    m_wordList->setUniformRowHeights(false);
    m_wordList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_wordList->setFrameShape(QFrame::NoFrame);
    m_wordList->header()->setStretchLastSection(false);
    m_wordList->header()->setSectionResizeMode(WORD_COLUMN, QHeaderView::Stretch);
    m_wordList->header()->setSectionResizeMode(COUNT_COLUMN, QHeaderView::ResizeToContents);
    m_wordList->header()->setSectionResizeMode(SHARE_COLUMN, QHeaderView::ResizeToContents);
    //: The page of the panel Repetitions with the words used most often. In Polish: Najczęstsze
    m_tabs->addTab(makePage(m_frequentSummaryLabel, m_frequentStack, m_frequentEmpty, m_wordList),
                   tr("Most Frequent"));

    connect(m_tabs, &QTabWidget::currentChanged, this, [this]() {
        if (!m_updating) {
            emit pageChanged(page());
        }
    });
    connect(m_cardList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_updating && row >= 0) {
            emit cardSelected(row);
        }
    });
    connect(m_cardList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) { emit cardActivated(m_cardList->row(item)); });
    connect(m_cardList, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (QListWidgetItem* item = m_cardList->itemAt(pos)) {
            m_cardList->setCurrentItem(item);
        }
        showCardMenu(m_cardList->viewport()->mapToGlobal(pos));
    });
    connect(m_wordList, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (!m_updating && item != nullptr) {
            emit wordSelected(m_wordList->indexOfTopLevelItem(item));
        }
    });
    connect(m_wordList, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        emit wordActivated(m_wordList->indexOfTopLevelItem(item));
    });
    m_cardList->installEventFilter(this);
    m_wordList->installEventFilter(this);

    updateParts();
}

void RepetitionsPanel::setScope(Scope scope) {
    m_scope = scope;
    m_scopeGroup->button(static_cast<int>(scope))->setChecked(true);
    updateParts();
}

void RepetitionsPanel::setBookAvailable(bool available) {
    m_bookButton->setEnabled(available);
    m_bookButton->setToolTip(available ? QString() : tr("No book is open."));
}

void RepetitionsPanel::setPage(Page page) {
    m_updating = true;
    m_tabs->setCurrentIndex(page == Page::Close ? 0 : 1);
    m_updating = false;
}

RepetitionsPanel::Page RepetitionsPanel::page() const {
    return m_tabs->currentIndex() == 1 ? Page::Frequent : Page::Close;
}

void RepetitionsPanel::setResults(const QList<RepetitionCard>& cards,
                                  const QList<FrequentWord>& words, const QString& closeSummary,
                                  const QString& frequentSummary) {
    m_updating = true;
    const int card = std::max(currentCard(), 0);
    const int word = std::max(currentWord(), 0);
    m_cards = cards;
    m_words = words;
    m_closeSummary = closeSummary;
    m_frequentSummary = frequentSummary;

    m_cardList->clear();
    for (const RepetitionCard& repetition : cards) {
        auto* item = new QListWidgetItem(m_cardList);
        item->setData(WORD_ROLE, repetition.word);
        item->setData(DETAIL_ROLE, repetition.detail);
        item->setData(CONTEXT_ROLE, repetition.context);
        QVariantList marks;
        for (const auto& [start, length] : repetition.marks) {
            marks.append(QPoint(start, length));
        }
        item->setData(MARKS_ROLE, marks);
        // What a screen reader says of the card
        item->setData(Qt::AccessibleTextRole,
                      repetition.word + QStringLiteral(", ") + repetition.detail +
                          QStringLiteral(": ") + repetition.context);
    }
    if (!cards.isEmpty()) {
        m_cardList->setCurrentRow(std::min(card, static_cast<int>(cards.size()) - 1));
    }

    m_wordList->clear();
    const QLocale locale;
    for (const FrequentWord& frequent : words) {
        auto* item = new QTreeWidgetItem(m_wordList);
        item->setText(WORD_COLUMN, frequent.forms.isEmpty()
                                       ? frequent.word
                                       : frequent.word + QLatin1Char('\n') + frequent.forms);
        item->setText(COUNT_COLUMN, locale.toString(frequent.count));
        item->setText(SHARE_COLUMN, locale.toString(frequent.perThousand, 'f', 1));
        item->setTextAlignment(COUNT_COLUMN, Qt::AlignRight | Qt::AlignTop);
        item->setTextAlignment(SHARE_COLUMN, Qt::AlignRight | Qt::AlignTop);
        item->setToolTip(WORD_COLUMN, frequent.forms);
    }
    if (!words.isEmpty()) {
        m_wordList->setCurrentItem(
            m_wordList->topLevelItem(std::min(word, static_cast<int>(words.size()) - 1)));
    }
    m_updating = false;
    updateParts();
}

void RepetitionsPanel::setMessage(const QString& message) {
    m_message = message;
    updateParts();
}

QString RepetitionsPanel::message() const {
    return m_message;
}

int RepetitionsPanel::currentCard() const {
    return m_cardList->currentRow();
}

void RepetitionsPanel::setCurrentCard(int index) {
    m_updating = true;
    m_cardList->setCurrentRow(index);
    if (QListWidgetItem* item = m_cardList->item(index)) {
        m_cardList->scrollToItem(item);
    }
    m_updating = false;
}

int RepetitionsPanel::currentWord() const {
    return m_wordList->currentItem() != nullptr
               ? m_wordList->indexOfTopLevelItem(m_wordList->currentItem())
               : -1;
}

void RepetitionsPanel::setCurrentWord(int index) {
    m_updating = true;
    if (QTreeWidgetItem* item = m_wordList->topLevelItem(index)) {
        m_wordList->setCurrentItem(item);
        m_wordList->scrollToItem(item);
    }
    m_updating = false;
}

void RepetitionsPanel::setMarkColor(const QColor& color) {
    m_markColor = color;
    m_cardDelegate->setMarkColor(color);
    m_cardList->viewport()->update();
}

void RepetitionsPanel::focusList() {
    if (!m_message.isEmpty()) {
        setFocus(Qt::OtherFocusReason);
        return;
    }
    QWidget* list = page() == Page::Close ? static_cast<QWidget*>(m_cardList)
                                          : static_cast<QWidget*>(m_wordList);
    list->setFocus(Qt::OtherFocusReason);
}

bool RepetitionsPanel::hasFocusInside() const {
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && (focus == this || isAncestorOf(focus));
}

bool RepetitionsPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_cardList && watched != m_wordList) {
        return QWidget::eventFilter(watched, event);
    }
    const bool cards = watched == m_cardList;
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        const bool plain = key->modifiers() == Qt::NoModifier ||
                           key->modifiers() == Qt::KeypadModifier;
        const bool ours = plain && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter ||
                                    key->key() == Qt::Key_Escape ||
                                    (cards && key->key() == Qt::Key_Delete));
        if (!ours) {
            return QWidget::eventFilter(watched, event);
        }
        // The panel's keys, before the window's shortcuts (Edit > Delete has Delete)
        if (event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            emit editorFocusRequested();
        } else if (cards && currentCard() >= 0) {
            if (key->key() == Qt::Key_Delete) {
                emit skipWordRequested(currentCard());
            } else {
                emit cardActivated(currentCard());
            }
        } else if (!cards && currentWord() >= 0) {
            emit wordActivated(currentWord());
        }
        return true;
    }
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
        // The frame of the card chosen shows whether the keys are in the list
        m_cardList->viewport()->update();
    }
    return QWidget::eventFilter(watched, event);
}

void RepetitionsPanel::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        updateParts();
    }
}

void RepetitionsPanel::updateParts() {
    m_recount->setVisible(m_scope == Scope::Book);
    m_pages->setCurrentWidget(m_message.isEmpty() ? static_cast<QWidget*>(m_tabs)
                                                  : static_cast<QWidget*>(m_messageLabel));
    m_messageLabel->setText(m_message);

    m_closeSummaryLabel->setText(m_closeSummary);
    m_closeSummaryLabel->setVisible(!m_closeSummary.isEmpty());
    m_frequentSummaryLabel->setText(m_frequentSummary);
    m_frequentSummaryLabel->setVisible(!m_frequentSummary.isEmpty());
    m_closeEmpty->setText(tr("No word comes back too close."));
    m_frequentEmpty->setText(tr("No words are counted."));
    m_closeStack->setCurrentWidget(m_cards.isEmpty() ? static_cast<QWidget*>(m_closeEmpty)
                                                     : static_cast<QWidget*>(m_cardList));
    m_frequentStack->setCurrentWidget(m_words.isEmpty() ? static_cast<QWidget*>(m_frequentEmpty)
                                                        : static_cast<QWidget*>(m_wordList));
}

void RepetitionsPanel::showCardMenu(const QPoint& globalPos) {
    const int card = currentCard();
    if (card < 0) {
        return;
    }
    QMenu menu(this);
    //: The menu of a card of the panel Repetitions. In Polish: Przejdź
    QAction* goTo = menu.addAction(tr("Go To"));
    QAction* skip = menu.addAction(tr("Skip This Word"));
    goTo->setShortcut(QKeySequence(Qt::Key_Return));
    skip->setShortcut(QKeySequence(Qt::Key_Delete));
    // From the keys: at the card chosen
    QPoint at = globalPos;
    if (!m_cardList->viewport()->rect().contains(m_cardList->viewport()->mapFromGlobal(at))) {
        at = m_cardList->viewport()->mapToGlobal(
            m_cardList->visualItemRect(m_cardList->item(card)).center());
    }
    QAction* chosen = menu.exec(at);
    if (chosen == goTo) {
        emit cardActivated(card);
    } else if (chosen == skip) {
        emit skipWordRequested(card);
    }
}

}  // namespace kalahari::gui
