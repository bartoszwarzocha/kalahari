// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file footnotes_panel.cpp
/// @brief The Footnotes panel: the footnotes and endnotes of the text in front or of the book

#include "kalahari/gui/panels/footnotes_panel.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace kalahari::gui {

namespace {

constexpr int PANEL_MARGIN = 6;   ///< Around the parts of the panel
constexpr int CARD_MARGIN = 4;    ///< Around a card in the list
constexpr int CARD_PADDING = 6;   ///< Inside a card
constexpr int BAR_WIDTH = 4;      ///< The bar of the color of the kind
constexpr int CARD_RADIUS = 4;    ///< The corners of a card
constexpr int SELECTED_BORDER = 2;
constexpr double SELECTED_TINT = 0.12;  ///< The highlight in the background of the selected card
constexpr int MAX_SCOPE_CHARS = 16;     ///< The name of the text in front on its button, at most

/// @brief The data of a row of the list
enum Role : int {
    HEADING_ROLE = Qt::UserRole + 1,  ///< bool: the title of a text, not a note
    NUMBER_ROLE,                      ///< QString
    KIND_ROLE,                        ///< int: editor::NoteKind
    BEFORE_ROLE,                      ///< QString
    TEXT_ROLE,                        ///< QString
};

/// @brief @p a mixed with @p b, @p share of @p b
QColor mixed(const QColor& a, const QColor& b, double share) {
    return QColor::fromRgbF(static_cast<float>(a.redF() + ((b.redF() - a.redF()) * share)),
                            static_cast<float>(a.greenF() + ((b.greenF() - a.greenF()) * share)),
                            static_cast<float>(a.blueF() + ((b.blueF() - a.blueF()) * share)));
}

/// @brief The name of a kind with a number: "Footnote 3", as in the text
QString titleOf(const FootnoteEntry& entry) {
    return editor::BookEditor::noteTitle(entry.kind, entry.number);
}

/// @brief The end of @p text as wide as @p width at most: whole words after "…" where they fit
QString lastWords(const QString& text, const QFontMetrics& metrics, int width) {
    if (metrics.horizontalAdvance(text) <= width) {
        return text;
    }
    const QString ellipsis(QChar(0x2026));
    qsizetype space = text.indexOf(u' ');
    while (space >= 0) {
        const QString shown = ellipsis + text.mid(space + 1);
        if (metrics.horizontalAdvance(shown) <= width) {
            return shown;
        }
        space = text.indexOf(u' ', space + 1);
    }
    // One word longer than the card: its end
    return metrics.elidedText(text, Qt::ElideLeft, width);
}

/// @brief Whether an entry passes the search and the kinds
bool passes(const FootnoteEntry& entry, FootnoteKinds kinds, const QString& search) {
    if ((kinds == FootnoteKinds::Footnotes && entry.kind != editor::NoteKind::Footnote) ||
        (kinds == FootnoteKinds::Endnotes && entry.kind != editor::NoteKind::Endnote)) {
        return false;
    }
    return search.isEmpty() || entry.text.contains(search, Qt::CaseInsensitive);
}

/// @brief How the cards look: the settings and the colors of the kinds
struct CardLook {
    bool before = true;
    int lines = 2;
    std::array<QColor, 2> kinds;
};

/// @brief Paints the cards of the notes and the titles of the texts
class CardDelegate : public QStyledItemDelegate {
public:
    explicit CardDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

    CardLook look;

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override {
        const QFontMetrics metrics(option.font);
        const int line = metrics.lineSpacing();
        if (index.data(HEADING_ROLE).toBool()) {
            return {option.rect.width(), line + (2 * CARD_MARGIN) + CARD_PADDING};
        }
        int lines = 1 + look.lines;
        if (look.before && !index.data(BEFORE_ROLE).toString().isEmpty()) {
            ++lines;
        }
        return {option.rect.width(), (lines * line) + (2 * CARD_PADDING) + (2 * CARD_MARGIN)};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QPalette& palette = option.palette;
        const QFontMetrics metrics(option.font);
        if (index.data(HEADING_ROLE).toBool()) {
            QFont bold = option.font;
            bold.setBold(true);
            painter->setFont(bold);
            painter->setPen(palette.color(QPalette::WindowText));
            const QRect area = option.rect.adjusted(CARD_MARGIN, CARD_MARGIN + CARD_PADDING,
                                                    -CARD_MARGIN, -CARD_MARGIN);
            painter->drawText(area, Qt::AlignLeft | Qt::AlignVCenter,
                              QFontMetrics(bold).elidedText(index.data(Qt::DisplayRole).toString(),
                                                            Qt::ElideRight, area.width()));
            painter->restore();
            return;
        }

        const bool selected = (option.state & QStyle::State_Selected) != 0;
        const QRectF card = QRectF(option.rect).adjusted(CARD_MARGIN, CARD_MARGIN, -CARD_MARGIN,
                                                         -CARD_MARGIN);
        const QColor base = palette.color(QPalette::Base);
        const QColor highlight = palette.color(QPalette::Highlight);
        QPainterPath shape;
        shape.addRoundedRect(card, CARD_RADIUS, CARD_RADIUS);
        painter->fillPath(shape, selected ? mixed(base, highlight, SELECTED_TINT) : base);
        const auto kind = static_cast<editor::NoteKind>(index.data(KIND_ROLE).toInt());
        const QColor color = look.kinds.at(kind == editor::NoteKind::Endnote ? 1 : 0);
        painter->save();
        painter->setClipPath(shape);
        painter->fillRect(QRectF(card.left(), card.top(), BAR_WIDTH, card.height()), color);
        painter->restore();
        painter->setPen(QPen(selected ? highlight : palette.color(QPalette::Mid),
                             selected ? SELECTED_BORDER : 1));
        painter->drawPath(shape);

        const QRect inner = card.toRect().adjusted(BAR_WIDTH + CARD_PADDING, CARD_PADDING,
                                                   -CARD_PADDING, -CARD_PADDING);
        const int line = metrics.lineSpacing();
        int y = inner.top();
        // The number and the kind
        QFont bold = option.font;
        bold.setBold(true);
        painter->setFont(bold);
        painter->setPen(color);
        const QString number = index.data(NUMBER_ROLE).toString();
        painter->drawText(QRect(inner.left(), y, inner.width(), line), Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(bold).elidedText(
                              editor::BookEditor::noteTitle(kind, number), Qt::ElideRight,
                              inner.width()));
        y += line;
        // The words before the number
        const QString before = index.data(BEFORE_ROLE).toString();
        if (look.before && !before.isEmpty()) {
            QFont italic = option.font;
            italic.setItalic(true);
            painter->setFont(italic);
            painter->setPen(palette.color(QPalette::PlaceholderText));
            painter->drawText(QRect(inner.left(), y, inner.width(), line),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              lastWords(before, QFontMetrics(italic), inner.width()));
            y += line;
        }
        // The beginning of its text, in the lines the settings give
        painter->setFont(option.font);
        painter->setPen(palette.color(QPalette::Text));
        drawLines(painter, index.data(TEXT_ROLE).toString(), option.font,
                  QRect(inner.left(), y, inner.width(), line * look.lines), look.lines);
        painter->restore();
    }

private:
    /// @brief @p text wrapped in at most @p lines lines, the last one cut with "…"
    static void drawLines(QPainter* painter, const QString& text, const QFont& font,
                          const QRect& area, int lines) {
        const QFontMetrics metrics(font);
        QTextLayout layout(text, font);
        QTextOption wrap;
        wrap.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(wrap);
        layout.beginLayout();
        int y = area.top();
        for (int shown = 0; shown < lines; ++shown) {
            QTextLine textLine = layout.createLine();
            if (!textLine.isValid()) {
                break;
            }
            textLine.setLineWidth(area.width());
            const bool last = shown == lines - 1;
            const int end = textLine.textStart() + textLine.textLength();
            if (last && end < text.size()) {
                const QString rest = text.mid(textLine.textStart());
                painter->drawText(QPoint(area.left(), y + metrics.ascent()),
                                  metrics.elidedText(rest, Qt::ElideRight, area.width()));
                break;
            }
            painter->drawText(QPoint(area.left(), y + metrics.ascent()),
                              text.mid(textLine.textStart(), textLine.textLength()).trimmed());
            y += metrics.lineSpacing();
        }
        layout.endLayout();
    }
};

}  // namespace

// =============================================================================
// The panel
// =============================================================================

FootnotesPanel::FootnotesPanel(CommandRegistry& commands, QWidget* parent)
    : QWidget(parent)
    , m_commands(commands)
{
    setObjectName(QStringLiteral("footnotesPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN);
    layout->setSpacing(PANEL_MARGIN);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("footnotesSearch"));
    //: The search of the Footnotes panel. In Polish: Szukaj w przypisach...
    m_search->setPlaceholderText(tr("Search in the notes..."));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(m_search->placeholderText());
    m_search->installEventFilter(this);
    connect(m_search, &QLineEdit::textChanged, this, [this]() { rebuild(); });
    layout->addWidget(m_search);

    // The kinds, then the scope; in two rows in a narrow panel
    auto* buttonRow = new QWidget(this);
    auto* buttons = new FlowLayout(buttonRow, PANEL_MARGIN, PANEL_MARGIN);
    buttons->setContentsMargins(0, 0, 0, 0);
    m_kindGroup = new QButtonGroup(this);
    const std::array<QString, 3> kindNames{
        //: The button of the Footnotes panel that shows every note. In Polish: Wszystkie
        tr("All"),
        //: The button of the Footnotes panel that shows the footnotes. In Polish: Dolne
        tr("Footnotes"),
        //: The button of the Footnotes panel that shows the endnotes. In Polish: Końcowe
        tr("Endnotes")};
    const std::array<const char*, 3> kindObjects{"footnotesAll", "footnotesFootnotes",
                                                  "footnotesEndnotes"};
    for (size_t i = 0; i < m_kindButtons.size(); ++i) {
        auto* button = new QToolButton(buttonRow);
        button->setObjectName(QString::fromLatin1(kindObjects.at(i)));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setText(kindNames.at(i));
        button->setAccessibleName(kindNames.at(i));
        m_kindGroup->addButton(button, static_cast<int>(i));
        buttons->addWidget(button);
        m_kindButtons.at(i) = button;
    }
    m_kindButtons.at(0)->setChecked(true);
    connect(m_kindGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_kinds = static_cast<FootnoteKinds>(id);
        rebuild();
    });

    m_scopeGroup = new QButtonGroup(this);
    m_textScope = new QToolButton(buttonRow);
    m_textScope->setObjectName(QStringLiteral("footnotesTextScope"));
    m_bookScope = new QToolButton(buttonRow);
    m_bookScope->setObjectName(QStringLiteral("footnotesBookScope"));
    //: The scope of the Footnotes panel: the notes of every text of the book. In Polish:
    //: Książka
    m_bookScope->setText(tr("Book"));
    for (QToolButton* button : {m_textScope, m_bookScope}) {
        button->setCheckable(true);
        button->setAutoRaise(true);
        buttons->addWidget(button);
    }
    m_scopeGroup->addButton(m_textScope, static_cast<int>(FootnoteScope::Text));
    m_scopeGroup->addButton(m_bookScope, static_cast<int>(FootnoteScope::Book));
    m_textScope->setChecked(true);
    m_bookScope->setEnabled(false);
    connect(m_scopeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        const auto scope = static_cast<FootnoteScope>(id);
        if (scope != m_scope) {
            // The titles of the texts shown or hidden, even when the notes stay the same
            m_scope = scope;
            rebuild();
            emit scopeChanged(scope);
        }
    });
    layout->addWidget(buttonRow);
    updateScopeText();

    m_list = new QListView(this);
    m_list->setObjectName(QStringLiteral("footnotesList"));
    //: In Polish: Przypisy
    m_list->setAccessibleName(tr("Notes"));
    m_model = new QStandardItemModel(this);
    m_list->setModel(m_model);
    m_list->setItemDelegate(new CardDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setContextMenuPolicy(Qt::DefaultContextMenu);
    m_list->installEventFilter(this);
    m_list->viewport()->installEventFilter(this);
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current) {
                const FootnoteEntry* entry = current.isValid() ? entryAt(current.row()) : nullptr;
                if (!m_updating && entry != nullptr) {
                    emit entryActivated(*entry);
                }
            });
    connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (const FootnoteEntry* entry = entryAt(index.row())) {
            emit editRequested(*entry);
        }
    });
    layout->addWidget(m_list, 1);

    m_empty = new QLabel(this);
    m_empty->setObjectName(QStringLiteral("footnotesEmpty"));
    m_empty->setWordWrap(true);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_empty);

    applySettings();
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &FootnotesPanel::applySettings);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            &FootnotesPanel::applySettings);
    // The settings of the cards; the listener runs on the thread that changes them
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key == BEFORE_SETTING || key == LINES_SETTING) {
                QMetaObject::invokeMethod(this, &FootnotesPanel::applySettings,
                                          Qt::QueuedConnection);
            }
        });
    rebuild();
}

FootnotesPanel::~FootnotesPanel()
{
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

void FootnotesPanel::setEntries(const std::vector<FootnoteEntry>& entries)
{
    if (entries == m_entries) {
        return;
    }
    // The rows name the notes given before: the selected one is found before they go
    const QString kept = selectedKey();
    m_entries = entries;
    m_rowEntries.clear();
    rebuild(kept);
}

std::vector<FootnoteEntry> FootnotesPanel::shownEntries() const
{
    std::vector<FootnoteEntry> shown;
    for (const int entry : m_rowEntries) {
        if (entry >= 0) {
            shown.push_back(m_entries.at(static_cast<size_t>(entry)));
        }
    }
    return shown;
}

void FootnotesPanel::setScope(FootnoteScope scope)
{
    if (scope == FootnoteScope::Book && !m_bookAvailable) {
        scope = FootnoteScope::Text;
    }
    m_scope = scope;
    (scope == FootnoteScope::Book ? m_bookScope : m_textScope)->setChecked(true);
    rebuild();
}

void FootnotesPanel::setBookScopeAvailable(bool available)
{
    m_bookAvailable = available;
    m_bookScope->setEnabled(available);
    if (!available && m_scope == FootnoteScope::Book) {
        setScope(FootnoteScope::Text);
    }
}

void FootnotesPanel::setDocumentState(bool inFront, bool notesAllowed)
{
    m_inFront = inFront;
    m_notesAllowed = notesAllowed;
    updateEmptyText(!m_rowEntries.empty());
}

void FootnotesPanel::setTextName(const QString& name)
{
    m_textName = name;
    updateScopeText();
}

void FootnotesPanel::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
}

void FootnotesPanel::setKinds(FootnoteKinds kinds)
{
    m_kinds = kinds;
    m_kindButtons.at(static_cast<size_t>(kinds))->setChecked(true);
    rebuild();
}

QString FootnotesPanel::searchText() const
{
    return m_search->text();
}

void FootnotesPanel::setSearchText(const QString& text)
{
    m_search->setText(text);
}

void FootnotesPanel::updateScopeText()
{
    //: The scope of the Footnotes panel: the notes of the text in front. In Polish: Tekst
    const QString name = m_textName.isEmpty() ? tr("Text") : m_textName;
    // A long name ("Copyright page") does not widen the narrow panel: it is cut, and the
    // tooltip has all of it
    const QFontMetrics metrics(m_textScope->font());
    const QString shown =
        metrics.elidedText(name, Qt::ElideRight, metrics.averageCharWidth() * MAX_SCOPE_CHARS);
    m_textScope->setText(shown);
    m_textScope->setToolTip(shown == name ? QString() : name);
}

void FootnotesPanel::rebuild()
{
    rebuild(selectedKey());
}

void FootnotesPanel::rebuild(const QString& kept)
{
    m_updating = true;
    m_model->clear();
    m_rowEntries.clear();
    const QString search = m_search->text().trimmed();
    QString heading;
    bool first = true;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const FootnoteEntry& entry = m_entries.at(i);
        if (!passes(entry, m_kinds, search)) {
            continue;
        }
        if (m_scope == FootnoteScope::Book && (first || entry.elementId != heading)) {
            heading = entry.elementId;
            auto* title = new QStandardItem(entry.textTitle);
            title->setData(true, HEADING_ROLE);
            title->setFlags(Qt::NoItemFlags);  // the arrows go over it
            m_model->appendRow(title);
            m_rowEntries.push_back(-1);
        }
        first = false;
        auto* item = new QStandardItem(
            entry.before.isEmpty() ? titleOf(entry) + QStringLiteral(": ") + entry.text
                                   : titleOf(entry) + QStringLiteral(": ") + entry.text +
                                         QStringLiteral(" (") + entry.before +
                                         QLatin1Char(')'));
        item->setData(false, HEADING_ROLE);
        item->setData(entry.number, NUMBER_ROLE);
        item->setData(static_cast<int>(entry.kind), KIND_ROLE);
        item->setData(entry.before, BEFORE_ROLE);
        item->setData(entry.text, TEXT_ROLE);
        item->setToolTip(entry.text);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_model->appendRow(item);
        m_rowEntries.push_back(static_cast<int>(i));
    }
    // The note selected before stays selected, without going to it again
    for (size_t row = 0; row < m_rowEntries.size(); ++row) {
        const FootnoteEntry* entry = entryAt(static_cast<int>(row));
        if (entry != nullptr && entry->key() == kept) {
            const QModelIndex index = m_model->index(static_cast<int>(row), 0);
            m_list->setCurrentIndex(index);
            m_list->scrollTo(index);
            break;
        }
    }
    m_updating = false;
    updateCounts();
    updateEmptyText(!m_rowEntries.empty());
}

void FootnotesPanel::updateCounts()
{
    const QString search = m_search->text().trimmed();
    std::array<int, 3> counts{};
    for (const FootnoteEntry& entry : m_entries) {
        if (passes(entry, FootnoteKinds::All, search)) {
            ++counts[0];
            ++counts[entry.kind == editor::NoteKind::Endnote ? 2 : 1];
        }
    }
    const std::array<QString, 3> names{tr("All"), tr("Footnotes"), tr("Endnotes")};
    for (size_t i = 0; i < m_kindButtons.size(); ++i) {
        //: A button of the kinds of the Footnotes panel with its count: "Footnotes (4)"
        m_kindButtons.at(i)->setText(tr("%1 (%2)").arg(names.at(i)).arg(counts.at(i)));
    }
}

void FootnotesPanel::updateEmptyText(bool anyShown)
{
    m_list->setVisible(anyShown);
    m_empty->setVisible(!anyShown);
    if (anyShown) {
        return;
    }
    QString text;
    if (m_scope == FootnoteScope::Text && !m_inFront) {
        text = m_bookAvailable
                   //: In Polish: Żaden tekst nie jest otwarty. Otwórz tekst albo wybierz
                   //: „Książka”, aby zobaczyć przypisy całej książki.
                   ? tr("No text is open. Open a text, or choose Book to see the notes of the "
                        "whole book.")
                   //: In Polish: Żaden tekst nie jest otwarty.
                   : tr("No text is open.");
    } else if (m_scope == FootnoteScope::Text && !m_notesAllowed) {
        //: In Polish: Plik zwykłego tekstu nie ma przypisów.
        text = tr("A plain text file keeps no notes.");
    } else if (m_entries.empty()) {
        text = m_scope == FootnoteScope::Book
                   //: In Polish: Książka nie ma jeszcze przypisów.
                   ? tr("The book has no notes yet.")
                   //: In Polish: Ten tekst nie ma jeszcze przypisów: dodaje je Wstaw > Przypis
                   //: dolny albo Wstaw > Przypis końcowy.
                   : tr("This text has no notes yet: Insert > Footnote or Insert > Endnote "
                        "adds one.");
    } else if (!m_search->text().trimmed().isEmpty()) {
        //: In Polish: Żaden przypis nie zawiera „%1”.
        text = tr("No note contains “%1”.").arg(m_search->text().trimmed());
    } else {
        //: In Polish: Brak przypisów tego rodzaju.
        text = tr("No notes of this kind.");
    }
    m_empty->setText(text);
}

const FootnoteEntry* FootnotesPanel::entryAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rowEntries.size())) {
        return nullptr;
    }
    const int entry = m_rowEntries.at(static_cast<size_t>(row));
    return entry >= 0 ? &m_entries.at(static_cast<size_t>(entry)) : nullptr;
}

const FootnoteEntry* FootnotesPanel::selectedEntry() const
{
    const QModelIndex current = m_list->currentIndex();
    return current.isValid() ? entryAt(current.row()) : nullptr;
}

void FootnotesPanel::selectEntry(const QString& key)
{
    for (size_t row = 0; row < m_rowEntries.size(); ++row) {
        const FootnoteEntry* entry = entryAt(static_cast<int>(row));
        if (entry != nullptr && entry->key() == key) {
            const QModelIndex index = m_model->index(static_cast<int>(row), 0);
            m_updating = true;
            m_list->setCurrentIndex(index);
            m_updating = false;
            m_list->scrollTo(index);
            return;
        }
    }
    m_list->clearSelection();
    m_list->setCurrentIndex(QModelIndex());
}

QString FootnotesPanel::selectedKey() const
{
    const FootnoteEntry* entry = selectedEntry();
    return entry != nullptr ? entry->key() : QString();
}

void FootnotesPanel::focusList(const QString& key)
{
    if (!key.isEmpty()) {
        selectEntry(key);
    }
    if (selectedEntry() == nullptr) {
        for (size_t row = 0; row < m_rowEntries.size(); ++row) {
            if (m_rowEntries.at(row) >= 0) {
                m_list->setCurrentIndex(m_model->index(static_cast<int>(row), 0));
                break;
            }
        }
    }
    m_list->setFocus(Qt::OtherFocusReason);
}

bool FootnotesPanel::hasFocusInside() const
{
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && (focus == this || isAncestorOf(focus));
}

QColor FootnotesPanel::kindColor(editor::NoteKind kind) const
{
    return m_kindColors.at(kind == editor::NoteKind::Endnote ? 1 : 0);
}

QAbstractButton* FootnotesPanel::kindButton(FootnoteKinds kinds) const
{
    return m_kindButtons.at(static_cast<size_t>(kinds));
}

QAbstractButton* FootnotesPanel::scopeButton(FootnoteScope scope) const
{
    return scope == FootnoteScope::Book ? m_bookScope : m_textScope;
}

void FootnotesPanel::applySettings()
{
    const auto& themes = core::ThemeManager::getInstance();
    m_kindColors = {themes.editorColor(FOOTNOTE_COLOR_KEY, palette().color(QPalette::Highlight)),
                    themes.editorColor(ENDNOTE_COLOR_KEY, palette().color(QPalette::Link))};
    auto& settings = core::SettingsManager::getInstance();
    auto* delegate = static_cast<CardDelegate*>(m_list->itemDelegate());
    delegate->look.before = settings.get<bool>(BEFORE_SETTING, true);
    delegate->look.lines = std::clamp(settings.get<int>(LINES_SETTING, 2), MIN_LINES, MAX_LINES);
    delegate->look.kinds = m_kindColors;
    // The rows take their new heights
    m_list->doItemsLayout();
    m_list->viewport()->update();
}

// =============================================================================
// Keys and menus
// =============================================================================

bool FootnotesPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Down && !m_rowEntries.empty()) {
            focusList();
            return true;
        }
    }
    if (watched == m_list && event->type() == QEvent::KeyPress &&
        listKeyPressed(static_cast<QKeyEvent*>(event))) {
        return true;
    }
    if (watched == m_list->viewport() && event->type() == QEvent::ContextMenu) {
        const auto* menuEvent = static_cast<QContextMenuEvent*>(event);
        const QModelIndex index = m_list->indexAt(menuEvent->pos());
        if (entryAt(index.row()) != nullptr) {
            m_list->setCurrentIndex(index);
            showMenu(menuEvent->globalPos());
        }
        return true;
    }
    if (watched == m_list && event->type() == QEvent::ContextMenu) {
        const QRect card = m_list->visualRect(m_list->currentIndex());
        showMenu(m_list->viewport()->mapToGlobal(card.isValid() ? card.bottomLeft() : QPoint()));
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

bool FootnotesPanel::listKeyPressed(const QKeyEvent* event)
{
    const FootnoteEntry* entry = selectedEntry();
    const bool plain = (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_F2:
        if (plain && entry != nullptr) {
            emit editRequested(*entry);
            return true;
        }
        break;
    case Qt::Key_Delete:
        if (plain && entry != nullptr && !m_readOnly) {
            emit deleteRequested(*entry);
            return true;
        }
        break;
    case Qt::Key_Menu:
        showMenu(m_list->viewport()->mapToGlobal(
            m_list->visualRect(m_list->currentIndex()).bottomLeft()));
        return true;
    case Qt::Key_F10:
        if (event->modifiers() == Qt::ShiftModifier) {
            showMenu(m_list->viewport()->mapToGlobal(
                m_list->visualRect(m_list->currentIndex()).bottomLeft()));
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

void FootnotesPanel::keyPressEvent(QKeyEvent* event)
{
    // Esc from anywhere in the panel: back to the text
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        emit editorFocusRequested();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

QMenu* FootnotesPanel::createMenu(const QString& key)
{
    const FootnoteEntry* found = nullptr;
    for (size_t row = 0; row < m_rowEntries.size(); ++row) {
        const FootnoteEntry* entry = entryAt(static_cast<int>(row));
        if (entry != nullptr && entry->key() == key) {
            found = entry;
        }
    }
    if (found == nullptr) {
        return nullptr;
    }
    const FootnoteEntry entry = *found;
    const bool endnote = entry.kind == editor::NoteKind::Endnote;
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("footnotesMenu"));
    menu->addAction(
        //: The menu of a note in the Footnotes panel. In Polish: &Edytuj przypis końcowy
        endnote ? tr("&Edit Endnote")
                //: In Polish: &Edytuj przypis dolny
                : tr("&Edit Footnote"),
        this, [this, entry]() { emit editRequested(entry); });
    QAction* convert = menu->addAction(
        //: In Polish: &Zamień na przypis dolny
        endnote ? tr("Con&vert to Footnote")
                //: In Polish: &Zamień na przypis końcowy
                : tr("Con&vert to Endnote"),
        this, [this, entry]() { emit convertRequested(entry); });
    convert->setEnabled(!m_readOnly);
    //: Copies the text of a note in the Footnotes panel. In Polish: &Kopiuj treść
    menu->addAction(tr("&Copy Text"), this,
                    [entry]() { QGuiApplication::clipboard()->setText(entry.text); });
    QAction* remove = menu->addAction(
        //: In Polish: &Usuń przypis końcowy
        endnote ? tr("&Delete Endnote")
                //: In Polish: &Usuń przypis dolny
                : tr("&Delete Footnote"),
        this, [this, entry]() { emit deleteRequested(entry); });
    remove->setEnabled(!m_readOnly);
    if (QAction* layout = m_commands.getAction(QStringLiteral("insert.footnoteSettings"))) {
        menu->addSeparator();
        menu->addAction(layout);
    }
    return menu;
}

void FootnotesPanel::showMenu(const QPoint& globalPos)
{
    const FootnoteEntry* entry = selectedEntry();
    if (entry == nullptr) {
        return;
    }
    const std::unique_ptr<QMenu> menu(createMenu(entry->key()));
    if (menu) {
        menu->exec(globalPos);
    }
}

}  // namespace kalahari::gui
