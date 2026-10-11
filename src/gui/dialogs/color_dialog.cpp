/// @file color_dialog.cpp
/// @brief The program's own window for choosing a color

#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/core/settings_manager.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <array>
#include <string>
#include <vector>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int PALETTE_COLUMNS = 10;
constexpr int SWATCH_SIZE = 20;            ///< Side of a swatch
constexpr int SWATCH_CELL = 26;            ///< Side of a swatch's cell, with the room around it
constexpr int PREVIEW_HEIGHT = 56;
constexpr int PREVIEW_WIDTH = 84;
constexpr int SPACING = 12;
constexpr int LABEL_SPACING = 4;
constexpr const char* RECENT_COLORS_KEY = "ui.recentColors";

/// @brief Hues of the palette's columns after the greys, in degrees
constexpr std::array<int, PALETTE_COLUMNS - 1> PALETTE_HUES = {0, 30, 50, 120, 170, 200, 220, 270, 320};

/// @brief Lightness of the palette's bright rows, from light to dark, in percent
constexpr std::array<int, 5> PALETTE_LIGHTNESS = {88, 74, 58, 44, 30};

constexpr int PALETTE_SATURATION = 70;     ///< Saturation of the bright rows, in percent
constexpr int MUTED_SATURATION = 35;       ///< The last row: muted tones
constexpr int MUTED_LIGHTNESS = 50;
constexpr int GREY_STEP = 51;              ///< Greys from white to black in six steps

/// @brief A color from hue, saturation and lightness in degrees and percent
QColor fromHsl(int hue, int saturation, int lightness)
{
    // Rounded to whole 0-255 values, as the HEX and RGB fields show it and as a name
    // stores it, so the same color always compares equal
    return QColor::fromRgb(
        QColor::fromHslF(hue / 360.0F, saturation / 100.0F, lightness / 100.0F).rgb());
}

/// @brief Text color readable on a background color: black or white on any swatch, which
///        no theme color can promise
QColor readableTextOn(const QColor& background)
{
    constexpr int LIGHT_BACKGROUND = 140;
    const int luminance = qGray(background.rgb());
    return luminance > LIGHT_BACKGROUND ? QColor(Qt::black) : QColor(Qt::white);
}

/// @brief Style sheet of a preview area in a color
QString previewStyle(const QColor& color, const QColor& border)
{
    return QStringLiteral("background-color: %1; color: %2; border: 1px solid %3; padding: 4px;")
        .arg(color.name(), readableTextOn(color).name(), border.name());
}

/// @brief Draws a swatch in its own color. An icon would be tinted when selected, which
///        would show the writer a color other than the one they pick.
class SwatchDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        const QRect swatch(option.rect.center().x() - SWATCH_SIZE / 2 + 1,
                           option.rect.center().y() - SWATCH_SIZE / 2 + 1,
                           SWATCH_SIZE, SWATCH_SIZE);
        painter->save();
        painter->fillRect(swatch, index.data(Qt::UserRole).value<QColor>());
        painter->setPen(option.palette.color(QPalette::Mid));
        painter->drawRect(swatch.adjusted(0, 0, -1, -1));

        // The selected swatch gets a frame around it, the current one a thinner frame
        // while the list has the focus
        const QColor highlight = option.palette.color(QPalette::Highlight);
        if (option.state.testFlag(QStyle::State_Selected)) {
            painter->setPen(QPen(highlight, 2));
            painter->drawRect(swatch.adjusted(-2, -2, 1, 1));
        } else if (option.state.testFlag(QStyle::State_HasFocus)) {
            painter->setPen(QPen(highlight, 1, Qt::DotLine));
            painter->drawRect(swatch.adjusted(-2, -2, 1, 1));
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& /*option*/,
                   const QModelIndex& /*index*/) const override
    {
        return {SWATCH_CELL, SWATCH_CELL};
    }
};

} // anonymous namespace

ColorDialog::ColorDialog(const QColor& initial, const QString& description, QWidget* parent)
    : KalahariDialog(parent)
    , m_initial(QColor::fromRgb(initial.isValid() ? initial.rgb() : QColor(Qt::black).rgb()))
    , m_color(m_initial)
{
    setHeading(tr("Select Color"), description);
    setHeadingIcon(QStringLiteral("common.palette"));
    setCompactHeading(true);
    setAcceptText(tr("Select"));

    auto* columns = new QHBoxLayout();
    columns->setSpacing(SPACING * 2);

    // Left: the palette and the recent colors
    auto* swatches = new QVBoxLayout();
    swatches->setSpacing(LABEL_SPACING);

    auto* paletteLabel = new QLabel(tr("&Palette"), this);
    m_paletteList = createSwatchList(tr("Palette"));
    for (const QColor& color : paletteColors()) {
        addSwatch(m_paletteList, color);
    }
    const int paletteRows = static_cast<int>(paletteColors().size()) / PALETTE_COLUMNS;
    m_paletteList->setFixedHeight(paletteRows * SWATCH_CELL + 2 * m_paletteList->frameWidth());
    paletteLabel->setBuddy(m_paletteList);
    swatches->addWidget(paletteLabel);
    swatches->addWidget(m_paletteList);
    swatches->addSpacing(SPACING);

    auto* recentLabel = new QLabel(tr("Rec&ent colors"), this);
    m_recentList = createSwatchList(tr("Recent colors"));
    for (const QString& name : recentColors()) {
        addSwatch(m_recentList, QColor(name));
    }
    m_recentList->setFixedHeight(SWATCH_CELL + 2 * m_recentList->frameWidth());
    recentLabel->setBuddy(m_recentList);
    m_noRecentLabel = new QLabel(tr("The colors you select appear here."), this);
    m_noRecentLabel->setForegroundRole(QPalette::PlaceholderText);
    m_recentList->setVisible(m_recentList->count() > 0);
    m_noRecentLabel->setVisible(m_recentList->count() == 0);
    swatches->addWidget(recentLabel);
    swatches->addWidget(m_recentList);
    swatches->addWidget(m_noRecentLabel);
    swatches->addStretch(1);
    columns->addLayout(swatches);

    // Right: the preview, HEX and RGB
    auto* values = new QVBoxLayout();
    values->setSpacing(LABEL_SPACING);

    values->addWidget(new QLabel(tr("Preview"), this));
    auto* preview = new QHBoxLayout();
    preview->setSpacing(0);
    m_previousButton = new QPushButton(tr("Pre&vious"), this);
    m_previousButton->setFixedSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    m_previousButton->setToolTip(tr("Bring back the previous color"));
    m_previousButton->setAutoDefault(false);
    connect(m_previousButton, &QPushButton::clicked, this, [this]() { setColor(m_initial); });
    m_newLabel = new QLabel(tr("New"), this);
    m_newLabel->setFixedSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    m_newLabel->setAlignment(Qt::AlignCenter);
    preview->addWidget(m_previousButton);
    preview->addWidget(m_newLabel);
    preview->addStretch(1);
    values->addLayout(preview);
    values->addSpacing(SPACING);

    auto* hexLabel = new QLabel(tr("&HEX"), this);
    m_hexEdit = new QLineEdit(this);
    m_hexEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("#?[0-9A-Fa-f]{0,6}")), m_hexEdit));
    m_hexEdit->setPlaceholderText(QStringLiteral("#RRGGBB"));
    hexLabel->setBuddy(m_hexEdit);
    connect(m_hexEdit, &QLineEdit::textEdited, this, &ColorDialog::onHexEdited);
    // Leaving the field shows the color in full, "#12AB34" for "12ab34"
    connect(m_hexEdit, &QLineEdit::editingFinished, this,
            [this]() { m_hexEdit->setText(m_color.name().toUpper()); });
    values->addWidget(hexLabel);
    values->addWidget(m_hexEdit);
    values->addSpacing(SPACING);

    auto* rgb = new QGridLayout();
    rgb->setHorizontalSpacing(LABEL_SPACING * 2);
    rgb->setVerticalSpacing(LABEL_SPACING);
    const std::array<QString, 3> rgbLabels = {tr("&Red"), tr("&Green"), tr("&Blue")};
    std::array<QSpinBox**, 3> rgbFields = {&m_redSpin, &m_greenSpin, &m_blueSpin};
    for (int i = 0; i < 3; ++i) {
        auto* label = new QLabel(rgbLabels[static_cast<size_t>(i)], this);
        auto* spin = new QSpinBox(this);
        spin->setRange(0, 255);
        label->setBuddy(spin);
        connect(spin, &QSpinBox::valueChanged, this, &ColorDialog::onRgbChanged);
        rgb->addWidget(label, 0, i);
        rgb->addWidget(spin, 1, i);
        *rgbFields[static_cast<size_t>(i)] = spin;
    }
    values->addLayout(rgb);
    values->addStretch(1);
    columns->addLayout(values);

    contentLayout()->addLayout(columns);

    updateViews();
    m_paletteList->setFocus();
}

QList<QColor> ColorDialog::paletteColors()
{
    QList<QColor> colors;
    const int rows = static_cast<int>(PALETTE_LIGHTNESS.size()) + 1;
    for (int row = 0; row < rows; ++row) {
        const int grey = 255 - row * GREY_STEP;
        colors.append(QColor(grey, grey, grey));
        const bool muted = row == rows - 1;
        for (const int hue : PALETTE_HUES) {
            colors.append(muted ? fromHsl(hue, MUTED_SATURATION, MUTED_LIGHTNESS)
                                : fromHsl(hue, PALETTE_SATURATION,
                                          PALETTE_LIGHTNESS[static_cast<size_t>(row)]));
        }
    }
    return colors;
}

QStringList ColorDialog::recentColors()
{
    const auto names = core::SettingsManager::getInstance().get<std::vector<std::string>>(
        RECENT_COLORS_KEY, {});
    QStringList colors;
    for (const std::string& name : names) {
        const QColor color(QString::fromStdString(name));
        if (color.isValid() && colors.size() < MAX_RECENT_COLORS) {
            colors.append(color.name());
        }
    }
    return colors;
}

void ColorDialog::addRecentColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    QStringList colors = recentColors();
    colors.removeAll(color.name());
    colors.prepend(color.name());
    std::vector<std::string> names;
    for (int i = 0; i < colors.size() && i < MAX_RECENT_COLORS; ++i) {
        names.push_back(colors.at(i).toStdString());
    }
    core::SettingsManager::getInstance().set<std::vector<std::string>>(RECENT_COLORS_KEY, names);
}

std::optional<QColor> ColorDialog::getColor(const QColor& initial, QWidget* parent,
                                            const QString& description)
{
    ColorDialog dialog(initial, description, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return dialog.color();
}

void ColorDialog::setColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    m_color = QColor::fromRgb(color.rgb());
    m_color.setAlpha(255);
    updateViews();
}

void ColorDialog::accept()
{
    addRecentColor(m_color);
    KalahariDialog::accept();
}

QListWidget* ColorDialog::createSwatchList(const QString& accessibleName)
{
    auto* list = new QListWidget(this);
    list->setAccessibleName(accessibleName);
    list->setViewMode(QListView::IconMode);
    list->setMovement(QListView::Static);
    list->setFlow(QListView::LeftToRight);
    list->setWrapping(true);
    list->setUniformItemSizes(true);
    list->setItemDelegate(new SwatchDelegate(list));
    list->setGridSize(QSize(SWATCH_CELL, SWATCH_CELL));
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Ten cells a row: the view needs a little room beyond them, or it wraps at nine
    list->setFixedWidth(PALETTE_COLUMNS * SWATCH_CELL + 2 * list->frameWidth() + SWATCH_CELL / 2);

    // The selected swatch is the color: arrows and clicks select, Space too
    connect(list, &QListWidget::itemSelectionChanged, this, [this, list]() {
        if (m_updating) {
            return;
        }
        const QList<QListWidgetItem*> selected = list->selectedItems();
        if (!selected.isEmpty()) {
            setColor(selected.first()->data(Qt::UserRole).value<QColor>());
        }
    });
    return list;
}

void ColorDialog::addSwatch(QListWidget* list, const QColor& color)
{
    auto* item = new QListWidgetItem(list);
    item->setData(Qt::UserRole, color);
    item->setToolTip(color.name().toUpper());
    item->setData(Qt::AccessibleTextRole, color.name().toUpper());
}

void ColorDialog::onHexEdited(const QString& text)
{
    QString hex = text.trimmed();
    if (!hex.startsWith(QLatin1Char('#'))) {
        hex.prepend(QLatin1Char('#'));
    }
    // A complete value chooses the color; an incomplete one cannot be selected yet
    const bool complete = hex.size() == 7 && QColor::isValidColorName(hex);
    acceptButton()->setEnabled(complete);
    if (complete) {
        m_color = QColor(hex);
        m_typingHex = true;
        updateViews();
        m_typingHex = false;
    }
}

void ColorDialog::onRgbChanged()
{
    if (m_updating) {
        return;
    }
    setColor(QColor(m_redSpin->value(), m_greenSpin->value(), m_blueSpin->value()));
}

void ColorDialog::updateViews()
{
    m_updating = true;

    const QColor border = palette().color(QPalette::Mid);
    m_previousButton->setStyleSheet(previewStyle(m_initial, border));
    m_newLabel->setStyleSheet(previewStyle(m_color, border));
    m_newLabel->setAccessibleName(tr("New color %1").arg(m_color.name().toUpper()));

    if (!m_typingHex) {
        m_hexEdit->setText(m_color.name().toUpper());
    }
    m_redSpin->setValue(m_color.red());
    m_greenSpin->setValue(m_color.green());
    m_blueSpin->setValue(m_color.blue());
    acceptButton()->setEnabled(true);

    // Select the swatch of the color, if a list has it. Otherwise nothing is selected,
    // but the list keeps a current item, so getting the focus does not pick a color.
    for (QListWidget* list : {m_paletteList, m_recentList}) {
        QListWidgetItem* match = nullptr;
        for (int i = 0; i < list->count() && !match; ++i) {
            if (list->item(i)->data(Qt::UserRole).value<QColor>() == m_color) {
                match = list->item(i);
            }
        }
        if (match) {
            list->setCurrentItem(match);
        } else {
            list->clearSelection();
            if (!list->currentItem() && list->count() > 0) {
                list->selectionModel()->setCurrentIndex(list->model()->index(0, 0),
                                                        QItemSelectionModel::NoUpdate);
            }
        }
    }

    m_updating = false;
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
