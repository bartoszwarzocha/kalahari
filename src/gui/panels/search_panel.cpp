/// @file search_panel.cpp
/// @brief The Search panel: Find in Book, the matches in all the texts of the book grouped by
///        text, and Replace All in Book

#include "kalahari/gui/panels/search_panel.h"

#include "kalahari/editor/find_replace_bar.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/wrapping_button.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace kalahari::gui {

namespace {

/// The index of the text of a top item, of the match of an item under it
constexpr int INDEX_ROLE = Qt::UserRole;
/// Of a match: its note ("footnote 2: "), the text around it, where it is there and its
/// length
constexpr int NOTE_ROLE = Qt::UserRole + 1;
constexpr int CONTEXT_ROLE = Qt::UserRole + 2;
constexpr int START_ROLE = Qt::UserRole + 3;
constexpr int LENGTH_ROLE = Qt::UserRole + 4;

/// The size of the buttons of the options, as on the find bar
constexpr int OPTION_WIDTH = 28;
constexpr int OPTION_HEIGHT = 24;
/// The narrowest the field of what to find gets before the options go on in the next row
constexpr int FIELD_MINIMUM_WIDTH = 120;
/// Between the groups of the first row
constexpr int GROUP_SPACING = 6;

}  // namespace

// =============================================================================
// The matches drawn with their text around them
// =============================================================================

/// @brief Draws a match of the tree: its note in a softer color, the text around it, the
///        match itself bold on the color of the search's hits
class SearchPanel::HitDelegate : public QStyledItemDelegate {
public:
    explicit HitDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

    void setMarkColor(const QColor& color) { m_mark = color; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        if (!index.parent().isValid()) {
            QStyledItemDelegate::paint(painter, option, index);  // a text
            return;
        }
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        item.text.clear();
        const QWidget* widget = item.widget;
        QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

        const QRect rect =
            style->subElementRect(QStyle::SE_ItemViewItemText, &item, widget).adjusted(2, 0, -2, 0);
        const QString note = index.data(NOTE_ROLE).toString();
        const QString context = index.data(CONTEXT_ROLE).toString();
        const int start = index.data(START_ROLE).toInt();
        const int length = index.data(LENGTH_ROLE).toInt();
        const bool selected = (item.state & QStyle::State_Selected) != 0;
        const QColor text = item.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        const QColor soft =
            selected ? text : item.palette.color(QPalette::PlaceholderText);
        QFont bold = item.font;
        bold.setBold(true);

        painter->save();
        painter->setClipRect(rect);
        const QFontMetrics metrics(item.font);
        const int baseline = rect.top() + (rect.height() - metrics.height()) / 2 + metrics.ascent();
        int x = rect.left();
        // A part at a time, the last one cut with "..." where the row ends
        const auto draw = [&](const QString& part, const QFont& font, const QColor& color,
                              const QColor& background) {
            const int room = rect.right() - x;
            if (part.isEmpty() || room <= 0) {
                return room > 0;
            }
            const QFontMetrics fontMetrics(font);
            const QString shown = fontMetrics.elidedText(part, Qt::ElideRight, room);
            const int width = fontMetrics.horizontalAdvance(shown);
            if (background.isValid()) {
                painter->fillRect(QRect(x, rect.top() + 1, width, rect.height() - 2), background);
            }
            painter->setFont(font);
            painter->setPen(color);
            painter->drawText(x, baseline, shown);
            x += width;
            return shown == part;
        };
        if (draw(note, item.font, soft, QColor())) {
            if (draw(context.left(start), item.font, text, QColor()) &&
                draw(context.mid(start, length), bold, text, m_mark)) {
                draw(context.mid(start + length), item.font, text, QColor());
            }
        }
        painter->restore();
    }

private:
    QColor m_mark{255, 214, 0, 96};  ///< The default of the search's hits (setMarkColor())
};

// =============================================================================
// The panel
// =============================================================================

SearchPanel::SearchPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // What to find, and the options of the find bar beside it; in a narrow panel the options
    // go on in the next row
    auto* findRow = new QWidget(this);
    auto* findItems = new FlowLayout(findRow, GROUP_SPACING, GROUP_SPACING);
    findItems->setContentsMargins(0, 0, 0, 0);
    m_searchField = new QLineEdit(findRow);
    //: The field of what to find in the Search panel. In Polish: Znajdź w książce...
    m_searchField->setPlaceholderText(tr("Find in book..."));
    //: In Polish: Znajdź w książce
    m_searchField->setAccessibleName(tr("Find in Book"));
    m_searchField->setClearButtonEnabled(true);
    m_searchField->setMinimumWidth(FIELD_MINIMUM_WIDTH);
    m_searchField->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    findItems->addWidget(m_searchField);

    auto* options = new QWidget(findRow);
    auto* optionsLayout = new QHBoxLayout(options);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(4);
    const QList<QKeyCombination> keys = editor::FindReplaceBar::optionKeys();
    const auto keysOf = [&keys](qsizetype option) {
        return QKeySequence(keys.at(option)).toString(QKeySequence::NativeText);
    };
    const auto addOption = [&](const QString& text, const QString& toolTip) {
        auto* button = new QToolButton(options);
        button->setText(text);
        button->setToolTip(toolTip);
        button->setAccessibleName(toolTip);
        button->setCheckable(true);
        button->setFixedSize(OPTION_WIDTH, OPTION_HEIGHT);
        optionsLayout->addWidget(button);
        connect(button, &QToolButton::toggled, this, [this]() {
            if (!m_quiet) {
                searchNow();
            }
        });
        return button;
    };
    //: The options of the Search panel, as on the find bar. In Polish: Uwzględniaj wielkość
    //: liter (%1) / Tylko całe wyrazy (%1) / Wyrażenie regularne (%1)
    m_caseButton = addOption(tr("Aa"), tr("Match Case (%1)").arg(keysOf(0)));
    m_wordButton = addOption(tr("W"), tr("Match Whole Word (%1)").arg(keysOf(1)));
    m_regexButton = addOption(tr(".*"), tr("Use Regular Expression (%1)").arg(keysOf(2)));
    findItems->addWidget(options);
    layout->addWidget(findRow);

    // What the matches become, and Replace All in Book
    m_replaceField = new QLineEdit(this);
    //: The field of the replacement in the Search panel. In Polish: Zamień na...
    m_replaceField->setPlaceholderText(tr("Replace with..."));
    //: In Polish: Zamień na
    m_replaceField->setAccessibleName(tr("Replace With"));
    m_replaceField->setClearButtonEnabled(true);
    layout->addWidget(m_replaceField);
    auto* replaceRow = new QHBoxLayout();
    replaceRow->setContentsMargins(0, 0, 0, 0);
    replaceRow->addStretch(1);
    //: In Polish: Zamień wszystko w książce
    // Its text goes on in a second line where the panel is narrow
    m_replaceButton = new WrappingButton(tr("Replace All in Book"), this);
    m_replaceButton->setEnabled(false);
    replaceRow->addWidget(m_replaceButton);
    layout->addLayout(replaceRow);
    connect(m_replaceButton, &QPushButton::clicked, this, &SearchPanel::replaceAllRequested);

    // What was found, and the matches
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_statusLabel);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(false);  // a double click goes to the match
    m_tree->setTextElideMode(Qt::ElideRight);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    //: The tree of the matches of the Search panel. In Polish: Znalezione trafienia
    m_tree->setAccessibleName(tr("Matches Found"));
    m_tree->setMinimumWidth(0);
    m_delegate = new HitDelegate(m_tree);
    m_tree->setItemDelegate(m_delegate);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (const std::optional<HitIndex> hit = hitOf(item); hit && !m_quiet) {
            emit hitChosen(hit->text, hit->hit);
        }
    });
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        if (const std::optional<HitIndex> hit = hitOf(item)) {
            emit hitActivated(hit->text, hit->hit, false);
        }
    });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        if (const std::optional<HitIndex> hit = hitOf(item)) {
            emit hitActivated(hit->text, hit->hit, true);
        } else if (item != nullptr) {
            item->setExpanded(!item->isExpanded());
        }
    });

    // The search a moment after the typing stops
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(SEARCH_DELAY_MS);
    connect(m_searchTimer, &QTimer::timeout, this, &SearchPanel::searchRequested);
    connect(m_searchField, &QLineEdit::textChanged, this, [this]() {
        if (!m_quiet) {
            m_searchTimer->start();
        }
    });

    // Tab: the field, the options, the replacement, its button and the tree
    setTabOrder(m_searchField, m_caseButton);
    setTabOrder(m_caseButton, m_wordButton);
    setTabOrder(m_wordButton, m_regexButton);
    setTabOrder(m_regexButton, m_replaceField);
    setTabOrder(m_replaceField, m_replaceButton);
    setTabOrder(m_replaceButton, m_tree);

    for (QWidget* widget : {static_cast<QWidget*>(m_searchField),
                            static_cast<QWidget*>(m_caseButton), static_cast<QWidget*>(m_wordButton),
                            static_cast<QWidget*>(m_regexButton),
                            static_cast<QWidget*>(m_replaceField),
                            static_cast<QWidget*>(m_replaceButton), static_cast<QWidget*>(m_tree)}) {
        widget->installEventFilter(this);
    }
    setFocusProxy(m_searchField);
}

SearchPanel::~SearchPanel() = default;

QString SearchPanel::searchText() const {
    return m_searchField->text();
}

void SearchPanel::setSearchText(const QString& text) {
    m_quiet = true;
    m_searchField->setText(text);
    m_quiet = false;
    m_searchTimer->stop();
}

QString SearchPanel::replaceText() const {
    return m_replaceField->text();
}

editor::SearchOptions SearchPanel::options() const {
    editor::SearchOptions options;
    options.caseSensitive = m_caseButton->isChecked();
    options.wholeWord = m_wordButton->isChecked();
    options.useRegex = m_regexButton->isChecked();
    return options;
}

void SearchPanel::setOptions(const editor::SearchOptions& options) {
    m_quiet = true;
    m_caseButton->setChecked(options.caseSensitive);
    m_wordButton->setChecked(options.wholeWord);
    m_regexButton->setChecked(options.useRegex);
    m_quiet = false;
}

void SearchPanel::focusSearchField() {
    m_searchField->setFocus(Qt::ShortcutFocusReason);
    m_searchField->selectAll();
}

void SearchPanel::setStatus(const QString& status) {
    m_statusLabel->setText(status);
}

QString SearchPanel::status() const {
    return m_statusLabel->text();
}

void SearchPanel::setTexts(const QList<SearchPanelText>& texts) {
    // What is chosen and what is folded, by the keys of the texts
    QString chosenKey;
    int chosenHit = -1;
    if (const QTreeWidgetItem* item = m_tree->currentItem()) {
        const QTreeWidgetItem* top = item->parent() != nullptr ? item->parent() : item;
        const int text = top->data(0, INDEX_ROLE).toInt();
        if (text >= 0 && text < m_texts.size()) {
            chosenKey = m_texts.at(text).key;
            chosenHit = item->parent() != nullptr ? item->data(0, INDEX_ROLE).toInt() : -1;
        }
    }
    QSet<QString> folded;
    for (int i = 0; i < m_tree->topLevelItemCount() && i < m_texts.size(); ++i) {
        if (!m_tree->topLevelItem(i)->isExpanded()) {
            folded.insert(m_texts.at(i).key);
        }
    }

    m_quiet = true;
    m_tree->setUpdatesEnabled(false);
    m_tree->clear();
    m_texts = texts;
    QTreeWidgetItem* chosen = nullptr;
    QList<QTreeWidgetItem*> tops;
    tops.reserve(texts.size());
    for (int t = 0; t < texts.size(); ++t) {
        const SearchPanelText& text = texts.at(t);
        auto* top = new QTreeWidgetItem();
        //: A text of the book in the Search panel with the number of its matches. In Polish
        //: the same: %1 (%2)
        top->setText(0, tr("%1 (%2)").arg(text.title).arg(text.count));
        top->setData(0, INDEX_ROLE, t);
        QFont font = top->font(0);
        font.setBold(true);
        top->setFont(0, font);
        for (int h = 0; h < text.hits.size(); ++h) {
            const SearchPanelHit& hit = text.hits.at(h);
            auto* item = new QTreeWidgetItem(top);
            //: A match in the text of a note in the Search panel, before the text around it.
            //: In Polish the same: %1: (przypis dolny 2: )
            const QString note = hit.note.isEmpty() ? QString() : tr("%1: ").arg(hit.note);
            item->setText(0, note + hit.context);
            item->setData(0, INDEX_ROLE, h);
            item->setData(0, NOTE_ROLE, note);
            item->setData(0, CONTEXT_ROLE, hit.context);
            item->setData(0, START_ROLE, hit.start);
            item->setData(0, LENGTH_ROLE, hit.length);
        }
        if (text.key == chosenKey && !chosenKey.isEmpty()) {
            chosen = chosenHit >= 0 && chosenHit < top->childCount() ? top->child(chosenHit) : top;
        }
        tops.append(top);
    }
    m_tree->addTopLevelItems(tops);
    for (int t = 0; t < tops.size(); ++t) {
        tops.at(t)->setExpanded(!folded.contains(texts.at(t).key));
    }
    if (chosen != nullptr) {
        m_tree->setCurrentItem(chosen);
    }
    m_tree->setUpdatesEnabled(true);
    m_quiet = false;
}

std::optional<SearchPanel::HitIndex> SearchPanel::currentHit() const {
    return hitOf(m_tree->currentItem());
}

void SearchPanel::setCurrentHit(const HitIndex& hit) {
    if (QTreeWidgetItem* item = itemOf(hit)) {
        m_quiet = true;
        m_tree->setCurrentItem(item);
        m_tree->scrollToItem(item);
        m_quiet = false;
    }
}

void SearchPanel::setReplaceEnabled(bool enabled, const QString& whyNot) {
    m_replaceButton->setEnabled(enabled);
    //: The tooltip of Replace All in Book. In Polish: Zamień wszystkie trafienia we wszystkich
    //: tekstach książki
    m_replaceButton->setToolTip(enabled || whyNot.isEmpty()
                                    ? tr("Replace all the matches in all the texts of the book")
                                    : whyNot);
}

void SearchPanel::setMarkColor(const QColor& color) {
    m_delegate->setMarkColor(color);
    m_tree->viewport()->update();
}

std::optional<SearchPanel::HitIndex> SearchPanel::hitOf(const QTreeWidgetItem* item) const {
    if (item == nullptr || item->parent() == nullptr) {
        return std::nullopt;
    }
    return HitIndex{item->parent()->data(0, INDEX_ROLE).toInt(), item->data(0, INDEX_ROLE).toInt()};
}

QTreeWidgetItem* SearchPanel::itemOf(const HitIndex& hit) const {
    QTreeWidgetItem* top =
        hit.text >= 0 && hit.text < m_tree->topLevelItemCount() ? m_tree->topLevelItem(hit.text)
                                                               : nullptr;
    return top != nullptr && hit.hit >= 0 && hit.hit < top->childCount() ? top->child(hit.hit)
                                                                         : nullptr;
}

void SearchPanel::searchNow() {
    m_searchTimer->stop();
    emit searchRequested();
}

bool SearchPanel::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    auto* key = static_cast<QKeyEvent*>(event);
    const bool press = event->type() == QEvent::KeyPress;

    // The options' keys, before the access keys of the menus with the same letters (Pomo&c
    // and &Widok in Polish), as on the find bar
    const qsizetype option = editor::FindReplaceBar::optionKeys().indexOf(key->keyCombination());
    if (option >= 0) {
        if (press && !key->isAutoRepeat()) {
            const QList<QToolButton*> buttons = {m_caseButton, m_wordButton, m_regexButton};
            buttons.at(option)->toggle();
        }
        key->accept();
        return true;
    }

    const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
    const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
    const bool escape = key->key() == Qt::Key_Escape;
    if (modifiers != Qt::NoModifier || (!enter && !escape && key->key() != Qt::Key_Down)) {
        return QWidget::eventFilter(watched, event);
    }
    if (key->key() == Qt::Key_Down && watched != m_searchField) {
        return QWidget::eventFilter(watched, event);
    }
    if (!press) {
        // The panel's keys, before the window's shortcuts
        key->accept();
        return true;
    }
    if (escape) {
        emit editorFocusRequested();
    } else if (key->key() == Qt::Key_Down) {
        // From the field to the first match
        if (QTreeWidgetItem* top = m_tree->topLevelItem(0)) {
            m_tree->setFocus(Qt::TabFocusReason);
            m_tree->setCurrentItem(top->childCount() > 0 ? top->child(0) : top);
        }
    } else if (watched == m_searchField) {
        searchNow();
    } else if (watched == m_replaceField || watched == m_replaceButton) {
        if (m_replaceButton->isEnabled() && !key->isAutoRepeat()) {
            m_replaceButton->click();
        }
    } else if (watched == m_tree) {
        QTreeWidgetItem* item = m_tree->currentItem();
        if (const std::optional<HitIndex> hit = hitOf(item)) {
            emit hitActivated(hit->text, hit->hit, true);
        } else if (item != nullptr) {
            item->setExpanded(!item->isExpanded());
        }
    } else if (auto* button = qobject_cast<QToolButton*>(watched); button != nullptr) {
        button->toggle();  // Enter as Space on an option
    }
    return true;
}

}  // namespace kalahari::gui
