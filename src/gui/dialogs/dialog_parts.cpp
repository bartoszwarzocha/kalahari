/// @file dialog_parts.cpp
/// @brief Parts that the program's own dialogs share: a list as high as its rows, a note with
/// an icon, a link, the box of the tools of the types, and the Copy button

#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/gui/panels/annotation_colors.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int NOTE_ICON_SIZE = 16;      ///< Size of the icon of a note
constexpr int NOTE_SPACING = 8;         ///< Between the icon of a note and its sentence
constexpr int COPIED_NOTICE_MS = 2000;  ///< How long Copy says "Copied"
constexpr int TOOLS_MARGIN_X = 12;      ///< At the sides of the box of the tools
constexpr int TOOLS_MARGIN_Y = 8;       ///< Above and below the texts of the box
constexpr int TOOLS_SPACING = 8;        ///< Between the icon and the texts of the box
constexpr int TOOLS_LINE_SPACING = 2;   ///< Between the title and the sentence of the box
constexpr qreal TOOLS_FILL = 0.04;      ///< Mid in the background of the box
constexpr qreal TOOLS_BORDER = 0.3;     ///< Mid in the border of the box

/// @brief The box of the tools of the types: a little darker than the window, with a border
class ToolsBox : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        QPainter painter(this);
        const QColor window = palette().color(QPalette::Window);
        const QColor mid = palette().color(QPalette::Mid);
        painter.setPen(QPen(mixedColor(window, mid, TOOLS_BORDER), 1));
        painter.setBrush(mixedColor(window, mid, TOOLS_FILL));
        painter.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }
};

} // anonymous namespace

void RowList::setRows(int rows) {
    m_rows = rows;
    updateGeometry();
}

QSize RowList::sizeHint() const {
    return {QTreeWidget::sizeHint().width(), heightFor(qBound(1, m_rows, MAXIMUM_ROWS))};
}

QSize RowList::minimumSizeHint() const {
    return {QTreeWidget::minimumSizeHint().width(), heightFor(qBound(1, m_rows, MINIMUM_ROWS))};
}

int RowList::heightFor(int rows) const {
    const int measured = topLevelItemCount() > 0 ? sizeHintForRow(0) : 0;
    const int rowHeight =
        qMax(measured, qMax(fontMetrics().height() + 6, iconSize().height() + 4));
    return rowHeight * rows + 2 * frameWidth();
}

RowList* newRowList(QWidget* parent, const QString& accessibleName) {
    auto* list = new RowList(parent);
    list->setColumnCount(2);
    list->setHeaderHidden(true);
    list->setRootIsDecorated(false);
    list->setItemsExpandable(false);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    list->setAccessibleName(accessibleName);
    list->header()->setStretchLastSection(false);
    list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    return list;
}

QWidget* newNote(QWidget* parent, QLabel** icon, QLabel** text) {
    auto* note = new QWidget(parent);
    auto* layout = new QHBoxLayout(note);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(NOTE_SPACING);
    *icon = new QLabel(note);
    (*icon)->setAlignment(Qt::AlignTop);
    layout->addWidget(*icon);
    *text = new QLabel(note);
    (*text)->setTextFormat(Qt::PlainText);
    (*text)->setWordWrap(true);
    (*text)->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(*text, 1);
    return note;
}

void setNoteIcon(QLabel* icon, const QString& iconId) {
    icon->setPixmap(core::ArtProvider::getInstance().getIcon(iconId).pixmap(NOTE_ICON_SIZE,
                                                                            NOTE_ICON_SIZE));
}

QLabel* newLink(const QString& text, QWidget* parent) {
    auto* link = new QLabel(parent);
    link->setTextFormat(Qt::RichText);
    link->setText(QStringLiteral("<a href=\"#\">%1</a>").arg(text.toHtmlEscaped()));
    link->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    return link;
}

QWidget* newToolsBox(QWidget* parent) {
    auto* tools = new ToolsBox(parent);
    auto* toolsLayout = new QHBoxLayout(tools);
    toolsLayout->setContentsMargins(TOOLS_MARGIN_X, TOOLS_MARGIN_Y, TOOLS_MARGIN_X,
                                    TOOLS_MARGIN_Y);
    toolsLayout->setSpacing(TOOLS_SPACING);
    auto* toolsIcon = new QLabel(tools);
    toolsIcon->setAlignment(Qt::AlignTop);
    setNoteIcon(toolsIcon, QStringLiteral("help.about"));
    toolsLayout->addWidget(toolsIcon);
    auto* toolsTexts = new QVBoxLayout();
    toolsTexts->setSpacing(TOOLS_LINE_SPACING);
    //: The tools that book types will have, e.g. a plot board. In Polish: Narzędzia typów
    auto* toolsTitle =
        new QLabel(QCoreApplication::translate("DialogParts", "Tools of the Types"), tools);
    QFont bold;  // only the weight: the rest follows the font of the window
    bold.setWeight(QFont::Bold);
    toolsTitle->setFont(bold);
    toolsTexts->addWidget(toolsTitle);
    //: In Polish: Żaden typ nie ma jeszcze narzędzi. Gdy powstaną (np. tablica fabuły
    //: i słownik książki), wybierzesz je tutaj.
    auto* toolsText = new QLabel(
        QCoreApplication::translate("DialogParts",
                                    "No type has tools yet. When they come (e.g. a plot board "
                                    "and a book dictionary), you will choose them here."),
        tools);
    toolsText->setWordWrap(true);
    toolsText->setForegroundRole(QPalette::PlaceholderText);
    toolsTexts->addWidget(toolsText);
    toolsLayout->addLayout(toolsTexts, 1);

    // New icon colors or sizes (a change of theme or icon settings) redraw the icon of the box
    QObject::connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
                     toolsIcon,
                     [toolsIcon]() { setNoteIcon(toolsIcon, QStringLiteral("help.about")); });
    return tools;
}

void setUpCopyButton(QPushButton* button, const std::function<QString()>& text) {
    const QString name = QCoreApplication::translate("DialogParts", "&Copy");
    const QString copied = QCoreApplication::translate("DialogParts", "Copied");
    button->setToolTip(
        QCoreApplication::translate("DialogParts", "Copy what the window says to the clipboard"));
    // As wide as its longest text, so "Copied" does not move the other buttons
    button->setText(copied);
    const int copiedWidth = button->sizeHint().width();
    button->setText(name);
    button->setMinimumWidth(std::max(copiedWidth, button->sizeHint().width()));
    QObject::connect(button, &QPushButton::clicked, button, [button, text, name, copied]() {
        QApplication::clipboard()->setText(text());
        button->setText(copied);
        QTimer::singleShot(COPIED_NOTICE_MS, button, [button, name]() { button->setText(name); });
    });
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
