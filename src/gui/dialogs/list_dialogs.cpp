/// @file list_dialogs.cpp
/// @brief The windows of the lists: Bullets and Numbering, Set Numbering Value

#include "kalahari/gui/dialogs/list_dialogs.h"

#include "kalahari/editor/paragraph_format.h"
#include "kalahari/gui/dialogs/special_character_dialog.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace kalahari {
namespace gui {
namespace dialogs {

using editor::ListDefinition;
using editor::ListKind;
using editor::ListLevel;
using editor::ListMarkerAlignment;
using editor::ListNumberFormat;

namespace {

/// @brief Room between the columns of a tab and between its fields
constexpr int COLUMN_SPACING = 16;
constexpr int FIELD_SPACING = 6;

/// @brief The preview shows a point of the list as large as this share of a point on paper
constexpr qreal PREVIEW_SCALE = 0.75;

/// @brief Margin of the preview around its text, and room between a number and its text
constexpr int PREVIEW_MARGIN = 10;
constexpr int PREVIEW_MARK_SPACE = 4;

/// @brief The font of the preview and of the tiles: the serif one of a book
QFont serifFont(const QFont& base)
{
    QFont font = base;
    font.setFamilies({QStringLiteral("Times New Roman"), QStringLiteral("Liberation Serif")});
    font.setStyleHint(QFont::Serif);
    return font;
}

/// @brief The numbers offered on the tab Numbering: their format, suffix and name
struct NumberTile {
    ListNumberFormat format;
    const char* suffix;
    const char* name;
};
constexpr std::array<NumberTile, 6> NUMBER_TILES = {{
    {ListNumberFormat::Decimal, ".", "1. 2. 3."},
    {ListNumberFormat::Decimal, ")", "1) 2) 3)"},
    {ListNumberFormat::LowerAlpha, ")", "a) b) c)"},
    {ListNumberFormat::UpperAlpha, ".", "A. B. C."},
    {ListNumberFormat::LowerRoman, ".", "i. ii. iii."},
    {ListNumberFormat::UpperRoman, ".", "I. II. III."},
}};

/// @brief The bullets offered on the tab Bullets, with their names
QList<std::pair<QChar, QString>> bulletTiles()
{
    return {{QChar(editor::BULLET_DISC), BulletsNumberingDialog::tr("Disc")},
            {QChar(editor::BULLET_CIRCLE), BulletsNumberingDialog::tr("Circle")},
            {QChar(editor::BULLET_SQUARE), BulletsNumberingDialog::tr("Square")},
            {QChar(editor::BULLET_DASH), BulletsNumberingDialog::tr("En dash")},
            {QChar(editor::BULLET_CHECK), BulletsNumberingDialog::tr("Check mark")},
            {QChar(editor::BULLET_ARROW), BulletsNumberingDialog::tr("Arrow")}};
}

/// @brief A tile of a format: three items with their numbers or bullets
class ListFormatTile : public QAbstractButton {
public:
    ListFormatTile(const ListLevel& level, const QString& name, QWidget* parent)
        : QAbstractButton(parent)
        , m_level(level)
    {
        setCheckable(true);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(name);
        setToolTip(name);
    }

    [[nodiscard]] const ListLevel& level() const { return m_level; }

    QSize sizeHint() const override
    {
        const int line = fontMetrics().height();
        return {qRound(line * 5.6), qRound(line * 3.8)};
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        const QPalette& colors = palette();
        painter.fillRect(rect(), colors.base());
        if (isChecked()) {
            painter.setPen(QPen(colors.highlight().color(), 2));
            painter.drawRect(QRectF(rect()).adjusted(1, 1, -1, -1));
        } else {
            painter.setPen(colors.mid().color());
            painter.drawRect(rect().adjusted(0, 0, -1, -1));
        }
        if (hasFocus()) {
            QStyleOptionFocusRect focus;
            focus.initFrom(this);
            focus.rect = rect().adjusted(4, 4, -4, -4);
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
        }

        // Three lines, each with its number or bullet and a bar for the text
        const QFont font = serifFont(this->font());
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const int markWidth = metrics.horizontalAdvance(QStringLiteral("III."));
        const int margin = height() / 8;
        for (int line = 0; line < 3; ++line) {
            const int centre = (line + 1) * height() / 4;
            const QRect mark(margin, centre - metrics.height() / 2, markWidth, metrics.height());
            painter.setPen(colors.text().color());
            painter.drawText(mark, Qt::AlignRight | Qt::AlignVCenter,
                             editor::listMarkerText(m_level, line + 1));
            const int barLeft = mark.right() + margin;
            painter.fillRect(QRect(barLeft, centre - 1, width() - barLeft - margin, 2), colors.mid());
        }
    }

private:
    ListLevel m_level;
};

/// @brief The texts of the preview's items: of a list of steps or of things
QStringList previewTexts(bool numbered)
{
    if (numbered) {
        return {BulletsNumberingDialog::tr("The 6:40 train."),
                BulletsNumberingDialog::tr("Change at Łapy:"),
                BulletsNumberingDialog::tr("ask for Mr Hilary,"),
                BulletsNumberingDialog::tr("do not give the name."),
                BulletsNumberingDialog::tr("The address on Mostowa Street.")};
    }
    return {BulletsNumberingDialog::tr("a coat and a torch,"),
            BulletsNumberingDialog::tr("the key to the locker:"),
            BulletsNumberingDialog::tr("a spare one at aunt Helena's,"),
            BulletsNumberingDialog::tr("another one in the drawer."),
            BulletsNumberingDialog::tr("a return ticket.")};
}

}  // namespace

// =============================================================================
// ListNumberSpinBox
// =============================================================================

ListNumberSpinBox::ListNumberSpinBox(QWidget* parent)
    : QSpinBox(parent)
{
    setRange(editor::MIN_LIST_NUMBER, editor::MAX_LIST_NUMBER);
}

void ListNumberSpinBox::setNumberFormat(ListNumberFormat format)
{
    m_format = format == ListNumberFormat::Bullet ? ListNumberFormat::Decimal : format;
    lineEdit()->setText(textFromValue(value()));
}

QString ListNumberSpinBox::textFromValue(int value) const
{
    return editor::listNumberText(value, m_format);
}

int ListNumberSpinBox::valueFromText(const QString& text) const
{
    return editor::listNumberFromText(text, m_format).value_or(value());
}

QValidator::State ListNumberSpinBox::validate(QString& input, int& /*position*/) const
{
    if (editor::listNumberFromText(input, m_format)) {
        return QValidator::Acceptable;
    }
    // On the way to a number: digits, or the letters of the format
    const QString written = input.trimmed().toLower();
    const bool letters = m_format == ListNumberFormat::LowerAlpha ||
                         m_format == ListNumberFormat::UpperAlpha;
    const bool roman = m_format == ListNumberFormat::LowerRoman ||
                       m_format == ListNumberFormat::UpperRoman;
    const bool plausible = std::all_of(written.begin(), written.end(), [&](QChar character) {
        return character.isDigit() ||
               (letters && character >= QLatin1Char('a') && character <= QLatin1Char('z')) ||
               (roman && QStringLiteral("ivxlcdm").contains(character));
    });
    return plausible ? QValidator::Intermediate : QValidator::Invalid;
}

// =============================================================================
// ListPreview
// =============================================================================

ListPreview::ListPreview(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ListPreview::showList(const ListDefinition& definition, int level)
{
    const int upper = level > 0 ? level - 1 : 0;
    const int lower = level > 0 ? level : 1;
    const ListLevel& top = definition.levels[static_cast<size_t>(upper)];
    const ListLevel& nested = definition.levels[static_cast<size_t>(lower)];
    const QStringList texts = previewTexts(!top.isBullet());
    m_rows = {{editor::listMarkerText(top, top.start), texts.at(0), top},
              {editor::listMarkerText(top, top.start + 1), texts.at(1), top},
              {editor::listMarkerText(nested, nested.start), texts.at(2), nested},
              {editor::listMarkerText(nested, nested.start + 1), texts.at(3), nested},
              {editor::listMarkerText(top, top.start + 2), texts.at(4), top}};
    updateGeometry();
    update();
}

void ListPreview::showItem(const ListLevel& level, int number)
{
    m_rows = {{editor::listMarkerText(level, number), previewTexts(true).at(4), level}};
    updateGeometry();
    update();
}

QStringList ListPreview::shownMarks() const
{
    QStringList marks;
    for (const Row& row : m_rows) {
        marks << row.mark;
    }
    return marks;
}

QSize ListPreview::sizeHint() const
{
    const QFontMetrics metrics(serifFont(font()));
    const int rows = std::max<int>(1, static_cast<int>(m_rows.size()));
    return {metrics.averageCharWidth() * 34,
            2 * PREVIEW_MARGIN + rows * (metrics.lineSpacing() + 2)};
}

void ListPreview::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const QPalette& colors = palette();
    painter.fillRect(rect(), colors.base());
    painter.setPen(colors.mid().color());
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    const QFont font = serifFont(this->font());
    painter.setFont(font);
    const QFontMetrics metrics(font);
    const qreal scale = logicalDpiX() / 72.0 * PREVIEW_SCALE;
    painter.setClipRect(rect().adjusted(1, 1, -1, -1));
    painter.setPen(colors.text().color());

    // Each item as the editor shows it: its number where the level puts it, its text at the
    // text indent, or after the number when the number does not fit before it
    int baseline = PREVIEW_MARGIN + metrics.ascent();
    for (const Row& row : m_rows) {
        const int markerAt = PREVIEW_MARGIN + qRound(row.level.markerIndent * scale);
        const int markWidth = metrics.horizontalAdvance(row.mark);
        int markLeft = markerAt;
        if (row.level.alignment == ListMarkerAlignment::Center) {
            markLeft -= markWidth / 2;
        } else if (row.level.alignment == ListMarkerAlignment::Right) {
            markLeft -= markWidth;
        }
        markLeft = std::max(1, markLeft);
        const int textLeft = std::max(PREVIEW_MARGIN + qRound(row.level.textIndent * scale),
                                      row.mark.isEmpty() ? 0 : markLeft + markWidth + PREVIEW_MARK_SPACE);
        painter.drawText(QPoint(markLeft, baseline), row.mark);
        painter.drawText(QPoint(textLeft, baseline),
                         metrics.elidedText(row.text, Qt::ElideRight,
                                            std::max(0, width() - PREVIEW_MARGIN - textLeft)));
        baseline += metrics.lineSpacing() + 2;
    }
}

// =============================================================================
// BulletsNumberingDialog
// =============================================================================

BulletsNumberingDialog::BulletsNumberingDialog(const ListDefinition& definition, int level,
                                               QWidget* parent)
    : KalahariDialog(parent)
    , m_definition(definition)
    , m_level(std::clamp(level, 0, editor::LIST_LEVELS - 1))
{
    setHeading(tr("Bullets and Numbering"),
               tr("The look of the list in the selected paragraphs; each level has its own "
                  "format."));
    setHeadingIcon(QStringLiteral("format.bulletsNumbering"));
    setAcceptText(tr("OK"));

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createPage(true, m_numbering), tr("&Numbering"));
    m_tabs->addTab(createPage(false, m_bullets), tr("&Bullets"));
    contentLayout()->addWidget(m_tabs, 1);
    m_tabs->setCurrentIndex(m_definition.levels[static_cast<size_t>(m_level)].isBullet() ? 1 : 0);

    // The other tab: the level gets the numbers or the bullet a new list has on it
    connect(m_tabs, &QTabWidget::currentChanged, this, [this]() {
        if (m_refreshing) {
            return;
        }
        const bool numbering = isNumberingShown();
        if (m_definition.levels[static_cast<size_t>(m_level)].isBullet() == numbering) {
            changeLevel([this, numbering](ListLevel& shown) {
                const ListLevel other = ListDefinition::defaultLevel(
                    numbering ? ListKind::Numbered : ListKind::Bulleted, m_level);
                shown.format = other.format;
                shown.suffix = other.suffix;
                shown.bullet = other.bullet;
            });
        }
    });

    QPushButton* restore = addSideButton(tr("&Restore Defaults"));
    connect(restore, &QPushButton::clicked, this, [this]() {
        m_definition =
            ListDefinition::ofKind(isNumberingShown() ? ListKind::Numbered : ListKind::Bulleted);
        refresh();
    });

    // A bullet of the Special Character window
    m_chooseCharacter = [this]() -> std::optional<QString> {
        SpecialCharacterDialog window({}, {}, {}, this);
        window.setChoosing();
        std::optional<QString> chosen;
        connect(&window, &SpecialCharacterDialog::insertRequested, &window,
                [&chosen](char32_t code) { chosen = QString::fromUcs4(&code, 1); });
        window.exec();
        return chosen;
    };

    refresh();
}

QWidget* BulletsNumberingDialog::createPage(bool numbering, Page& page)
{
    auto* tab = new QWidget(m_tabs);
    auto* columns = new QGridLayout(tab);
    columns->setHorizontalSpacing(COLUMN_SPACING);
    columns->setVerticalSpacing(FIELD_SPACING);

    // The levels
    page.levels = new QListWidget(tab);
    for (int level = 1; level <= editor::LIST_LEVELS; ++level) {
        page.levels->addItem(QString::number(level));
    }
    page.levels->setFixedWidth(page.levels->fontMetrics().horizontalAdvance(QStringLiteral("9")) * 3 +
                               page.levels->frameWidth() * 2 + 2 * FIELD_SPACING);
    page.levels->setFixedHeight(page.levels->sizeHintForRow(0) * editor::LIST_LEVELS +
                                page.levels->frameWidth() * 2);
    page.levels->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    page.levels->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* levelLabel = new QLabel(tr("&Level"), tab);
    levelLabel->setBuddy(page.levels);
    columns->addWidget(levelLabel, 0, 0);
    columns->addWidget(page.levels, 1, 0, Qt::AlignTop);
    connect(page.levels, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_refreshing && row >= 0) {
            setLevel(row);
        }
    });

    // The tiles of the formats, and what the format has besides
    auto* formatColumn = new QVBoxLayout();
    formatColumn->setSpacing(FIELD_SPACING);
    auto* tiles = new QGridLayout();
    tiles->setSpacing(FIELD_SPACING + 2);
    page.tiles = new QButtonGroup(tab);
    page.tiles->setExclusive(true);
    if (numbering) {
        for (size_t i = 0; i < NUMBER_TILES.size(); ++i) {
            ListLevel format;
            format.format = NUMBER_TILES[i].format;
            format.suffix = QString::fromLatin1(NUMBER_TILES[i].suffix);
            auto* tile = new ListFormatTile(format, QString::fromLatin1(NUMBER_TILES[i].name), tab);
            page.tiles->addButton(tile, static_cast<int>(i));
            tiles->addWidget(tile, static_cast<int>(i / 3), static_cast<int>(i % 3));
        }
    } else {
        const QList<std::pair<QChar, QString>> bullets = bulletTiles();
        for (qsizetype i = 0; i < bullets.size(); ++i) {
            ListLevel format;
            format.format = ListNumberFormat::Bullet;
            format.bullet = bullets.at(i).first;
            auto* tile = new ListFormatTile(format, bullets.at(i).second, tab);
            page.tiles->addButton(tile, static_cast<int>(i));
            tiles->addWidget(tile, static_cast<int>(i / 3), static_cast<int>(i % 3));
        }
    }
    auto* formatLabel = new QLabel(numbering ? tr("&Format") : tr("B&ullet"), tab);
    formatLabel->setBuddy(page.tiles->button(0));
    columns->addWidget(formatLabel, 0, 1);
    formatColumn->addLayout(tiles);
    connect(page.tiles, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* button) {
        const ListLevel& chosen = static_cast<ListFormatTile*>(button)->level();
        changeLevel([&chosen](ListLevel& shown) {
            shown.format = chosen.format;
            shown.suffix = chosen.isBullet() ? QStringLiteral(".") : chosen.suffix;
            shown.bullet = chosen.bullet;
        });
    });

    auto* fields = new QGridLayout();
    fields->setHorizontalSpacing(FIELD_SPACING + 2);
    fields->setVerticalSpacing(FIELD_SPACING);
    if (numbering) {
        page.start = new ListNumberSpinBox(tab);
        auto* startLabel = new QLabel(tr("&Start at"), tab);
        startLabel->setBuddy(page.start);
        fields->addWidget(startLabel, 0, 0);
        fields->addWidget(page.start, 0, 1, Qt::AlignLeft);
        connect(page.start, &QSpinBox::valueChanged, this, [this](int start) {
            if (!m_refreshing) {
                changeLevel([start](ListLevel& shown) { shown.start = start; });
            }
        });

        page.alignment = new QComboBox(tab);
        page.alignment->addItem(tr("Left"), static_cast<int>(ListMarkerAlignment::Left));
        page.alignment->addItem(tr("Center"), static_cast<int>(ListMarkerAlignment::Center));
        page.alignment->addItem(tr("Right"), static_cast<int>(ListMarkerAlignment::Right));
        auto* alignmentLabel = new QLabel(tr("Number &alignment"), tab);
        alignmentLabel->setBuddy(page.alignment);
        fields->addWidget(alignmentLabel, 1, 0);
        fields->addWidget(page.alignment, 1, 1, Qt::AlignLeft);
        connect(page.alignment, &QComboBox::currentIndexChanged, this, [this, &page](int index) {
            if (!m_refreshing && index >= 0) {
                const auto alignment =
                    static_cast<ListMarkerAlignment>(page.alignment->itemData(index).toInt());
                changeLevel([alignment](ListLevel& shown) { shown.alignment = alignment; });
            }
        });
    } else {
        auto* characterButton = new QPushButton(tr("&Character..."), tab);
        characterButton->setAutoDefault(false);
        connect(characterButton, &QPushButton::clicked, this,
                &BulletsNumberingDialog::chooseCharacter);
        page.character = new QLabel(tr("any character of the Special Character window"), tab);
        page.character->setWordWrap(true);
        page.character->setForegroundRole(QPalette::PlaceholderText);
        fields->addWidget(characterButton, 0, 0, Qt::AlignLeft | Qt::AlignTop);
        fields->addWidget(page.character, 0, 1);
    }
    fields->setColumnStretch(1, 1);
    formatColumn->addLayout(fields);
    formatColumn->addStretch(1);
    columns->addLayout(formatColumn, 1, 1);

    // Where the number and the text stand, and the preview
    auto* placeColumn = new QGridLayout();
    placeColumn->setHorizontalSpacing(FIELD_SPACING + 2);
    placeColumn->setVerticalSpacing(FIELD_SPACING);
    page.markerIndent = new LengthSpinBox(LengthUnit::Points, 0.0, editor::MAX_PARAGRAPH_INDENT, tab);
    page.textIndent = new LengthSpinBox(LengthUnit::Points, 0.0, editor::MAX_PARAGRAPH_INDENT, tab);
    auto* markerLabel = new QLabel(numbering ? tr("Number &indent") : tr("Bullet &indent"), tab);
    markerLabel->setBuddy(page.markerIndent);
    auto* textLabel = new QLabel(tr("&Text indent"), tab);
    textLabel->setBuddy(page.textIndent);
    placeColumn->addWidget(markerLabel, 0, 0);
    placeColumn->addWidget(page.markerIndent, 0, 1, Qt::AlignLeft);
    placeColumn->addWidget(textLabel, 1, 0);
    placeColumn->addWidget(page.textIndent, 1, 1, Qt::AlignLeft);
    connect(page.markerIndent, &QDoubleSpinBox::valueChanged, this, [this, &page]() {
        if (!m_refreshing) {
            const qreal indent = page.markerIndent->storedValue();
            changeLevel([indent](ListLevel& shown) { shown.markerIndent = indent; });
        }
    });
    connect(page.textIndent, &QDoubleSpinBox::valueChanged, this, [this, &page]() {
        if (!m_refreshing) {
            const qreal indent = page.textIndent->storedValue();
            changeLevel([indent](ListLevel& shown) { shown.textIndent = indent; });
        }
    });
    placeColumn->addWidget(new QLabel(tr("Preview"), tab), 2, 0, 1, 2);
    page.preview = new ListPreview(tab);
    placeColumn->addWidget(page.preview, 3, 0, 1, 2);
    placeColumn->setRowStretch(4, 1);
    columns->addLayout(placeColumn, 0, 2, 2, 1);
    columns->setColumnStretch(2, 1);
    columns->setRowStretch(1, 1);
    return tab;
}

bool BulletsNumberingDialog::isNumberingShown() const
{
    return m_tabs->currentIndex() == 0;
}

void BulletsNumberingDialog::showNumbering(bool numbering)
{
    m_tabs->setCurrentIndex(numbering ? 0 : 1);
}

void BulletsNumberingDialog::setLevel(int level)
{
    m_level = std::clamp(level, 0, editor::LIST_LEVELS - 1);
    refresh();
}

void BulletsNumberingDialog::setCharacterChooser(CharacterChooser chooser)
{
    m_chooseCharacter = std::move(chooser);
}

void BulletsNumberingDialog::changeLevel(const std::function<void(ListLevel&)>& change)
{
    change(m_definition.levels[static_cast<size_t>(m_level)]);
    refresh();
}

void BulletsNumberingDialog::chooseCharacter()
{
    if (!m_chooseCharacter) {
        return;
    }
    if (const std::optional<QString> character = m_chooseCharacter();
        character && !character->isEmpty()) {
        const QString bullet = *character;
        changeLevel([&bullet](ListLevel& shown) {
            shown.format = ListNumberFormat::Bullet;
            shown.bullet = bullet;
        });
    }
}

void BulletsNumberingDialog::refresh()
{
    m_refreshing = true;
    const ListLevel& shown = m_definition.levels[static_cast<size_t>(m_level)];
    m_tabs->setCurrentIndex(shown.isBullet() ? 1 : 0);
    for (Page* page : {&m_numbering, &m_bullets}) {
        page->levels->setCurrentRow(m_level);
        // The tile of the level's format; none for a format of no tile
        const QList<QAbstractButton*> buttons = page->tiles->buttons();
        QAbstractButton* matching = nullptr;
        for (QAbstractButton* button : buttons) {
            const ListLevel& format = static_cast<ListFormatTile*>(button)->level();
            if (format.format == shown.format &&
                (shown.isBullet() ? format.bullet == shown.bullet : format.suffix == shown.suffix)) {
                matching = button;
            }
        }
        page->tiles->setExclusive(false);
        for (QAbstractButton* button : buttons) {
            button->setChecked(button == matching);
        }
        page->tiles->setExclusive(true);
        page->markerIndent->setStoredValue(shown.markerIndent);
        page->textIndent->setStoredValue(shown.textIndent);
        page->preview->showList(m_definition, m_level);
    }
    m_numbering.start->setNumberFormat(shown.format);
    m_numbering.start->setValue(shown.start);
    m_numbering.alignment->setCurrentIndex(
        m_numbering.alignment->findData(static_cast<int>(shown.alignment)));
    // A bullet of no tile is named beside Character...
    const bool ownBullet = shown.isBullet() && m_bullets.tiles->checkedButton() == nullptr;
    m_bullets.character->setText(
        ownBullet ? tr("The bullet: %1").arg(shown.bullet)
                  : tr("any character of the Special Character window"));
    m_refreshing = false;
}

// =============================================================================
// NumberingValueDialog
// =============================================================================

NumberingValueDialog::NumberingValueDialog(const ListLevel& level, int number, QWidget* parent)
    : KalahariDialog(parent)
    , m_level(level)
{
    setHeading(tr("Set Numbering Value"),
               tr("The number of the list item; the items after it go on from it."));
    setHeadingIcon(QStringLiteral("format.numbering"));
    setAcceptText(tr("OK"));

    m_value = new ListNumberSpinBox(this);
    m_value->setNumberFormat(level.format);
    m_value->setValue(number);
    addField(tr("Set &value to"), m_value);
    contentLayout()->addWidget(new QLabel(tr("Preview"), this));
    m_preview = new ListPreview(this);
    m_preview->showItem(m_level, number);
    contentLayout()->addWidget(m_preview);
    contentLayout()->addStretch(1);
    connect(m_value, &QSpinBox::valueChanged, this,
            [this](int value) { m_preview->showItem(m_level, value); });

    m_value->setFocus();
    m_value->selectAll();
}

int NumberingValueDialog::value() const
{
    return m_value->value();
}

void NumberingValueDialog::setValue(int value)
{
    m_value->setValue(value);
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
