// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file figures_panel.cpp
/// @brief The panel Images and Tables: the images and the tables of the text in front or of
///        the book, with their captions

#include "kalahari/gui/panels/figures_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
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
#include <utility>

namespace kalahari::gui {

namespace {

constexpr int PANEL_MARGIN = 6;   ///< Around the parts of the panel
constexpr int CARD_MARGIN = 4;    ///< Around a card in the list
constexpr int CARD_PADDING = 6;   ///< Inside a card
constexpr int BAR_WIDTH = 4;      ///< The bar of the color of the kind
constexpr int CARD_RADIUS = 4;    ///< The corners of a card
constexpr int SELECTED_BORDER = 2;
constexpr double SELECTED_TINT = 0.12;  ///< The highlight in the background of the selected card
constexpr double HEADER_TINT = 0.3;     ///< The first row of the drawing of a table
constexpr int MAX_SCOPE_CHARS = 16;     ///< The name of the text in front on its button, at most
constexpr int MIN_PICTURE_LINES = 2;    ///< The picture of a card: at least as high as two lines
constexpr int MAX_PICTURE_LINES = 4;    ///< and at most as four
constexpr int MAX_DRAWN_CELLS = 4;      ///< The rows and columns of the drawing of a table, at most
constexpr qsizetype MAX_KEPT_PICTURES = 300;  ///< The pictures kept for the cards, at most

/// @brief The data of a row of the list
enum Role : int {
    HEADING_ROLE = Qt::UserRole + 1,  ///< bool: the title of a text, not a figure
    KIND_ROLE,                        ///< int: editor::FigureKind
    TITLE_ROLE,                       ///< QString: "Figure 3", or the kind
    TEXT_ROLE,                        ///< QString: the text of its caption, or what it is
    CAPTIONED_ROLE,                   ///< bool: TEXT_ROLE is the text of its caption
    ROWS_ROLE,                        ///< int: of a table
    COLUMNS_ROLE,                     ///< int: of a table
};

/// @brief @p a mixed with @p b, @p share of @p b
QColor mixed(const QColor& a, const QColor& b, double share) {
    return QColor::fromRgbF(static_cast<float>(a.redF() + ((b.redF() - a.redF()) * share)),
                            static_cast<float>(a.greenF() + ((b.greenF() - a.greenF()) * share)),
                            static_cast<float>(a.blueF() + ((b.blueF() - a.blueF()) * share)));
}

/// @brief Whether an entry is of a kind
bool isImage(const FigureEntry& entry) {
    return entry.figure.kind == editor::FigureKind::Image;
}

/// @brief The size of a table in words: "3 rows, 4 columns"
QString tableSize(const editor::FigureInText& figure) {
    //: The size of a table without a caption on its card in the panel Images and Tables. In
    //: Polish: %n wiersz / %n wiersze / %n wierszy
    const QString rows = FiguresPanel::tr("%n row(s)", "", figure.rows);
    //: In Polish: %n kolumna / %n kolumny / %n kolumn
    const QString columns = FiguresPanel::tr("%n column(s)", "", figure.columns);
    //: The rows and the columns of a table: "3 rows, 4 columns". In Polish: %1, %2
    return FiguresPanel::tr("%1, %2").arg(rows, columns);
}

/// @brief What a figure is, beside its caption: the name of an image, the size of a table
QString detailOf(const FigureEntry& entry) {
    return isImage(entry) ? editor::imageNameOf(entry.figure.image) : tableSize(entry.figure);
}

/// @brief The title of a card: the label and the number of its caption, else its kind
QString titleOf(const FigureEntry& entry) {
    if (!entry.figure.title.isEmpty()) {
        return entry.figure.title;
    }
    if (isImage(entry)) {
        //: The title of a card of an image without a numbered caption. In Polish: Obraz
        return FiguresPanel::tr("Image");
    }
    //: The title of a card of a table without a numbered caption. In Polish: Tabela
    return FiguresPanel::tr("Table");
}

/// @brief The text of a card under its title: the text of its caption, else what it is
QString textOf(const FigureEntry& entry) {
    if (entry.figure.captioned && !entry.figure.caption.isEmpty()) {
        return entry.figure.caption;
    }
    if (entry.figure.captioned) {
        return detailOf(entry);
    }
    //: A card of the panel Images and Tables without a caption: "No caption · bridge.png". In
    //: Polish: Bez podpisu · %1
    return FiguresPanel::tr("No caption · %1").arg(detailOf(entry));
}

/// @brief Whether an entry passes the search and the kinds
bool passes(const FigureEntry& entry, FigureKinds kinds, const QString& search) {
    if ((kinds == FigureKinds::Images && !isImage(entry)) ||
        (kinds == FigureKinds::Tables && isImage(entry))) {
        return false;
    }
    if (search.isEmpty()) {
        return true;
    }
    return entry.figure.whole.contains(search, Qt::CaseInsensitive) ||
           detailOf(entry).contains(search, Qt::CaseInsensitive) ||
           (isImage(entry) && entry.figure.image.source.contains(search, Qt::CaseInsensitive));
}

/// @brief How the cards look: the settings and the colors of the kinds
struct CardLook {
    bool pictures = true;
    int lines = FiguresPanel::DEFAULT_LINES;
    std::array<QColor, 2> kinds;
};

/// @brief Paints the cards of the figures and the titles of the texts
class CardDelegate : public QStyledItemDelegate {
public:
    /// @brief The picture of the image of a row in these pixels (null: none)
    using PictureOf = std::function<QImage(const QModelIndex& index, const QSize& pixels)>;

    CardDelegate(QObject* parent, PictureOf pictureOf)
        : QStyledItemDelegate(parent)
        , m_pictureOf(std::move(pictureOf)) {}

    CardLook look;

    /// @brief The side of the picture of a card
    [[nodiscard]] int pictureSide(const QFontMetrics& metrics) const {
        const int lines = std::clamp(1 + look.lines, MIN_PICTURE_LINES, MAX_PICTURE_LINES);
        return lines * metrics.lineSpacing();
    }

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override {
        const QFontMetrics metrics(option.font);
        const int line = metrics.lineSpacing();
        if (index.data(HEADING_ROLE).toBool()) {
            return {option.rect.width(), line + (2 * CARD_MARGIN) + CARD_PADDING};
        }
        int inside = (1 + look.lines) * line;
        if (look.pictures) {
            inside = std::max(inside, pictureSide(metrics));
        }
        return {option.rect.width(), inside + (2 * CARD_PADDING) + (2 * CARD_MARGIN)};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        const QPalette& palette = option.palette;
        const QFontMetrics metrics(option.font);
        if (index.data(HEADING_ROLE).toBool()) {
            paintHeading(painter, option, index);
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
        const auto kind = static_cast<editor::FigureKind>(index.data(KIND_ROLE).toInt());
        const QColor color = look.kinds.at(kind == editor::FigureKind::Table ? 1 : 0);
        painter->save();
        painter->setClipPath(shape);
        painter->fillRect(QRectF(card.left(), card.top(), BAR_WIDTH, card.height()), color);
        painter->restore();
        painter->setPen(QPen(selected ? highlight : palette.color(QPalette::Mid),
                             selected ? SELECTED_BORDER : 1));
        painter->drawPath(shape);

        QRect inner = card.toRect().adjusted(BAR_WIDTH + CARD_PADDING, CARD_PADDING,
                                             -CARD_PADDING, -CARD_PADDING);
        if (look.pictures) {
            const int side = pictureSide(metrics);
            const QRect box(inner.left(), inner.top(), side, side);
            if (kind == editor::FigureKind::Table) {
                paintTable(painter, box, index, color, palette);
            } else {
                paintPicture(painter, box, index, palette);
            }
            inner.setLeft(box.right() + 1 + CARD_PADDING);
        }

        const int line = metrics.lineSpacing();
        int y = inner.top();
        // The label and the number of its caption, or its kind
        QFont bold = option.font;
        bold.setBold(true);
        painter->setFont(bold);
        painter->setPen(color);
        painter->drawText(QRect(inner.left(), y, inner.width(), line),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(bold).elidedText(index.data(TITLE_ROLE).toString(),
                                                        Qt::ElideRight, inner.width()));
        y += line;
        // The text of its caption in the lines the settings give, else what it is
        QFont font = option.font;
        if (index.data(CAPTIONED_ROLE).toBool()) {
            painter->setPen(palette.color(QPalette::Text));
        } else {
            font.setItalic(true);
            painter->setPen(palette.color(QPalette::PlaceholderText));
        }
        painter->setFont(font);
        drawLines(painter, index.data(TEXT_ROLE).toString(), font,
                  QRect(inner.left(), y, inner.width(), line * look.lines), look.lines);
        painter->restore();
    }

private:
    PictureOf m_pictureOf;

    /// @brief The title of a text, in the scope Book
    static void paintHeading(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) {
        QFont bold = option.font;
        bold.setBold(true);
        painter->setFont(bold);
        painter->setPen(option.palette.color(QPalette::WindowText));
        const QRect area = option.rect.adjusted(CARD_MARGIN, CARD_MARGIN + CARD_PADDING,
                                                -CARD_MARGIN, -CARD_MARGIN);
        painter->drawText(area, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(bold).elidedText(index.data(Qt::DisplayRole).toString(),
                                                        Qt::ElideRight, area.width()));
    }

    /// @brief The picture of an image in its box, whole; its file missing: the icon of an image
    void paintPicture(QPainter* painter, const QRect& box, const QModelIndex& index,
                      const QPalette& palette) const {
        const qreal ratio = painter->device() != nullptr ? painter->device()->devicePixelRatioF()
                                                          : 1.0;
        const QSize pixels(qRound(box.width() * ratio), qRound(box.height() * ratio));
        const QImage picture = m_pictureOf ? m_pictureOf(index, pixels) : QImage();
        if (picture.isNull()) {
            painter->setPen(QPen(palette.color(QPalette::Mid), 1, Qt::DashLine));
            painter->drawRect(box.adjusted(0, 0, -1, -1));
            const int icon = box.width() / 2;
            core::ArtProvider::getInstance()
                .getIcon(QStringLiteral("image.properties"))
                .paint(painter, QRect(box.center() - QPoint(icon / 2, icon / 2), QSize(icon, icon)),
                       Qt::AlignCenter, QIcon::Disabled);
            return;
        }
        QSizeF shown = QSizeF(picture.size()) / ratio;
        shown.scale(QSizeF(box.size()), Qt::KeepAspectRatio);
        const QRectF target(QPointF(box.left() + ((box.width() - shown.width()) / 2.0),
                                    box.top() + ((box.height() - shown.height()) / 2.0)),
                            shown);
        painter->drawImage(target, picture);
        painter->setPen(QPen(palette.color(QPalette::Mid), 1));
        painter->drawRect(target.adjusted(0.5, 0.5, -0.5, -0.5));
    }

    /// @brief A table as a drawing of its first rows and columns, the first row tinted
    static void paintTable(QPainter* painter, const QRect& box, const QModelIndex& index,
                           const QColor& color, const QPalette& palette) {
        const int rows = std::clamp(index.data(ROWS_ROLE).toInt(), 1, MAX_DRAWN_CELLS);
        const int columns = std::clamp(index.data(COLUMNS_ROLE).toInt(), 1, MAX_DRAWN_CELLS);
        const QRectF area = QRectF(box).adjusted(1.5, 1.5, -1.5, -1.5);
        const qreal rowHeight = area.height() / rows;
        const qreal columnWidth = area.width() / columns;
        painter->fillRect(area, palette.color(QPalette::Base));
        painter->fillRect(QRectF(area.left(), area.top(), area.width(), rowHeight),
                          mixed(palette.color(QPalette::Base), color, HEADER_TINT));
        painter->setPen(QPen(color, 1));
        painter->drawRect(area);
        for (int row = 1; row < rows; ++row) {
            const qreal y = area.top() + (row * rowHeight);
            painter->drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        }
        for (int column = 1; column < columns; ++column) {
            const qreal x = area.left() + (column * columnWidth);
            painter->drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        }
    }

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

QString FigureEntry::key() const
{
    return elementId + (figure.kind == editor::FigureKind::Table ? QStringLiteral("/t")
                                                                  : QStringLiteral("/i")) +
           QString::number(figure.ordinal);
}

// =============================================================================
// The panel
// =============================================================================

FiguresPanel::FiguresPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("figuresPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN);
    layout->setSpacing(PANEL_MARGIN);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("figuresSearch"));
    //: The search of the panel Images and Tables. In Polish: Szukaj w podpisach i nazwach...
    m_search->setPlaceholderText(tr("Search in the captions and names..."));
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
    const std::array<const char*, 3> kindObjects{"figuresAll", "figuresImages", "figuresTables"};
    for (size_t i = 0; i < m_kindButtons.size(); ++i) {
        auto* button = new QToolButton(buttonRow);
        button->setObjectName(QString::fromLatin1(kindObjects.at(i)));
        button->setCheckable(true);
        button->setAutoRaise(true);
        m_kindGroup->addButton(button, static_cast<int>(i));
        buttons->addWidget(button);
        m_kindButtons.at(i) = button;
    }
    m_kindButtons.at(0)->setChecked(true);
    connect(m_kindGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_kinds = static_cast<FigureKinds>(id);
        rebuild();
    });

    m_scopeGroup = new QButtonGroup(this);
    m_textScope = new QToolButton(buttonRow);
    m_textScope->setObjectName(QStringLiteral("figuresTextScope"));
    m_bookScope = new QToolButton(buttonRow);
    m_bookScope->setObjectName(QStringLiteral("figuresBookScope"));
    //: The scope of the panel Images and Tables: the figures of every text of the book. In
    //: Polish: Książka
    m_bookScope->setText(tr("Book"));
    for (QToolButton* button : {m_textScope, m_bookScope}) {
        button->setCheckable(true);
        button->setAutoRaise(true);
        buttons->addWidget(button);
    }
    m_scopeGroup->addButton(m_textScope, static_cast<int>(FigureScope::Text));
    m_scopeGroup->addButton(m_bookScope, static_cast<int>(FigureScope::Book));
    m_textScope->setChecked(true);
    m_bookScope->setEnabled(false);
    connect(m_scopeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        const auto scope = static_cast<FigureScope>(id);
        if (scope != m_scope) {
            // The titles of the texts shown or hidden, even when the figures stay the same
            m_scope = scope;
            rebuild();
            emit scopeChanged(scope);
        }
    });
    layout->addWidget(buttonRow);
    updateScopeText();

    m_list = new QListView(this);
    m_list->setObjectName(QStringLiteral("figuresList"));
    //: The list of the panel Images and Tables. In Polish: Obrazy i tabele
    m_list->setAccessibleName(tr("Images and Tables"));
    m_model = new QStandardItemModel(this);
    m_list->setModel(m_model);
    m_list->setItemDelegate(new CardDelegate(
        m_list, [this](const QModelIndex& index, const QSize& pixels) {
            const FigureEntry* entry = entryAt(index.row());
            return entry != nullptr ? pictureOf(*entry, pixels) : QImage();
        }));
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
                const FigureEntry* entry = current.isValid() ? entryAt(current.row()) : nullptr;
                if (!m_updating && entry != nullptr) {
                    emit entryActivated(*entry);
                }
            });
    connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (const FigureEntry* entry = entryAt(index.row())) {
            emit openRequested(*entry);
        }
    });
    layout->addWidget(m_list, 1);

    m_empty = new QLabel(this);
    m_empty->setObjectName(QStringLiteral("figuresEmpty"));
    m_empty->setWordWrap(true);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_empty);

    applySettings();
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &FiguresPanel::applySettings);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            &FiguresPanel::applySettings);
    // The settings of the cards; the listener runs on the thread that changes them
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key == PICTURES_SETTING || key == LINES_SETTING) {
                QMetaObject::invokeMethod(this, &FiguresPanel::applySettings,
                                          Qt::QueuedConnection);
            }
        });
    rebuild();
}

FiguresPanel::~FiguresPanel()
{
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

void FiguresPanel::setPictureSource(PictureSource source)
{
    m_pictureSource = std::move(source);
    m_pictures.clear();
    m_list->viewport()->update();
}

void FiguresPanel::setEntries(const std::vector<FigureEntry>& entries)
{
    if (entries == m_entries) {
        return;
    }
    // The rows name the figures given before: the selected one is found before they go
    const QString kept = selectedKey();
    m_entries = entries;
    m_rowEntries.clear();
    if (m_pictures.size() > MAX_KEPT_PICTURES) {
        m_pictures.clear();
    }
    rebuild(kept);
}

std::vector<FigureEntry> FiguresPanel::shownEntries() const
{
    std::vector<FigureEntry> shown;
    for (const int entry : m_rowEntries) {
        if (entry >= 0) {
            shown.push_back(m_entries.at(static_cast<size_t>(entry)));
        }
    }
    return shown;
}

void FiguresPanel::setScope(FigureScope scope)
{
    if (scope == FigureScope::Book && !m_bookAvailable) {
        scope = FigureScope::Text;
    }
    m_scope = scope;
    (scope == FigureScope::Book ? m_bookScope : m_textScope)->setChecked(true);
    rebuild();
}

void FiguresPanel::setBookScopeAvailable(bool available)
{
    m_bookAvailable = available;
    m_bookScope->setEnabled(available);
    if (!available && m_scope == FigureScope::Book) {
        setScope(FigureScope::Text);
    }
}

void FiguresPanel::setDocumentState(bool inFront)
{
    m_inFront = inFront;
    updateEmptyText(!m_rowEntries.empty());
}

void FiguresPanel::setTextName(const QString& name)
{
    m_textName = name;
    updateScopeText();
}

void FiguresPanel::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
}

void FiguresPanel::setKinds(FigureKinds kinds)
{
    m_kinds = kinds;
    m_kindButtons.at(static_cast<size_t>(kinds))->setChecked(true);
    rebuild();
}

QString FiguresPanel::searchText() const
{
    return m_search->text();
}

void FiguresPanel::setSearchText(const QString& text)
{
    m_search->setText(text);
}

void FiguresPanel::updateScopeText()
{
    //: The scope of the panel Images and Tables: the figures of the text in front. In Polish:
    //: Tekst
    const QString name = m_textName.isEmpty() ? tr("Text") : m_textName;
    // A long name ("Copyright page") does not widen the narrow panel: it is cut, and the
    // tooltip has all of it
    const QFontMetrics metrics(m_textScope->font());
    const QString shown =
        metrics.elidedText(name, Qt::ElideRight, metrics.averageCharWidth() * MAX_SCOPE_CHARS);
    m_textScope->setText(shown);
    m_textScope->setToolTip(shown == name ? QString() : name);
}

void FiguresPanel::rebuild()
{
    rebuild(selectedKey());
}

void FiguresPanel::rebuild(const QString& kept)
{
    m_updating = true;
    m_model->clear();
    m_rowEntries.clear();
    const QString search = m_search->text().trimmed();
    QString heading;
    bool first = true;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const FigureEntry& entry = m_entries.at(i);
        if (!passes(entry, m_kinds, search)) {
            continue;
        }
        if (m_scope == FigureScope::Book && (first || entry.elementId != heading)) {
            heading = entry.elementId;
            auto* title = new QStandardItem(entry.textTitle);
            title->setData(true, HEADING_ROLE);
            title->setFlags(Qt::NoItemFlags);  // the arrows go over it
            m_model->appendRow(title);
            m_rowEntries.push_back(-1);
        }
        first = false;
        const QString title = titleOf(entry);
        const QString text = textOf(entry);
        // What a screen reader reads of the card
        auto* item = new QStandardItem(title + QStringLiteral(": ") + text);
        item->setData(false, HEADING_ROLE);
        item->setData(static_cast<int>(entry.figure.kind), KIND_ROLE);
        item->setData(title, TITLE_ROLE);
        item->setData(text, TEXT_ROLE);
        item->setData(entry.figure.captioned && !entry.figure.caption.isEmpty(), CAPTIONED_ROLE);
        item->setData(entry.figure.rows, ROWS_ROLE);
        item->setData(entry.figure.columns, COLUMNS_ROLE);
        item->setToolTip(entry.figure.captioned
                             ? entry.figure.whole + QLatin1Char('\n') + detailOf(entry)
                             : text);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_model->appendRow(item);
        m_rowEntries.push_back(static_cast<int>(i));
    }
    // The figure selected before stays selected, without going to it again
    for (size_t row = 0; row < m_rowEntries.size(); ++row) {
        const FigureEntry* entry = entryAt(static_cast<int>(row));
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

void FiguresPanel::updateCounts()
{
    const QString search = m_search->text().trimmed();
    std::array<int, 3> counts{};
    for (const FigureEntry& entry : m_entries) {
        if (passes(entry, FigureKinds::All, search)) {
            ++counts[0];
            ++counts[isImage(entry) ? 1 : 2];
        }
    }
    const std::array<QString, 3> names{
        //: The button of the panel Images and Tables that shows every figure. In Polish:
        //: Wszystkie
        tr("All"),
        //: The button of the panel Images and Tables that shows the images. In Polish: Obrazy
        tr("Images"),
        //: The button of the panel Images and Tables that shows the tables. In Polish: Tabele
        tr("Tables")};
    for (size_t i = 0; i < m_kindButtons.size(); ++i) {
        //: A button of the kinds of the panel Images and Tables with its count: "Images (4)"
        m_kindButtons.at(i)->setText(tr("%1 (%2)").arg(names.at(i)).arg(counts.at(i)));
        m_kindButtons.at(i)->setAccessibleName(names.at(i));
    }
}

void FiguresPanel::updateEmptyText(bool anyShown)
{
    m_list->setVisible(anyShown);
    m_empty->setVisible(!anyShown);
    if (anyShown) {
        return;
    }
    QString text;
    if (m_scope == FigureScope::Text && !m_inFront) {
        text = m_bookAvailable
                   //: In Polish: Żaden tekst nie jest otwarty. Otwórz tekst albo wybierz
                   //: „Książka”, aby zobaczyć obrazy i tabele całej książki.
                   ? tr("No text is open. Open a text, or choose Book to see the images and "
                        "tables of the whole book.")
                   //: In Polish: Żaden tekst nie jest otwarty.
                   : tr("No text is open.");
    } else if (m_entries.empty()) {
        text = m_scope == FigureScope::Book
                   //: In Polish: Książka nie ma jeszcze obrazów ani tabel.
                   ? tr("The book has no images or tables yet.")
                   //: In Polish: Ten tekst nie ma jeszcze obrazów ani tabel: dodają je Wstaw >
                   //: Obraz... i Wstaw > Tabela...
                   : tr("This text has no images or tables yet: Insert > Image... and Insert > "
                        "Table... add them.");
    } else if (!m_search->text().trimmed().isEmpty()) {
        //: In Polish: Żaden podpis ani nazwa nie zawiera „%1”.
        text = tr("No caption or name contains “%1”.").arg(m_search->text().trimmed());
    } else if (m_kinds == FigureKinds::Images) {
        //: In Polish: Brak obrazów.
        text = tr("No images.");
    } else {
        //: In Polish: Brak tabel.
        text = tr("No tables.");
    }
    m_empty->setText(text);
}

const FigureEntry* FiguresPanel::entryAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_rowEntries.size())) {
        return nullptr;
    }
    const int entry = m_rowEntries.at(static_cast<size_t>(row));
    return entry >= 0 ? &m_entries.at(static_cast<size_t>(entry)) : nullptr;
}

const FigureEntry* FiguresPanel::selectedEntry() const
{
    const QModelIndex current = m_list->currentIndex();
    return current.isValid() ? entryAt(current.row()) : nullptr;
}

void FiguresPanel::selectEntry(const QString& key)
{
    for (size_t row = 0; row < m_rowEntries.size(); ++row) {
        const FigureEntry* entry = entryAt(static_cast<int>(row));
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

QString FiguresPanel::selectedKey() const
{
    const FigureEntry* entry = selectedEntry();
    return entry != nullptr ? entry->key() : QString();
}

void FiguresPanel::focusList(const QString& key)
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

bool FiguresPanel::hasFocusInside() const
{
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && (focus == this || isAncestorOf(focus));
}

QColor FiguresPanel::kindColor(editor::FigureKind kind) const
{
    return m_kindColors.at(kind == editor::FigureKind::Table ? 1 : 0);
}

QImage FiguresPanel::pictureOf(const FigureEntry& entry, const QSize& pixels)
{
    if (!m_showPictures || !m_pictureSource || !isImage(entry) || pixels.isEmpty()) {
        return {};
    }
    // The same image in the same look is drawn once
    const QString key = entry.elementId + QLatin1Char('|') + entry.figure.image.source +
                        QLatin1Char('|') + editor::pictureLookKey(entry.figure.image) +
                        QLatin1Char('|') + QString::number(pixels.width()) + QLatin1Char('x') +
                        QString::number(pixels.height());
    if (const auto found = m_pictures.constFind(key); found != m_pictures.constEnd()) {
        return found.value();
    }
    QImage picture = m_pictureSource(entry, pixels);
    m_pictures.insert(key, picture);
    return picture;
}

QAbstractButton* FiguresPanel::kindButton(FigureKinds kinds) const
{
    return m_kindButtons.at(static_cast<size_t>(kinds));
}

QAbstractButton* FiguresPanel::scopeButton(FigureScope scope) const
{
    return scope == FigureScope::Book ? m_bookScope : m_textScope;
}

void FiguresPanel::applySettings()
{
    const auto& themes = core::ThemeManager::getInstance();
    m_kindColors = {themes.editorColor(IMAGE_COLOR_KEY, palette().color(QPalette::Highlight)),
                    themes.editorColor(TABLE_COLOR_KEY, palette().color(QPalette::Link))};
    auto& settings = core::SettingsManager::getInstance();
    m_showPictures = settings.get<bool>(PICTURES_SETTING, true);
    auto* delegate = static_cast<CardDelegate*>(m_list->itemDelegate());
    delegate->look.pictures = m_showPictures;
    delegate->look.lines =
        std::clamp(settings.get<int>(LINES_SETTING, DEFAULT_LINES), MIN_LINES, MAX_LINES);
    delegate->look.kinds = m_kindColors;
    // The rows take their new heights
    m_list->doItemsLayout();
    m_list->viewport()->update();
}

// =============================================================================
// Keys and menus
// =============================================================================

bool FiguresPanel::eventFilter(QObject* watched, QEvent* event)
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

bool FiguresPanel::listKeyPressed(const QKeyEvent* event)
{
    const FigureEntry* entry = selectedEntry();
    const bool plain = (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (plain && entry != nullptr) {
            emit openRequested(*entry);
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

void FiguresPanel::keyPressEvent(QKeyEvent* event)
{
    // Esc from anywhere in the panel: back to the text
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        emit editorFocusRequested();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

QMenu* FiguresPanel::createMenu(const QString& key)
{
    const FigureEntry* found = nullptr;
    for (size_t row = 0; row < m_rowEntries.size() && found == nullptr; ++row) {
        const FigureEntry* entry = entryAt(static_cast<int>(row));
        if (entry != nullptr && entry->key() == key) {
            found = entry;
        }
    }
    if (found == nullptr) {
        return nullptr;
    }
    const FigureEntry entry = *found;
    const bool image = isImage(entry);
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("figuresMenu"));
    QString goText;
    QString propertiesText;
    if (image) {
        //: The menu of an image in the panel Images and Tables. In Polish: &Przejdź do obrazu
        goText = tr("&Go to Image");
        //: In Polish: &Właściwości obrazu...
        propertiesText = tr("Image &Properties...");
    } else {
        //: The menu of a table in the panel Images and Tables. In Polish: &Przejdź do tabeli
        goText = tr("&Go to Table");
        //: In Polish: &Właściwości tabeli...
        propertiesText = tr("Table &Properties...");
    }
    menu->addAction(goText, this, [this, entry]() { emit openRequested(entry); });
    QAction* properties = menu->addAction(propertiesText, this, [this, entry]() {
        emit actionRequested(entry, FigureAction::Properties);
    });
    properties->setEnabled(!m_readOnly);
    QString captionText;
    if (entry.figure.captioned) {
        //: The window of the caption of a figure that has one. In Polish: Zmień po&dpis...
        captionText = tr("Edit &Caption...");
    } else {
        //: In Polish: Wstaw po&dpis...
        captionText = tr("Insert &Caption...");
    }
    QAction* caption = menu->addAction(captionText, this, [this, entry]() {
        emit actionRequested(entry, FigureAction::Caption);
    });
    caption->setEnabled(!m_readOnly);
    if (entry.figure.captioned) {
        //: Copies the whole caption of a figure: "Figure 3. The bridge". In Polish: &Kopiuj
        //: podpis
        menu->addAction(tr("Cop&y Caption"), this, [entry]() {
            QGuiApplication::clipboard()->setText(entry.figure.whole);
        });
    }
    return menu;
}

void FiguresPanel::showMenu(const QPoint& globalPos)
{
    const FigureEntry* entry = selectedEntry();
    if (entry == nullptr) {
        return;
    }
    const std::unique_ptr<QMenu> menu(createMenu(entry->key()));
    if (menu) {
        menu->exec(globalPos);
    }
}

}  // namespace kalahari::gui
