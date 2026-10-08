/// @file kalahari_dialog.cpp
/// @brief The base of the program's own dialogs: one look for all of them

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/core/art_provider.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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
    m_contentLayout = new QVBoxLayout();
    m_contentLayout->setSpacing(SPACING);
    body->addLayout(m_contentLayout, 1);

    auto* buttons = new QDialogButtonBox(this);
    m_acceptButton = buttons->addButton(tr("OK"), QDialogButtonBox::AcceptRole);
    m_cancelButton = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    m_applyButton = buttons->addButton(tr("Apply"), QDialogButtonBox::ApplyRole);
    m_applyButton->hide();
    m_acceptButton->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_applyButton, &QPushButton::clicked, this, &KalahariDialog::applyClicked);
    body->addWidget(buttons);
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
    QSize hint = QDialog::sizeHint().expandedTo(minimumSize());
    if (layout()->hasHeightForWidth()) {
        hint.setHeight(layout()->totalHeightForWidth(hint.width()));
    }
    return hint;
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
    m_iconLabel->setPixmap(icon.pixmap(QSize(size, size), devicePixelRatioF()));
    m_iconLabel->show();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
