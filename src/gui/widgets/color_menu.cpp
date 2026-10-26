/// @file color_menu.cpp
/// @brief The submenus of colors of the Format menu

#include "kalahari/gui/widgets/color_menu.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/gui/dialogs/color_dialog.h"
#include "kalahari/gui/dialogs/color_palettes.h"

#include <QAction>
#include <QApplication>
#include <QFocusEvent>
#include <QHelpEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

/// @brief Room around the swatches, to the edges of the menu's items
constexpr int SWATCHES_MARGIN = 6;

/// @brief The colors of the recent ones in a row
constexpr int RECENT_COLUMNS = dialogs::PALETTE_FAMILIES;

/// @brief The colors the commands give before one is chosen, as in Word: a red of the text
///        and a yellow highlight of the first palette (its row and column)
constexpr int TEXT_DEFAULT_ROW = 3;
constexpr int TEXT_DEFAULT_FAMILY = 1;
constexpr int HIGHLIGHT_DEFAULT_ROW = 1;
constexpr int HIGHLIGHT_DEFAULT_FAMILY = 3;

/// @brief A color of the first palette by its row and column
QColor firstPaletteColor(int row, int family)
{
    const QList<dialogs::ColorPalette> palettes = dialogs::colorPalettes();
    const int index = row * dialogs::PALETTE_FAMILIES + family;
    return !palettes.isEmpty() && index < palettes.first().colors.size()
               ? palettes.first().colors.at(index).color
               : QColor();
}

/// @brief The icon of Automatic: a swatch of the color of the text
QIcon swatchIcon(const QColor& color, const QColor& border)
{
    QPixmap pixmap(ColorSwatches::SWATCH_SIZE, ColorSwatches::SWATCH_SIZE);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(pixmap.rect().adjusted(1, 1, -1, -1), color);
    painter.setPen(border);
    painter.drawRect(pixmap.rect().adjusted(1, 1, -2, -2));
    return QIcon(pixmap);
}

}  // namespace

// =============================================================================
// ColorSwatches
// =============================================================================

ColorSwatches::ColorSwatches(int columns, QWidget* parent)
    : QWidget(parent)
    , m_columns(std::max(columns, 1))
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void ColorSwatches::setColors(const QList<QColor>& colors, const QStringList& names)
{
    m_colors = colors;
    m_names = names;
    while (m_names.size() < m_colors.size()) {
        m_names.append(QString());
    }
    if (m_current >= count()) {
        m_current = -1;
    }
    updateGeometry();
    update();
}

QColor ColorSwatches::colorAt(int index) const
{
    return index >= 0 && index < count() ? m_colors.at(index) : QColor();
}

QString ColorSwatches::textAt(int index) const
{
    if (index < 0 || index >= count()) {
        return QString();
    }
    const QString hex = m_colors.at(index).name().toUpper();
    //: A swatch of a color submenu: the name of the color and its HEX, "Red (#CC0000)"
    return m_names.at(index).isEmpty() ? hex : tr("%1 (%2)").arg(m_names.at(index), hex);
}

void ColorSwatches::setCurrentIndex(int index)
{
    const int current = index >= 0 && index < count() ? index : -1;
    if (current == m_current) {
        return;
    }
    m_current = current;
    update();
}

void ColorSwatches::setMarkedColor(const QColor& color)
{
    m_marked = color;
    update();
}

int ColorSwatches::indexOf(const QColor& color) const
{
    if (!color.isValid()) {
        return -1;
    }
    for (int i = 0; i < count(); ++i) {
        if (m_colors.at(i).rgb() == color.rgb()) {
            return i;
        }
    }
    return -1;
}

int ColorSwatches::rows() const
{
    return (count() + m_columns - 1) / m_columns;
}

QSize ColorSwatches::sizeHint() const
{
    return {m_columns * CELL_SIZE + 2 * SWATCHES_MARGIN,
            std::max(rows(), 1) * CELL_SIZE + 2 * SWATCHES_MARGIN};
}

QSize ColorSwatches::minimumSizeHint() const
{
    return sizeHint();
}

QRect ColorSwatches::swatchRect(int index) const
{
    const int column = index % m_columns;
    const int row = index / m_columns;
    const int inset = (CELL_SIZE - SWATCH_SIZE) / 2;
    return {SWATCHES_MARGIN + column * CELL_SIZE + inset, SWATCHES_MARGIN + row * CELL_SIZE + inset,
            SWATCH_SIZE, SWATCH_SIZE};
}

int ColorSwatches::indexAt(const QPoint& point) const
{
    const int column = (point.x() - SWATCHES_MARGIN) / CELL_SIZE;
    const int row = (point.y() - SWATCHES_MARGIN) / CELL_SIZE;
    if (point.x() < SWATCHES_MARGIN || point.y() < SWATCHES_MARGIN || column >= m_columns) {
        return -1;
    }
    const int index = row * m_columns + column;
    return index < count() ? index : -1;
}

void ColorSwatches::tellCurrent()
{
    const QString text = textAt(m_current);
    setAccessibleDescription(text);
    if (m_current >= 0 && isVisible()) {
        const QRect swatch = swatchRect(m_current);
        QToolTip::showText(mapToGlobal(swatch.bottomLeft() + QPoint(0, 2)), text, this, swatch);
    } else {
        QToolTip::hideText();
    }
}

bool ColorSwatches::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const int index = indexAt(help->pos());
        if (index >= 0) {
            QToolTip::showText(help->globalPos(), textAt(index), this, swatchRect(index));
        } else {
            QToolTip::hideText();
            event->ignore();
        }
        return true;
    }
    return QWidget::event(event);
}

void ColorSwatches::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const QColor border = palette().color(QPalette::Mid);
    const QColor highlight = palette().color(QPalette::Highlight);
    for (int i = 0; i < count(); ++i) {
        const QRect swatch = swatchRect(i);
        painter.fillRect(swatch, m_colors.at(i));
        painter.setPen(border);
        painter.drawRect(swatch.adjusted(0, 0, -1, -1));
        // The color of the text gets a frame in the color of the text, the swatch the arrows
        // or the mouse are at a wider one in the color of a selection
        if (i == m_current) {
            painter.setPen(QPen(highlight, 2));
            painter.drawRect(swatch.adjusted(-2, -2, 1, 1));
        } else if (m_marked.isValid() && m_colors.at(i).rgb() == m_marked.rgb()) {
            painter.setPen(palette().color(QPalette::Text));
            painter.drawRect(swatch.adjusted(-2, -2, 1, 1));
        }
    }
}

void ColorSwatches::keyPressEvent(QKeyEvent* event)
{
    if (event->modifiers() & ~Qt::KeypadModifier) {
        event->ignore();
        return;
    }
    const int current = std::max(m_current, 0);
    int next = current;
    switch (event->key()) {
        case Qt::Key_Left:
            next = current % m_columns > 0 ? current - 1 : current;
            break;
        case Qt::Key_Right:
            next = current % m_columns < m_columns - 1 && current + 1 < count() ? current + 1
                                                                                  : current;
            break;
        case Qt::Key_Up:
            if (current < m_columns) {
                event->ignore();  // on to the menu: the item above
                return;
            }
            next = current - m_columns;
            break;
        case Qt::Key_Down:
            if (current + m_columns >= count()) {
                event->ignore();  // on to the menu: the item below
                return;
            }
            next = current + m_columns;
            break;
        case Qt::Key_Home:
            next = current - current % m_columns;
            break;
        case Qt::Key_End:
            next = std::min(current - current % m_columns + m_columns, count()) - 1;
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_current >= 0) {
                emit colorChosen(m_colors.at(m_current));
            }
            return;
        default:
            event->ignore();
            return;
    }
    setCurrentIndex(next);
    tellCurrent();
}

void ColorSwatches::mouseMoveEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position().toPoint());
    if (index >= 0) {
        setCurrentIndex(index);
    }
    QWidget::mouseMoveEvent(event);
}

void ColorSwatches::mouseReleaseEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && index >= 0) {
        setCurrentIndex(index);
        emit colorChosen(m_colors.at(index));
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ColorSwatches::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    emit entered();
    tellCurrent();
}

void ColorSwatches::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    QToolTip::hideText();
    setCurrentIndex(-1);
}

void ColorSwatches::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    if (!hasFocus()) {
        setCurrentIndex(-1);
    }
}

// =============================================================================
// ColorMenu
// =============================================================================

ColorMenu::ColorMenu(Kind kind, CurrentColor currentColor, QWidget* parent)
    : QMenu(parent)
    , m_kind(kind)
    , m_currentColor(std::move(currentColor))
{
    setToolTipsVisible(true);
    m_lastColor = kind == Kind::Text
                      ? firstPaletteColor(TEXT_DEFAULT_ROW, TEXT_DEFAULT_FAMILY)
                      : firstPaletteColor(HIGHLIGHT_DEFAULT_ROW, HIGHLIGHT_DEFAULT_FAMILY);

    // Automatic: the color of the editor, which follows its paper; No Color: none
    m_noColorAction = addAction(kind == Kind::Text ? tr("&Automatic") : tr("&No Color"));
    connect(m_noColorAction, &QAction::triggered, this, [this]() { choose(QColor()); });

    // The palette, as the color window showed it last
    m_palette = new ColorSwatches(dialogs::PALETTE_FAMILIES, this);
    m_palette->setAccessibleName(kind == Kind::Text ? tr("Font colors") : tr("Highlight colors"));
    m_paletteAction = new QWidgetAction(this);
    m_paletteAction->setDefaultWidget(m_palette);
    addSeparator();
    addAction(m_paletteAction);

    // The recent colors of the color window, under their label; none: hidden
    auto* recent = new QWidget(this);
    auto* recentLayout = new QVBoxLayout(recent);
    recentLayout->setContentsMargins(SWATCHES_MARGIN, 0, 0, 0);
    recentLayout->setSpacing(0);
    m_recentLabel = new QLabel(tr("Recent colors"), recent);
    m_recentLabel->setForegroundRole(QPalette::PlaceholderText);
    m_recent = new ColorSwatches(RECENT_COLUMNS, recent);
    m_recent->setAccessibleName(tr("Recent colors"));
    m_recentLabel->setBuddy(m_recent);
    recentLayout->addWidget(m_recentLabel);
    recentLayout->addWidget(m_recent);
    recent->setFocusProxy(m_recent);
    m_recentAction = new QWidgetAction(this);
    m_recentAction->setDefaultWidget(recent);
    addAction(m_recentAction);

    addSeparator();
    m_moreColorsAction = addAction(tr("&More Colors..."));
    m_askColor = [this](const QColor& initial) {
        return dialogs::ColorDialog::getColor(
            initial, parentWidget(),
            m_kind == Kind::Text ? tr("The color of the letters of the selected text.")
                                 : tr("The color behind the selected text."));
    };
    connect(m_moreColorsAction, &QAction::triggered, this, [this]() {
        const QColor current = m_currentColor ? m_currentColor() : QColor();
        if (const std::optional<QColor> chosen =
                m_askColor(current.isValid() ? current : m_lastColor)) {
            choose(*chosen);
        }
    });

    for (ColorSwatches* swatches : {m_palette, m_recent}) {
        connect(swatches, &ColorSwatches::colorChosen, this, &ColorMenu::choose);
        connect(swatches, &ColorSwatches::entered, this,
                [this, swatches]() { enterSwatches(swatches); });
    }

    // The icons, in the colors of the icons after they change
    const auto showIcons = [this]() {
        auto& art = core::ArtProvider::getInstance();
        m_noColorAction->setIcon(m_kind == Kind::Text
                                     ? swatchIcon(palette().color(QPalette::Text),
                                                  palette().color(QPalette::Mid))
                                     : art.getIcon(QStringLiteral("format.noColor")));
        m_moreColorsAction->setIcon(art.getIcon(QStringLiteral("format.moreColors")));
    };
    showIcons();
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            showIcons);

    connect(this, &QMenu::aboutToShow, this, &ColorMenu::refresh);
    refresh();
}

void ColorMenu::refresh()
{
    const dialogs::ColorPalette palette = dialogs::ColorDialog::lastPalette();
    QList<QColor> colors;
    QStringList names;
    for (const dialogs::NamedColor& named : palette.colors) {
        colors.append(named.color);
        names.append(named.name);
    }
    m_palette->setColors(colors, names);

    // The recent colors are named after the palettes, as in the color window
    const QList<dialogs::ColorPalette> palettes = dialogs::colorPalettes();
    QList<QColor> recent;
    QStringList recentNames;
    for (const QString& name : dialogs::ColorDialog::recentColors()) {
        const QColor color(name);
        recent.append(color);
        recentNames.append(dialogs::paletteColorName(palettes, color));
    }
    m_recent->setColors(recent, recentNames);
    m_recentAction->setVisible(!recent.isEmpty());

    const QColor current = m_currentColor ? m_currentColor() : QColor();
    m_palette->setMarkedColor(current);
    m_recent->setMarkedColor(current);
}

void ColorMenu::choose(const QColor& color)
{
    if (color.isValid()) {
        m_lastColor = color;
    }
    // The menu and the menus it was opened from close, as after an item was chosen
    for (int open = 0; open < 8 && QApplication::activePopupWidget() != nullptr; ++open) {
        QApplication::activePopupWidget()->close();
    }
    hide();
    emit colorChosen(color);
}

void ColorMenu::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        m_upward = event->key() == Qt::Key_Up;
    }
    QMenu::keyPressEvent(event);
}

void ColorMenu::enterSwatches(ColorSwatches* swatches)
{
    if (swatches->currentIndex() >= 0 || swatches->count() == 0) {
        return;
    }
    const QColor current = m_currentColor ? m_currentColor() : QColor();
    const int marked = swatches->indexOf(current);
    if (marked >= 0) {
        swatches->setCurrentIndex(marked);
        return;
    }
    // Up from below: the last row; else the first one
    const int lastRow = (swatches->count() - 1) / swatches->columns();
    swatches->setCurrentIndex(m_upward ? lastRow * swatches->columns() : 0);
}

} // namespace gui
} // namespace kalahari
