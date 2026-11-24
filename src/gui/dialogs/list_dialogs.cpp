// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file list_dialogs.cpp
/// @brief The windows of the lists: Bullets and Numbering, Set Numbering Value

#include "kalahari/gui/dialogs/list_dialogs.h"

#include "kalahari/core/project_manager.h"
#include "kalahari/editor/paragraph_format.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/special_character_dialog.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari/gui/widgets/list_set_tile.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari::gui::dialogs {

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

    [[nodiscard]] QSize sizeHint() const override
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
        const QFont font = listPictureFont(this->font());
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

/// @brief The tabs of the window, in their order
constexpr int SETS_TAB = 0;
constexpr int NUMBERING_TAB = 1;
constexpr int BULLETS_TAB = 2;

/// @brief The tiles of the sets in a row
constexpr int SET_COLUMNS = 3;

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
    // The items of the level below under the second item of the level above
    const auto topMark = [&](int number) {
        return editor::listMarkerText(definition, upper, firstListNumbers(definition, upper, number));
    };
    const auto nestedMark = [&](int number) {
        editor::ListPath path = firstListNumbers(definition, lower, number);
        path[static_cast<size_t>(upper)] = top.start + 1;
        return editor::listMarkerText(definition, lower, path);
    };
    m_rows = {{topMark(top.start), texts.at(0), top},
              {topMark(top.start + 1), texts.at(1), top},
              {nestedMark(nested.start), texts.at(2), nested},
              {nestedMark(nested.start + 1), texts.at(3), nested},
              {topMark(top.start + 2), texts.at(4), top}};
    updateGeometry();
    update();
}

void ListPreview::showLevels(const ListDefinition& definition)
{
    // 1., a), i., ii., b), 2.: each level under the first item of the one above, then back
    const QStringList texts = previewTexts(!definition.levels[0].isBullet());
    m_rows.clear();
    const auto row = [&](int level, int item, const QString& text) {
        const ListLevel& format = definition.levels[static_cast<size_t>(level)];
        editor::ListPath path = firstListNumbers(definition, level, format.start + item);
        m_rows.append({editor::listMarkerText(definition, level, path), text, format});
    };
    row(0, 0, texts.at(0));
    row(1, 0, texts.at(1));
    row(2, 0, texts.at(2));
    row(2, 1, texts.at(3));
    row(0, 1, texts.at(4));
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
    const QFontMetrics metrics(listPictureFont(font()));
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

    const QFont font = listPictureFont(this->font());
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
// ListSetNameDialog
// =============================================================================

ListSetNameDialog::ListSetNameDialog(const QString& name, bool askPlace, QWidget* parent)
    : KalahariDialog(parent)
{
    if (name.isEmpty()) {
        //: Save as Set... of Format > Bullets and Numbering. In Polish: Zapisz jako zestaw /
        //: Układ wszystkich poziomów tej listy pod własną nazwą.
        setHeading(tr("Save as Set"), tr("The formats of all the levels of this list, under a "
                                         "name of yours."));
    } else {
        //: In Polish: Zmień nazwę zestawu
        setHeading(tr("Rename Set"));
    }
    setHeadingIcon(QStringLiteral("format.bulletsNumbering"));
    setAcceptText(name.isEmpty() ? tr("&Save") : tr("&Rename"));

    m_name = new QLineEdit(name, this);
    //: In Polish: np. Konspekt rozdziału
    m_name->setPlaceholderText(tr("e.g. Chapter Outline"));
    addField(tr("&Name"), m_name);
    if (askPlace) {
        //: Where a set of list formats is kept. In Polish: W tej &książce (przenosi się z nią na
        //: inny komputer)
        m_inBook = new QRadioButton(tr("In this &book (it goes with the book to another computer)"), this);
        //: In Polish: W &programie (dla wszystkich książek)
        m_inProgram = new QRadioButton(tr("In the &program (for every book)"), this);
        m_inBook->setChecked(true);
        //: In Polish: Zapisz w
        contentLayout()->addWidget(new QLabel(tr("Keep it"), this));
        contentLayout()->addWidget(m_inBook);
        contentLayout()->addWidget(m_inProgram);
    }
    contentLayout()->addStretch(1);
    const auto check = [this]() { acceptButton()->setEnabled(!m_name->text().trimmed().isEmpty()); };
    connect(m_name, &QLineEdit::textChanged, this, check);
    check();
    m_name->setFocus();
    m_name->selectAll();
}

QString ListSetNameDialog::name() const
{
    return m_name->text().trimmed();
}

ListSetPlace ListSetNameDialog::place() const
{
    return m_inBook != nullptr && m_inBook->isChecked() ? ListSetPlace::Book : ListSetPlace::Program;
}

void ListSetNameDialog::setPlace(ListSetPlace place)
{
    if (m_inBook != nullptr) {
        (place == ListSetPlace::Book ? m_inBook : m_inProgram)->setChecked(true);
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
    //: The tooltip of the gear of Format > Bullets and Numbering. In Polish: Ustawienia list
    addSettingsButton(QString::fromLatin1(SettingsDialog::LISTS_PAGE), tr("List Settings"));

    m_tabs = new QTabWidget(this);
    //: The tab of the sets of all the levels of a list. In Polish: Z&estawy
    m_tabs->addTab(createSetsPage(), tr("S&ets"));
    m_tabs->addTab(createPage(true, m_numbering), tr("&Numbering"));
    m_tabs->addTab(createPage(false, m_bullets), tr("&Bullets"));
    contentLayout()->addWidget(m_tabs, 1);
    m_tabs->setCurrentIndex(SETS_TAB);

    // The other tab: the level gets the numbers or the bullet a new list has on it
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int tab) {
        if (m_refreshing || tab == SETS_TAB) {
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
                shown.prefix = other.prefix;
                shown.shownLevels = other.shownLevels;
            });
        }
    });

    QPushButton* restore = addSideButton(tr("&Restore Defaults"));
    connect(restore, &QPushButton::clicked, this, [this]() {
        const ListKind kind = isSetsShown() ? m_definition.kind()
                              : isNumberingShown() ? ListKind::Numbered
                                                   : ListKind::Bulleted;
        m_definition = ListDefinition::ofKind(kind);
        refresh();
    });
    //: Keeps the formats of the window as a set of the writer. In Polish: Zapi&sz jako zestaw...
    QPushButton* save = addSideButton(tr("Sa&ve as Set..."));
    connect(save, &QPushButton::clicked, this, [this]() { saveAsSet(); });

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

    fillSets();
    refresh();
}

QWidget* BulletsNumberingDialog::createSetsPage()
{
    auto* tab = new QWidget(m_tabs);
    auto* columns = new QHBoxLayout(tab);
    columns->setSpacing(COLUMN_SPACING);

    // The groups of the tiles, scrolled down when they are more than the window has room for;
    // as wide as their rows of tiles (fillSets())
    m_setsScroll = new QScrollArea(tab);
    m_setsScroll->setWidgetResizable(true);
    m_setsScroll->setFrameShape(QFrame::NoFrame);
    m_setsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_setsGrid = new QWidget(m_setsScroll);
    m_setsLayout = new QVBoxLayout(m_setsGrid);
    m_setsLayout->setContentsMargins(0, 0, 0, 0);
    m_setsLayout->setSpacing(FIELD_SPACING);
    m_setsScroll->setWidget(m_setsGrid);
    columns->addWidget(m_setsScroll, 3);

    auto* previewColumn = new QVBoxLayout();
    previewColumn->setSpacing(FIELD_SPACING);
    previewColumn->addWidget(new QLabel(tr("Preview"), tab));
    m_setsPreview = new ListPreview(tab);
    previewColumn->addWidget(m_setsPreview);
    //: Under the preview of the tab Sets. In Polish: Kliknięcie albo strzałki wybierają
    //: zestaw dla całej listy, a menu podręczne zmienia jego nazwę, usuwa go albo ustawia jako
    //: domyślny.
    auto* hint = new QLabel(tr("A click or the arrows choose a set for the whole list; its "
                               "context menu renames it, deletes it or makes it the default."),
                            tab);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    previewColumn->addWidget(hint);
    previewColumn->addStretch(1);
    columns->addLayout(previewColumn, 2);
    return tab;
}

void BulletsNumberingDialog::fillSets()
{
    // The groups as they are now: the tiles of all of them made again. The old ones go later
    // (one of them may be showing its menu), hidden at once: out of the layout, they would
    // show in its corner
    m_sets = list_sets::all(m_book);
    while (QLayoutItem* item = m_setsLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        if (QLayout* layout = item->layout()) {
            while (QLayoutItem* inner = layout->takeAt(0)) {
                if (QWidget* widget = inner->widget()) {
                    widget->hide();
                    widget->deleteLater();
                }
                delete inner;
            }
        }
        delete item;
    }
    m_setTiles.clear();

    const QString numberedDefault = list_sets::defaultId(ListKind::Numbered);
    const QString bulletedDefault = list_sets::defaultId(ListKind::Bulleted);
    QString group;
    QGridLayout* grid = nullptr;
    int index = 0;
    for (const ListSet& set : std::as_const(m_sets)) {
        const QString title = list_sets::groupTitle(set);
        if (grid == nullptr || title != group) {
            group = title;
            auto* heading = new QLabel(title, m_setsGrid);
            QFont bold = heading->font();
            bold.setBold(true);
            heading->setFont(bold);
            m_setsLayout->addWidget(heading);
            grid = new QGridLayout();
            grid->setSpacing(FIELD_SPACING + 2);
            m_setsLayout->addLayout(grid);
            index = 0;
        }
        const bool isDefault =
            set.id == (set.kind() == ListKind::Bulleted ? bulletedDefault : numberedDefault);
        auto* tile = new ListSetTile(set, isDefault, m_setsGrid);
        tile->setMove([this](const ListSetTile* from, int key) {
            // The nearest tile in the direction of the arrow, chosen as a radio button is
            if (ListSetTile* best = ListSetTile::nearest(m_setTiles, from, key, m_setsGrid)) {
                best->setFocus(Qt::TabFocusReason);
                chooseSet(best->set().id);
                m_setsScroll->ensureWidgetVisible(best);
            }
        });
        tile->setMenu([this](const QString& id, const QPoint& at) { showSetMenu(id, at); });
        connect(tile, &QAbstractButton::clicked, this, [this, id = set.id]() { chooseSet(id); });
        grid->addWidget(tile, index / SET_COLUMNS, index % SET_COLUMNS);
        ++index;
        m_setTiles.append(tile);
    }
    m_setsLayout->addStretch(1);
    // Wide enough for the rows of tiles and the scroll bar: never scrolled sideways
    m_setsScroll->setMinimumWidth(m_setsLayout->minimumSize().width() +
                                  m_setsScroll->verticalScrollBar()->sizeHint().width());
    refreshSets();
}

void BulletsNumberingDialog::refreshSets()
{
    const QString chosen = chosenSet();
    for (ListSetTile* tile : std::as_const(m_setTiles)) {
        tile->setChecked(tile->set().id == chosen);
    }
    if (m_setsPreview != nullptr) {
        m_setsPreview->showLevels(m_definition);
    }
}

QString BulletsNumberingDialog::chosenSet() const
{
    const std::optional<ListSet> set = list_sets::matching(m_definition, m_sets);
    return set ? set->id : QString();
}

void BulletsNumberingDialog::chooseSet(const QString& id)
{
    for (const ListSet& set : std::as_const(m_sets)) {
        if (set.id == id) {
            m_definition = set.definition;
            refresh();
            return;
        }
    }
}

void BulletsNumberingDialog::setBook(core::ProjectManager* book)
{
    m_book = book != nullptr && book->isProjectOpen() ? book : nullptr;
    fillSets();
}

void BulletsNumberingDialog::setListKeys(const QString& numbering, const QString& bullets)
{
    m_numberingKeys = numbering;
    m_bulletsKeys = bullets;
}

QList<ListSet> BulletsNumberingDialog::setsOf(ListSetPlace place) const
{
    if (place == ListSetPlace::Book) {
        return m_book != nullptr ? list_sets::ofBook(*m_book) : QList<ListSet>{};
    }
    return place == ListSetPlace::Program ? list_sets::program() : QList<ListSet>{};
}

bool BulletsNumberingDialog::keepSets(ListSetPlace place, const QList<ListSet>& sets)
{
    bool kept = true;
    if (place == ListSetPlace::Book) {
        kept = m_book != nullptr && list_sets::setOfBook(*m_book, sets);
    } else if (place == ListSetPlace::Program) {
        list_sets::setProgram(sets);
    }
    if (!kept) {
        //: In Polish: Nie udało się zapisać zestawu w książce.
        MessageDialog::warning(this, tr("Bullets and Numbering"),
                               tr("The set could not be saved in the book."));
    }
    fillSets();
    return kept;
}

bool BulletsNumberingDialog::saveAsSet()
{
    ListSetNameDialog window(QString(), m_book != nullptr && !m_book->isReadOnly(), this);
    if (window.exec() != QDialog::Accepted || window.name().isEmpty()) {
        return false;
    }
    ListSet set;
    set.id = list_sets::newId();
    set.name = window.name();
    set.definition = m_definition;
    set.place = m_book != nullptr && !m_book->isReadOnly() ? window.place() : ListSetPlace::Program;
    QList<ListSet> sets = setsOf(set.place);
    sets.append(set);
    return keepSets(set.place, sets);
}

bool BulletsNumberingDialog::renameSet(const QString& id)
{
    for (const ListSetPlace place : {ListSetPlace::Book, ListSetPlace::Program}) {
        QList<ListSet> sets = setsOf(place);
        for (ListSet& set : sets) {
            if (set.id != id) {
                continue;
            }
            ListSetNameDialog window(set.displayName(), false, this);
            if (window.exec() != QDialog::Accepted || window.name().isEmpty()) {
                return false;
            }
            set.name = window.name();
            return keepSets(place, sets);
        }
    }
    return false;
}

bool BulletsNumberingDialog::deleteSet(const QString& id)
{
    for (const ListSetPlace place : {ListSetPlace::Book, ListSetPlace::Program}) {
        QList<ListSet> sets = setsOf(place);
        for (qsizetype i = 0; i < sets.size(); ++i) {
            if (sets.at(i).id != id) {
                continue;
            }
            //: In Polish: Usunąć zestaw „%1”? Listy w tekstach zostaną takie, jakie są.
            if (!MessageDialog::confirm(this, tr("Delete Set"),
                                        tr("Delete the set \"%1\"? The lists in the texts stay "
                                           "as they are.")
                                            .arg(sets.at(i).displayName()),
                                        tr("&Delete"))) {
                return false;
            }
            sets.removeAt(i);
            return keepSets(place, sets);
        }
    }
    return false;
}

void BulletsNumberingDialog::makeDefault(const QString& id)
{
    for (const ListSet& set : std::as_const(m_sets)) {
        if (set.id == id) {
            list_sets::setDefault(set.kind(), id);
        }
    }
    fillSets();
}

void BulletsNumberingDialog::showSetMenu(const QString& id, const QPoint& globalPos)
{
    const auto found = std::find_if(m_sets.cbegin(), m_sets.cend(),
                                    [&id](const ListSet& set) { return set.id == id; });
    if (found == m_sets.cend()) {
        return;
    }
    const ListSet& set = *found;
    QMenu menu(this);
    const bool own = set.place != ListSetPlace::BuiltIn &&
                     !(set.place == ListSetPlace::Book && (m_book == nullptr || m_book->isReadOnly()));
    if (own) {
        //: In Polish: Zmień &nazwę...
        menu.addAction(tr("Re&name..."), this, [this, id]() { renameSet(id); });
        //: In Polish: &Usuń
        menu.addAction(tr("&Delete"), this, [this, id]() { deleteSet(id); });
        menu.addSeparator();
    }
    const bool bulleted = set.kind() == ListKind::Bulleted;
    //: %1: the keys of Numbering or Bullets. In Polish: Ustaw jako &domyślny dla %1
    QAction* makeDefaultAction = menu.addAction(
        tr("Set as &Default for %1").arg(bulleted ? m_bulletsKeys : m_numberingKeys), this,
        [this, id]() { makeDefault(id); });
    makeDefaultAction->setEnabled(list_sets::defaultId(set.kind()) != id);
    menu.exec(globalPos);
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
        // A tile writes the text around the number itself
        changeLevel([&chosen](ListLevel& shown) {
            shown.format = chosen.format;
            shown.suffix = chosen.isBullet() ? QStringLiteral(".") : chosen.suffix;
            shown.bullet = chosen.bullet;
            shown.prefix.clear();
        });
    });

    auto* fields = new QGridLayout();
    fields->setHorizontalSpacing(FIELD_SPACING + 2);
    fields->setVerticalSpacing(FIELD_SPACING);
    if (numbering) {
        createNumberFields(tab, fields, page);
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

void BulletsNumberingDialog::createNumberFields(QWidget* tab, QGridLayout* fields, Page& page)
{
    // The text before and after the number: any characters, (1), § 1., Step 1:
    page.prefix = new QLineEdit(tab);
    //: In Polish: np. „(”, „§ ”, „Krok ”
    page.prefix->setPlaceholderText(tr("e.g. \"(\", \"§ \", \"Step \""));
    //: In Polish: Te&kst przed numerem
    auto* prefixLabel = new QLabel(tr("Te&xt before number"), tab);
    prefixLabel->setBuddy(page.prefix);
    fields->addWidget(prefixLabel, 0, 0);
    fields->addWidget(page.prefix, 0, 1);
    connect(page.prefix, &QLineEdit::textEdited, this, [this](const QString& text) {
        changeLevel([&text](ListLevel& shown) { shown.prefix = text; });
    });

    page.suffix = new QLineEdit(tab);
    //: In Polish: np. „.”, „)”, „:”, „ –” albo nic
    page.suffix->setPlaceholderText(tr("e.g. \".\", \")\", \":\" or nothing"));
    //: In Polish: Tekst po nu&merze
    auto* suffixLabel = new QLabel(tr("Text after nu&mber"), tab);
    suffixLabel->setBuddy(page.suffix);
    fields->addWidget(suffixLabel, 1, 0);
    fields->addWidget(page.suffix, 1, 1);
    connect(page.suffix, &QLineEdit::textEdited, this, [this](const QString& text) {
        changeLevel([&text](ListLevel& shown) { shown.suffix = text; });
    });

    // The numbers of the levels above it: 1.1., 1.1.1.
    page.shownLevels = new QComboBox(tab);
    //: In Polish: Numer&y wyższych poziomów
    auto* levelsLabel = new QLabel(tr("Numbers of &higher levels"), tab);
    levelsLabel->setBuddy(page.shownLevels);
    fields->addWidget(levelsLabel, 2, 0);
    fields->addWidget(page.shownLevels, 2, 1, Qt::AlignLeft);
    connect(page.shownLevels, &QComboBox::currentIndexChanged, this, [this, &page](int index) {
        if (!m_refreshing && index >= 0) {
            const int shown = page.shownLevels->itemData(index).toInt();
            changeLevel([shown](ListLevel& level) { level.shownLevels = shown; });
        }
    });

    page.start = new ListNumberSpinBox(tab);
    auto* startLabel = new QLabel(tr("&Start at"), tab);
    startLabel->setBuddy(page.start);
    fields->addWidget(startLabel, 3, 0);
    fields->addWidget(page.start, 3, 1, Qt::AlignLeft);
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
    fields->addWidget(alignmentLabel, 4, 0);
    fields->addWidget(page.alignment, 4, 1, Qt::AlignLeft);
    connect(page.alignment, &QComboBox::currentIndexChanged, this, [this, &page](int index) {
        if (!m_refreshing && index >= 0) {
            const auto alignment =
                static_cast<ListMarkerAlignment>(page.alignment->itemData(index).toInt());
            changeLevel([alignment](ListLevel& shown) { shown.alignment = alignment; });
        }
    });
}

bool BulletsNumberingDialog::isNumberingShown() const
{
    return m_tabs->currentIndex() == NUMBERING_TAB;
}

bool BulletsNumberingDialog::isSetsShown() const
{
    return m_tabs->currentIndex() == SETS_TAB;
}

void BulletsNumberingDialog::showNumbering(bool numbering)
{
    m_tabs->setCurrentIndex(numbering ? NUMBERING_TAB : BULLETS_TAB);
}

void BulletsNumberingDialog::showSets()
{
    m_tabs->setCurrentIndex(SETS_TAB);
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
        const QString& bullet = *character;
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
    // The tab Sets stays in front; the others show the level's kind
    if (!isSetsShown()) {
        m_tabs->setCurrentIndex(shown.isBullet() ? BULLETS_TAB : NUMBERING_TAB);
    }
    for (Page* page : {&m_numbering, &m_bullets}) {
        page->levels->setCurrentRow(m_level);
        // The tile of the level's format; none for a format of no tile
        const QList<QAbstractButton*> buttons = page->tiles->buttons();
        QAbstractButton* matching = nullptr;
        for (QAbstractButton* button : buttons) {
            const ListLevel& format = static_cast<ListFormatTile*>(button)->level();
            if (format.format == shown.format &&
                (shown.isBullet() ? format.bullet == shown.bullet
                                  : format.suffix == shown.suffix && shown.prefix.isEmpty())) {
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
    // The texts around the number, not written again while they are typed
    const QString suffix = shown.isBullet() ? QString() : shown.suffix;
    if (m_numbering.prefix->text() != shown.prefix) {
        m_numbering.prefix->setText(shown.prefix);
    }
    if (m_numbering.suffix->text() != suffix) {
        m_numbering.suffix->setText(suffix);
    }
    // None, then the levels right above it: "Level 2", "Levels 1–2" on the third level
    m_numbering.shownLevels->clear();
    //: The numbers of no level above go before the number. In Polish: Brak
    m_numbering.shownLevels->addItem(tr("None"), 0);
    for (int count = 1; count <= m_level; ++count) {
        const int first = m_level - count + 1;
        m_numbering.shownLevels->addItem(
            count == 1
                //: The number of the level right above goes before the number (1.1.). In
                //: Polish: Poziom %1
                ? tr("Level %1").arg(m_level)
                //: The numbers of these levels go before the number (1.1.1.). In Polish:
                //: Poziomy %1–%2
                : tr("Levels %1–%2").arg(first).arg(m_level),
            count);
    }
    m_numbering.shownLevels->setCurrentIndex(
        std::clamp(shown.shownLevels, 0, m_numbering.shownLevels->count() - 1));
    m_numbering.shownLevels->setEnabled(m_level > 0);
    // A bullet of no tile is named beside Character...
    const bool ownBullet = shown.isBullet() && m_bullets.tiles->checkedButton() == nullptr;
    m_bullets.character->setText(
        ownBullet ? tr("The bullet: %1").arg(shown.bullet)
                  : tr("any character of the Special Character window"));
    refreshSets();
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

} // namespace kalahari::gui::dialogs
