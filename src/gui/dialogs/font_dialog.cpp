/// @file font_dialog.cpp
/// @brief Format > Font...

#include "kalahari/gui/dialogs/font_dialog.h"

#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/gui/dialogs/color_palettes.h"
#include "kalahari/gui/widgets/color_menu.h"

#include "kalahari/core/art_provider.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGridLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace kalahari {
namespace gui {
namespace dialogs {

using editor::CharacterFormat;

namespace {

/// @brief Room between the columns of a tab and between its fields
constexpr int COLUMN_SPACING = 16;
constexpr int FIELD_SPACING = 6;

/// @brief The rows a list of fonts, styles or sizes shows
constexpr int LIST_ROWS = 4;

/// @brief The least widths of the lists of fonts, styles and sizes
constexpr int FAMILY_LIST_WIDTH = 200;
constexpr int STYLE_LIST_WIDTH = 130;
constexpr int SIZE_LIST_WIDTH = 64;

/// @brief The sizes a font may have, in points, as in Word
constexpr double MIN_SIZE = 1.0;
constexpr double MAX_SIZE = 1638.0;

/// @brief The largest size the preview draws, in points: a larger one would not fit in it
constexpr qreal PREVIEW_MAX_POINTS = 26.0;

/// @brief Height of the preview
constexpr int PREVIEW_HEIGHT = 56;

/// @brief An index or a raised small capital is drawn at this share of the size, this share
///        of the ascent up or down
constexpr qreal INDEX_SCALE = 0.66;
constexpr qreal SUPERSCRIPT_RISE = 0.4;
constexpr qreal SUBSCRIPT_DROP = 0.2;

/// @brief The letter spacing a change from Normal starts with, in points, as in Word
constexpr double FIRST_SPACING = 1.0;

/// @brief The lengths of the samples: at most this many characters of the selection
constexpr int SAMPLE_LENGTH = 40;

/// @brief Sizes the same to a hundredth of a point
bool sameSize(qreal a, qreal b)
{
    return std::abs(a - b) < 0.005;
}

/// @brief A number of points as the fields show it: "12", "10,5"
QString pointsText(qreal points)
{
    return QLocale().toString(points, 'g', 4);
}

/// @brief The state of a checkbox for a part of the selection: half checked when only a part
///        of it has the format
Qt::CheckState stateOf(std::optional<bool> checked)
{
    if (!checked) {
        return Qt::PartiallyChecked;
    }
    return *checked ? Qt::Checked : Qt::Unchecked;
}

/// @brief A swatch for a color button
QIcon swatchIcon(const QColor& color, const QColor& border, int side)
{
    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    if (color.isValid()) {
        painter.fillRect(pixmap.rect().adjusted(1, 1, -1, -1), color);
    }
    painter.setPen(border);
    painter.drawRect(pixmap.rect().adjusted(1, 1, -2, -2));
    return QIcon(pixmap);
}

/// @brief Calls a function with each new state of a checkbox (Qt 6.7 renamed the signal)
template <typename Function>
void onCheckStateChanged(QCheckBox* box, QObject* context, Function function)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QObject::connect(box, &QCheckBox::checkStateChanged, context, function);
#else
    QObject::connect(box, &QCheckBox::stateChanged, context,
                     [function](int state) { function(static_cast<Qt::CheckState>(state)); });
#endif
}

}  // namespace

// =============================================================================
// FontPreview
// =============================================================================

FontPreview::FontPreview(QWidget* parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::StyledPanel);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setMinimumHeight(PREVIEW_HEIGHT);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void FontPreview::showFormat(const CharacterFormat& format, const QString& sample)
{
    m_format = format;
    m_sample = sample;
    update();
}

QFont FontPreview::sampleFont() const
{
    QFont sampleFont = font();
    if (m_format.family && !m_format.family->isEmpty()) {
        sampleFont.setFamilies({*m_format.family});
    }
    const qreal size = std::min(m_format.size.value_or(sampleFont.pointSizeF()), PREVIEW_MAX_POINTS);
    const bool index = m_format.position && *m_format.position != QTextCharFormat::AlignNormal;
    sampleFont.setPointSizeF(index ? size * INDEX_SCALE : size);
    sampleFont.setBold(m_format.bold.value_or(false));
    sampleFont.setItalic(m_format.italic.value_or(false));
    sampleFont.setUnderline(m_format.underline.value_or(false));
    sampleFont.setStrikeOut(m_format.strikethrough.value_or(false));
    sampleFont.setCapitalization(m_format.capitals.value_or(QFont::MixedCase));
    // The spacing in points, as the editor's layout draws it
    if (m_format.letterSpacing && *m_format.letterSpacing != 0.0) {
        sampleFont.setLetterSpacing(QFont::AbsoluteSpacing,
                                    *m_format.letterSpacing * logicalDpiY() / 72.0);
    }
    return sampleFont;
}

QSize FontPreview::sizeHint() const
{
    return {COLUMN_SPACING * 12, PREVIEW_HEIGHT};
}

void FontPreview::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    QPainter painter(this);
    const QFont shownFont = sampleFont();
    painter.setFont(shownFont);
    const QFontMetricsF metrics(shownFont);
    const QRectF area = contentsRect().adjusted(FIELD_SPACING, 0, -FIELD_SPACING, 0);
    const QString text = metrics.elidedText(m_sample, Qt::ElideRight, area.width());
    const qreal width = metrics.horizontalAdvance(text);

    // The baseline in the middle; an index above or under it
    qreal baseline = area.center().y() + (metrics.ascent() - metrics.descent()) / 2.0;
    if (m_format.position == QTextCharFormat::AlignSuperScript) {
        baseline -= metrics.ascent() * SUPERSCRIPT_RISE / INDEX_SCALE;
    } else if (m_format.position == QTextCharFormat::AlignSubScript) {
        baseline += metrics.ascent() * SUBSCRIPT_DROP / INDEX_SCALE;
    }
    const QPointF start(area.center().x() - width / 2.0, baseline);

    if (m_format.highlight && m_format.highlight->isValid()) {
        painter.fillRect(QRectF(start.x(), baseline - metrics.ascent(), width, metrics.height()),
                         *m_format.highlight);
    }
    painter.setPen(m_format.color && m_format.color->isValid() ? *m_format.color
                                                                : palette().color(QPalette::Text));
    painter.drawText(start, text);
}

// =============================================================================
// FontDialog
// =============================================================================

const QList<qreal>& FontDialog::offeredSizes()
{
    static const QList<qreal> sizes = {8, 9, 10, 10.5, 11, 12, 13, 14, 16, 18, 20,
                                       22, 24, 26, 28, 36, 48, 72};
    return sizes;
}

FontDialog::FontDialog(const CharacterFormat& shown, const QString& sample, QWidget* parent)
    : KalahariDialog(parent)
    , m_shown(shown)
    , m_format(shown)
{
    setHeading(tr("Font"),
               tr("The look of the selected text; the paragraph style stays as it is."));
    setHeadingIcon(QStringLiteral("format.font"));
    setCompactHeading(true);
    setAcceptText(tr("OK"));

    // The start of the selection, on one line; without one a sentence
    QString text = sample.simplified();
    if (text.size() > SAMPLE_LENGTH) {
        text = text.left(SAMPLE_LENGTH).trimmed() + QChar(0x2026);
    }
    //: The preview of Format > Font when no text is selected. In Polish: Mgła zeszła nad rzekę
    m_sample = text.isEmpty() ? tr("The mist came down over the river") : text;

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createFontTab(), tr("&Font"));
    m_tabs->addTab(createAdvancedTab(), tr("Ad&vanced"));
    contentLayout()->addWidget(m_tabs, 1);

    updatePreviews();
    m_familyField->setFocus();
}

QWidget* FontDialog::createFontTab()
{
    auto* tab = new QWidget(m_tabs);
    auto* grid = new QGridLayout(tab);
    grid->setHorizontalSpacing(COLUMN_SPACING);
    grid->setVerticalSpacing(FIELD_SPACING);

    // A field over its list: what is typed is found in the list, a row of the list goes in
    // the field
    const auto fieldWithList = [this, tab, grid](const QString& label, int column,
                                                QLineEdit*& field, QListWidget*& list) {
        auto* title = new QLabel(label, tab);
        field = new QLineEdit(tab);
        list = new QListWidget(tab);
        title->setBuddy(field);
        list->setFocusPolicy(Qt::ClickFocus);
        grid->addWidget(title, 0, column);
        auto* box = new QVBoxLayout();
        box->setSpacing(0);
        box->addWidget(field);
        box->addWidget(list);
        grid->addLayout(box, 1, column);
        // The arrows in the field move in the list, as in Word
        field->installEventFilter(this);
        connect(field, &QLineEdit::textEdited, this,
                [this, list](const QString& typed) { findInList(list, typed); });
    };

    fieldWithList(tr("Fo&nt"), 0, m_familyField, m_familyList);
    fieldWithList(tr("St&yle"), 1, m_styleField, m_styleList);
    fieldWithList(tr("&Size"), 2, m_sizeField, m_sizeList);
    grid->setColumnStretch(0, 3);
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(2, 1);

    // The fonts of the system
    QStringList families = QFontDatabase::families();
    families.erase(std::remove_if(families.begin(), families.end(),
                                  [](const QString& family) {
                                      return QFontDatabase::isPrivateFamily(family);
                                  }),
                   families.end());
    m_familyList->addItems(families);
    if (m_format.family) {
        m_familyField->setText(*m_format.family);
        findInList(m_familyList, *m_format.family);
    }
    connect(m_familyList, &QListWidget::currentTextChanged, this, [this](const QString& family) {
        if (m_updating || family.isEmpty()) {
            return;
        }
        m_familyField->setText(family);
        m_format.family = family;
        updatePreviews();
    });
    connect(m_familyField, &QLineEdit::textEdited, this, [this](const QString& typed) {
        const QString family = typed.trimmed();
        const QList<QListWidgetItem*> same = m_familyList->findItems(family, Qt::MatchFixedString);
        m_format.family = !same.isEmpty()   ? same.first()->text()
                          : family.isEmpty() ? m_shown.family
                                             : std::optional<QString>(family);
        updatePreviews();
    });

    // The styles: regular, italic, bold and both
    m_styleList->addItems({tr("Regular"), tr("Italic"), tr("Bold"), tr("Bold Italic")});
    if (m_format.bold && m_format.italic) {
        const int row = (*m_format.bold ? Bold : Regular) + (*m_format.italic ? 1 : 0);
        m_updating = true;
        m_styleList->setCurrentRow(row);
        m_updating = false;
        m_styleField->setText(m_styleList->item(row)->text());
    }
    const auto chooseStyle = [this](int row) {
        if (m_updating || row < 0) {
            return;
        }
        m_format.bold = row == Bold || row == BoldItalic;
        m_format.italic = row == Italic || row == BoldItalic;
        updatePreviews();
    };
    connect(m_styleList, &QListWidget::currentRowChanged, this, [this, chooseStyle](int row) {
        if (!m_updating && row >= 0) {
            m_styleField->setText(m_styleList->item(row)->text());
        }
        chooseStyle(row);
    });
    connect(m_styleField, &QLineEdit::textEdited, this, [this, chooseStyle](const QString& typed) {
        const QList<QListWidgetItem*> same =
            m_styleList->findItems(typed.trimmed(), Qt::MatchFixedString);
        if (!same.isEmpty()) {
            chooseStyle(m_styleList->row(same.first()));
        }
    });

    // The sizes, or any other typed
    for (const qreal size : offeredSizes()) {
        m_sizeList->addItem(pointsText(size));
    }
    auto* sizeValidator =
        new QDoubleValidator(MIN_SIZE, MAX_SIZE, 1, m_sizeField);
    sizeValidator->setNotation(QDoubleValidator::StandardNotation);
    m_sizeField->setValidator(sizeValidator);
    if (m_format.size) {
        m_sizeField->setText(pointsText(*m_format.size));
        findInList(m_sizeList, pointsText(*m_format.size));
    }
    connect(m_sizeList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_updating || row < 0) {
            return;
        }
        m_format.size = offeredSizes().at(row);
        m_sizeField->setText(pointsText(*m_format.size));
        updatePreviews();
    });
    connect(m_sizeField, &QLineEdit::textEdited, this, [this](const QString& typed) {
        bool ok = false;
        const double size = QLocale().toDouble(typed.trimmed(), &ok);
        m_format.size = ok && size >= MIN_SIZE &&
                                size <= MAX_SIZE
                            ? std::optional<qreal>(size)
                            : m_shown.size;
        updatePreviews();
    });

    // Each list shows a few rows; its width is the column's, not the width of its longest
    // item, so that a long name of a font does not widen the window
    for (QListWidget* list : {m_familyList, m_styleList, m_sizeList}) {
        list->setFixedHeight(list->sizeHintForRow(0) * LIST_ROWS + list->frameWidth() * 2);
        list->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }
    m_familyList->setMinimumWidth(FAMILY_LIST_WIDTH);
    m_styleList->setMinimumWidth(STYLE_LIST_WIDTH);
    m_sizeList->setMinimumWidth(SIZE_LIST_WIDTH);

    // The colors, as the submenus of the Format menu give them
    auto* colorLabel = new QLabel(tr("Font &color"), tab);
    auto* highlightLabel = new QLabel(tr("&Highlight color"), tab);
    m_colorButton = new QPushButton(tab);
    m_highlightButton = new QPushButton(tab);
    colorLabel->setBuddy(m_colorButton);
    highlightLabel->setBuddy(m_highlightButton);
    m_colorMenu = new ColorMenu(
        ColorMenu::Kind::Text, [this]() { return m_format.color.value_or(QColor()); }, this);
    m_highlightMenu = new ColorMenu(
        ColorMenu::Kind::Highlight, [this]() { return m_format.highlight.value_or(QColor()); },
        this);
    m_colorButton->setMenu(m_colorMenu);
    m_highlightButton->setMenu(m_highlightMenu);
    connect(m_colorMenu, &ColorMenu::colorChosen, this, [this](const QColor& color) {
        m_format.color = color;
        showColor(m_colorButton, m_format.color, false);
        updatePreviews();
    });
    connect(m_highlightMenu, &ColorMenu::colorChosen, this, [this](const QColor& color) {
        m_format.highlight = color;
        showColor(m_highlightButton, m_format.highlight, true);
        updatePreviews();
    });
    showColor(m_colorButton, m_format.color, false);
    showColor(m_highlightButton, m_format.highlight, true);
    grid->addWidget(colorLabel, 2, 0);
    grid->addWidget(highlightLabel, 2, 1, 1, 2);
    grid->addWidget(m_colorButton, 3, 0);
    auto* highlightRow = new QHBoxLayout();
    highlightRow->addWidget(m_highlightButton, 1);
    highlightRow->addStretch(1);
    grid->addLayout(highlightRow, 3, 1, 1, 2);

    // The effects, in two columns, and the preview
    auto* effectsLabel = new QLabel(tr("Effects"), tab);
    auto* previewLabel = new QLabel(tr("Preview"), tab);
    m_underline = createEffect(tr("&Underline"), m_format.underline);
    m_strikethrough = createEffect(tr("Stri&kethrough"), m_format.strikethrough);
    const auto position = m_format.position;
    m_superscript = createEffect(
        tr("Su&perscript"),
        position ? std::optional<bool>(*position == QTextCharFormat::AlignSuperScript)
                 : std::nullopt);
    m_subscript = createEffect(
        tr("Subscrip&t"),
        position ? std::optional<bool>(*position == QTextCharFormat::AlignSubScript)
                 : std::nullopt);
    const auto capitals = m_format.capitals;
    m_smallCaps = createEffect(
        tr("S&mall caps"),
        capitals ? std::optional<bool>(*capitals == QFont::SmallCaps) : std::nullopt);
    m_allCaps = createEffect(
        tr("&All caps"),
        capitals ? std::optional<bool>(*capitals == QFont::AllUppercase) : std::nullopt);
    auto* effects = new QGridLayout();
    effects->setHorizontalSpacing(COLUMN_SPACING);
    effects->setVerticalSpacing(FIELD_SPACING);
    effects->addWidget(m_underline, 0, 0);
    effects->addWidget(m_strikethrough, 1, 0);
    effects->addWidget(m_superscript, 2, 0);
    effects->addWidget(m_subscript, 0, 1);
    effects->addWidget(m_smallCaps, 1, 1);
    effects->addWidget(m_allCaps, 2, 1);
    effects->setRowStretch(3, 1);
    m_preview = new FontPreview(tab);
    grid->addWidget(effectsLabel, 4, 0);
    grid->addWidget(previewLabel, 4, 1, 1, 2);
    grid->addLayout(effects, 5, 0);
    grid->addWidget(m_preview, 5, 1, 1, 2);

    onCheckStateChanged(m_underline, this, [this](Qt::CheckState state) {
        if (state != Qt::PartiallyChecked) {
            m_format.underline = state == Qt::Checked;
            updatePreviews();
        }
    });
    onCheckStateChanged(m_strikethrough, this, [this](Qt::CheckState state) {
        if (state != Qt::PartiallyChecked) {
            m_format.strikethrough = state == Qt::Checked;
            updatePreviews();
        }
    });
    // An index takes the other off, as a kind of capitals does
    const auto exclusive = [this](QCheckBox* box, QCheckBox* other) {
        onCheckStateChanged(box, this, [this, other](Qt::CheckState state) {
            if (m_updating) {
                return;
            }
            if (state == Qt::Checked && other->checkState() != Qt::Unchecked) {
                m_updating = true;
                other->setTristate(false);
                other->setCheckState(Qt::Unchecked);
                m_updating = false;
            }
            readEffects();
        });
    };
    exclusive(m_superscript, m_subscript);
    exclusive(m_subscript, m_superscript);
    exclusive(m_smallCaps, m_allCaps);
    exclusive(m_allCaps, m_smallCaps);
    return tab;
}

QWidget* FontDialog::createAdvancedTab()
{
    auto* tab = new QWidget(m_tabs);
    auto* column = new QVBoxLayout(tab);
    column->setSpacing(FIELD_SPACING);

    auto* group = new QGroupBox(tr("Character spacing"), tab);
    auto* rows = new QVBoxLayout(group);
    auto* row = new QHBoxLayout();
    row->setSpacing(FIELD_SPACING);
    m_spacingBox = new QComboBox(group);
    m_spacingBox->addItems({tr("Normal"), tr("Expanded"), tr("Condensed")});
    m_spacingBox->setAccessibleName(tr("Character spacing"));
    //: Format > Font, the letter spacing: Expanded "by" 1.5 pt. In Polish: o
    auto* byLabel = new QLabel(tr("&by"), group);
    m_spacingField = new QDoubleSpinBox(group);
    m_spacingField->setDecimals(1);
    m_spacingField->setSingleStep(0.1);
    //: The unit of the letter spacing, points. In Polish: pt
    m_spacingField->setSuffix(tr(" pt"));
    byLabel->setBuddy(m_spacingField);
    row->addWidget(m_spacingBox, 1);
    row->addWidget(byLabel);
    row->addWidget(m_spacingField, 1);
    rows->addLayout(row);
    auto* note = new QLabel(
        tr("Expanded spacing moves the letters of the selected text apart, condensed spacing "
           "brings them closer together."),
        group);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    rows->addWidget(note);
    column->addWidget(group);

    column->addWidget(new QLabel(tr("Preview"), tab));
    m_advancedPreview = new FontPreview(tab);
    column->addWidget(m_advancedPreview);
    column->addStretch(1);

    // The spacing of the text: none, more or less; a selection with more than one: empty
    m_updating = true;
    if (!m_format.letterSpacing) {
        m_spacingBox->setCurrentIndex(-1);
        m_spacingField->setEnabled(false);
    } else {
        const double spacing = *m_format.letterSpacing;
        m_spacingBox->setCurrentIndex(spacing > 0 ? 1 : spacing < 0 ? 2 : 0);
        m_spacingField->setRange(0.0, spacing < 0 ? -editor::MIN_LETTER_SPACING
                                                  : editor::MAX_LETTER_SPACING);
        m_spacingField->setValue(std::abs(spacing));
        m_spacingField->setEnabled(spacing != 0.0);
    }
    m_updating = false;
    connect(m_spacingBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_updating) {
            return;
        }
        m_updating = true;
        m_spacingField->setEnabled(index > 0);
        m_spacingField->setRange(0.0, index == 2 ? -editor::MIN_LETTER_SPACING
                                                 : editor::MAX_LETTER_SPACING);
        if (index > 0 && m_spacingField->value() == 0.0) {
            m_spacingField->setValue(FIRST_SPACING);
        }
        if (index == 0) {
            m_spacingField->setValue(0.0);
        }
        m_updating = false;
        readSpacing();
    });
    connect(m_spacingField, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (!m_updating) {
            readSpacing();
        }
    });
    return tab;
}

QCheckBox* FontDialog::createEffect(const QString& text, std::optional<bool> checked)
{
    auto* box = new QCheckBox(text, this);
    // Half checked while the selection has it in part; once clicked it is on or off
    box->setTristate(!checked.has_value());
    box->setCheckState(stateOf(checked));
    connect(box, &QCheckBox::clicked, box, [box]() { box->setTristate(false); });
    return box;
}

bool FontDialog::eventFilter(QObject* watched, QEvent* event)
{
    // Up and Down in a field move in its list, Page Up and Page Down by its page
    if (event->type() == QEvent::KeyPress) {
        QListWidget* list = watched == m_familyField ? m_familyList
                            : watched == m_styleField ? m_styleList
                            : watched == m_sizeField  ? m_sizeList
                                                      : nullptr;
        auto* key = static_cast<QKeyEvent*>(event);
        if (list != nullptr && list->count() > 0 && key->modifiers() == Qt::NoModifier) {
            int step = 0;
            switch (key->key()) {
                case Qt::Key_Up: step = -1; break;
                case Qt::Key_Down: step = 1; break;
                case Qt::Key_PageUp: step = -LIST_ROWS; break;
                case Qt::Key_PageDown: step = LIST_ROWS; break;
                default: break;
            }
            if (step != 0) {
                const int row = list->currentRow() < 0
                                    ? (step > 0 ? 0 : list->count() - 1)
                                    : std::clamp(list->currentRow() + step, 0, list->count() - 1);
                list->setCurrentRow(row);
                if (auto* field = qobject_cast<QLineEdit*>(watched)) {
                    field->selectAll();
                }
                return true;
            }
        }
    }
    return KalahariDialog::eventFilter(watched, event);
}

void FontDialog::findInList(QListWidget* list, const QString& text)
{
    const QString typed = text.trimmed();
    if (typed.isEmpty()) {
        return;
    }
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->text().startsWith(typed, Qt::CaseInsensitive)) {
            m_updating = true;
            list->setCurrentRow(row);
            list->scrollToItem(list->item(row), QAbstractItemView::PositionAtTop);
            m_updating = false;
            return;
        }
    }
}

void FontDialog::readEffects()
{
    const auto readPair = [](QCheckBox* first, QCheckBox* second, auto onFirst, auto onSecond,
                             auto off, auto& part) {
        if (first->checkState() == Qt::Checked) {
            part = onFirst;
        } else if (second->checkState() == Qt::Checked) {
            part = onSecond;
        } else if (first->checkState() == Qt::Unchecked &&
                   second->checkState() == Qt::Unchecked) {
            part = off;
        }
    };
    std::optional<QTextCharFormat::VerticalAlignment> position = m_shown.position;
    readPair(m_superscript, m_subscript, QTextCharFormat::AlignSuperScript,
             QTextCharFormat::AlignSubScript, QTextCharFormat::AlignNormal, position);
    m_format.position = position;
    std::optional<QFont::Capitalization> capitals = m_shown.capitals;
    readPair(m_smallCaps, m_allCaps, QFont::SmallCaps, QFont::AllUppercase, QFont::MixedCase,
             capitals);
    m_format.capitals = capitals;
    updatePreviews();
}

void FontDialog::readSpacing()
{
    switch (m_spacingBox->currentIndex()) {
        case 0:
            m_format.letterSpacing = 0.0;
            break;
        case 1:
            m_format.letterSpacing = m_spacingField->value();
            break;
        case 2:
            m_format.letterSpacing = -m_spacingField->value();
            break;
        default:
            m_format.letterSpacing = m_shown.letterSpacing;
            break;
    }
    updatePreviews();
}

void FontDialog::showColor(QPushButton* button, const std::optional<QColor>& color, bool highlight)
{
    const int side = button->fontMetrics().height();
    const QColor border = palette().color(QPalette::Mid);
    if (!color) {
        // Not the same in all of the selection
        button->setIcon(swatchIcon(QColor(), border, side));
        button->setText(QString());
        return;
    }
    if (!color->isValid()) {
        button->setIcon(highlight ? core::ArtProvider::getInstance().getIcon(
                                        QStringLiteral("format.noColor"))
                                  : swatchIcon(palette().color(QPalette::Text), border, side));
        //: The color of a text: the editor's, which follows its paper
        button->setText(highlight ? tr("None") : tr("Automatic"));
        return;
    }
    button->setIcon(swatchIcon(*color, border, side));
    const QString name = paletteColorName(colorPalettes(), *color);
    button->setText(name.isEmpty() ? color->name().toUpper() : name);
}

void FontDialog::updatePreviews()
{
    m_preview->showFormat(m_format, m_sample);
    if (m_advancedPreview != nullptr) {
        m_advancedPreview->showFormat(m_format, m_sample);
    }
}

CharacterFormat FontDialog::changes() const
{
    CharacterFormat changes;
    const auto changed = [](const auto& now, const auto& before, auto& change) {
        if (now && now != before) {
            change = now;
        }
    };
    changed(m_format.family, m_shown.family, changes.family);
    if (m_format.size && (!m_shown.size || !sameSize(*m_format.size, *m_shown.size))) {
        changes.size = m_format.size;
    }
    changed(m_format.bold, m_shown.bold, changes.bold);
    changed(m_format.italic, m_shown.italic, changes.italic);
    changed(m_format.underline, m_shown.underline, changes.underline);
    changed(m_format.strikethrough, m_shown.strikethrough, changes.strikethrough);
    changed(m_format.position, m_shown.position, changes.position);
    changed(m_format.capitals, m_shown.capitals, changes.capitals);
    if (m_format.letterSpacing &&
        (!m_shown.letterSpacing || !sameSize(*m_format.letterSpacing, *m_shown.letterSpacing))) {
        changes.letterSpacing = m_format.letterSpacing;
    }
    changed(m_format.color, m_shown.color, changes.color);
    changed(m_format.highlight, m_shown.highlight, changes.highlight);
    return changes;
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
