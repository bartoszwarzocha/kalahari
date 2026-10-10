/// @file kalahari_dialog.cpp
/// @brief The base of the program's own dialogs: one look for all of them

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/gui/widgets/fitting_scroll_area.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int MARGIN = 20;                ///< Around the heading and around the content
constexpr int SPACING = 12;               ///< Between the parts of the content
constexpr int LABEL_SPACING = 4;          ///< Between a label and its field, a title and its sentence
constexpr int COMPACT_MARGIN = 8;         ///< Above and below a compact heading
constexpr int MINIMUM_WIDTH = 460;        ///< Room for a title in a field
constexpr qreal TITLE_SCALE = 1.3;        ///< Size of the heading's title against the dialog's font
constexpr qreal COMPACT_TITLE_SCALE = 1.1;

} // anonymous namespace

KalahariDialog::KalahariDialog(QWidget* parent)
    : QDialog(parent)
{
    setMinimumWidth(MINIMUM_WIDTH);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Heading: on the background of text fields, with a line under it
    auto* heading = new QFrame(this);
    heading->setObjectName(QStringLiteral("kalahariDialogHeading"));
    heading->setAutoFillBackground(true);
    heading->setBackgroundRole(QPalette::Base);
    m_headingLayout = new QHBoxLayout(heading);
    m_headingLayout->setSpacing(SPACING + LABEL_SPACING);

    m_iconLabel = new QLabel(heading);
    m_iconLabel->hide();
    m_headingLayout->addWidget(m_iconLabel, 0, Qt::AlignTop);

    // Both texts in the color of text in fields: the description is read, not skimmed
    auto* headingTexts = new QVBoxLayout();
    headingTexts->setSpacing(LABEL_SPACING);
    m_titleLabel = new QLabel(heading);
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setForegroundRole(QPalette::Text);
    headingTexts->addWidget(m_titleLabel);

    m_descriptionLabel = new QLabel(heading);
    m_descriptionLabel->setTextFormat(Qt::PlainText);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setForegroundRole(QPalette::Text);
    m_descriptionLabel->hide();
    headingTexts->addWidget(m_descriptionLabel);
    m_headingLayout->addLayout(headingTexts, 1);
    layout->addWidget(heading);

    auto* line = new QFrame(this);
    line->setFixedHeight(1);
    line->setAutoFillBackground(true);
    line->setBackgroundRole(QPalette::Mid);
    layout->addWidget(line);

    // Content, which gets the room of a resized dialog, then the buttons
    auto* body = new QVBoxLayout();
    body->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    body->setSpacing(MARGIN);
    auto* content = new QWidget();
    m_contentLayout = new QVBoxLayout(content);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(SPACING);
    m_contentArea = new FittingScrollArea(content, this);
    m_contentArea->viewport()->setAutoFillBackground(false);
    body->addWidget(m_contentArea, 1);

    m_buttonBox = new QDialogButtonBox(this);
    m_acceptButton = m_buttonBox->addButton(tr("OK"), QDialogButtonBox::AcceptRole);
    m_cancelButton = m_buttonBox->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_applyButton = m_buttonBox->addButton(tr("Apply"), QDialogButtonBox::ApplyRole);
    m_applyButton->hide();
    m_acceptButton->setDefault(true);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_applyButton, &QPushButton::clicked, this, &KalahariDialog::applyClicked);
    // Buttons that do not close the dialog go at the left end, the button box on the rest
    auto* buttonRow = new QHBoxLayout();
    m_sideButtonLayout = new QHBoxLayout();
    buttonRow->addLayout(m_sideButtonLayout);
    buttonRow->addWidget(m_buttonBox, 1);
    body->addLayout(buttonRow);
    layout->addLayout(body);

    updateHeadingStyle();

    // New icon colors or sizes (a change of theme or icon settings) redraw the icon
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            this, &KalahariDialog::updateHeadingIcon);
}

void KalahariDialog::setHeading(const QString& title, const QString& description)
{
    setWindowTitle(title);
    m_titleLabel->setText(title);
    m_descriptionLabel->setText(description);
    m_descriptionLabel->setVisible(!description.isEmpty());
}

void KalahariDialog::setHeadingIcon(const QString& iconId)
{
    m_iconId = iconId;
    updateHeadingIcon();
}

void KalahariDialog::setCompactHeading(bool compact)
{
    m_compactHeading = compact;
    updateHeadingStyle();
}

void KalahariDialog::addField(const QString& label, QWidget* field)
{
    auto* fieldLabel = new QLabel(label, this);
    fieldLabel->setTextFormat(Qt::PlainText);
    fieldLabel->setBuddy(field);

    auto* fieldLayout = new QVBoxLayout();
    fieldLayout->setSpacing(LABEL_SPACING);
    fieldLayout->addWidget(fieldLabel);
    fieldLayout->addWidget(field);
    m_contentLayout->addLayout(fieldLayout);
}

QPushButton* KalahariDialog::addSideButton(const QString& text)
{
    auto* button = new QPushButton(text, this);
    button->setAutoDefault(false);
    m_sideButtonLayout->addWidget(button);
    return button;
}

void KalahariDialog::setAcceptText(const QString& text)
{
    m_acceptButton->setText(text);
}

void KalahariDialog::setApplyButtonVisible(bool visible)
{
    m_applyButton->setVisible(visible);
}

QSize KalahariDialog::sizeHint() const
{
    // The plain hint is narrower than the minimum width, and its height is for that
    // width: wrapped heading texts take more lines there, which would leave a gap
    // above the buttons of the wider dialog
    // The content's own wrapped texts are measured at its own, narrower hint: at the
    // dialog's width they take fewer lines
    QSize hint = QDialog::sizeHint().expandedTo(minimumSize());
    const int height = layout()->hasHeightForWidth() ? layout()->totalHeightForWidth(hint.width())
                                                     : hint.height();
    const int contentWidth = hint.width() - 2 * MARGIN;
    hint.setHeight(height - m_contentArea->sizeHint().height() +
                   m_contentArea->contentHeightForWidth(contentWidth));
    return hint;
}

void KalahariDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (event->spontaneous()) {
        return;
    }
    // A dialog that was not given a size gets all its content asks for, where Qt would
    // stop at two thirds of the screen
    fitToScreen(testAttribute(Qt::WA_Resized) ? size() : sizeHint());
}

void KalahariDialog::fitToScreen(const QSize& wanted)
{
    const QScreen* screen = this->screen();
    // Content that changed just now (e.g. details hidden) has not told the area yet, and
    // the dialog would keep the minimum size of the old content
    m_contentArea->updateGeometry();
    layout()->activate();
    if (!screen) {
        resize(wanted);
        return;
    }
    // Room for the window's title bar and border, which the system adds around it
    const QRect available = screen->availableGeometry() - frameMargins();

    const QSize fitting = wanted.boundedTo(available.size()).expandedTo(minimumSize());
    if (fitting != size()) {
        resize(fitting);
    }
    QRect placed = geometry();
    placed.moveLeft(std::clamp(placed.left(), available.left(),
                               std::max(available.left(), available.right() - placed.width() + 1)));
    placed.moveTop(std::clamp(placed.top(), available.top(),
                              std::max(available.top(), available.bottom() - placed.height() + 1)));
    if (placed.topLeft() != geometry().topLeft()) {
        move(placed.topLeft() - (geometry().topLeft() - pos()));
    }
}

QMargins KalahariDialog::frameMargins() const
{
    // The system knows its frame once the window exists (a title bar of about 31 px and a
    // border of 8 px on Windows); before that, or without a frame, the style's guess
    const QWindow* window = windowHandle();
    if (window) {
        const QMargins margins = window->frameMargins();
        if (!margins.isNull()) {
            return margins;
        }
    }
    const int border = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, this);
    const int titleBar = style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, this);
    return {border, titleBar + border, border, border};
}

void KalahariDialog::updateHeadingStyle()
{
    const int margin = m_compactHeading ? COMPACT_MARGIN : MARGIN;
    m_headingLayout->setContentsMargins(MARGIN, margin, MARGIN, margin);

    QFont titleFont = font();
    titleFont.setPointSizeF(titleFont.pointSizeF() *
                            (m_compactHeading ? COMPACT_TITLE_SCALE : TITLE_SCALE));
    titleFont.setWeight(QFont::DemiBold);
    m_titleLabel->setFont(titleFont);

    updateHeadingIcon();
}

void KalahariDialog::updateHeadingIcon()
{
    if (m_iconId.isEmpty()) {
        m_iconLabel->clear();
        m_iconLabel->hide();
        return;
    }

    auto& artProvider = core::ArtProvider::getInstance();
    const core::IconContext context =
        m_compactHeading ? core::IconContext::Panel : core::IconContext::Dialog;
    const int size = artProvider.getIconSize(context);
    const QIcon icon = artProvider.getIcon(m_iconId, context);
    // No gap before the title when the icon is not there (e.g. before the icons are
    // registered at startup)
    m_iconLabel->setPixmap(icon.pixmap(QSize(size, size), devicePixelRatioF()));
    m_iconLabel->setVisible(!icon.isNull());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
