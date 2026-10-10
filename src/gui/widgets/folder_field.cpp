/// @file folder_field.cpp
/// @brief Implementation of FolderField

#include "kalahari/gui/widgets/folder_field.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

namespace {

/// @brief How long typing pauses before the line under the field follows it
constexpr int STATE_DELAY_MS = 300;

}  // namespace

FolderField::FolderField(QWidget* parent)
    : QWidget(parent)
    , m_edit(new QLineEdit(this))
    , m_chooseButton(new QPushButton(tr("Choose..."), this))
    , m_restoreButton(new QPushButton(tr("Restore Default"), this))
    , m_stateIcon(new QLabel(this))
    , m_stateText(new QLabel(this))
    , m_stateTimer(new QTimer(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* pathRow = new QHBoxLayout();
    pathRow->addWidget(m_edit, 1);
    m_chooseButton->setToolTip(tr("Choose the folder in the system window"));
    pathRow->addWidget(m_chooseButton);
    pathRow->addWidget(m_restoreButton);
    layout->addLayout(pathRow);

    auto* stateRow = new QHBoxLayout();
    m_stateIcon->setFixedSize(16, 16);
    m_stateIcon->hide();
    stateRow->addWidget(m_stateIcon, 0, Qt::AlignTop);
    m_stateText->setObjectName(QStringLiteral("folderFieldState"));
    m_stateText->setTextFormat(Qt::PlainText);
    m_stateText->setWordWrap(true);
    m_stateText->hide();
    stateRow->addWidget(m_stateText, 1);
    layout->addLayout(stateRow);

    m_stateTimer->setSingleShot(true);
    m_stateTimer->setInterval(STATE_DELAY_MS);
    connect(m_stateTimer, &QTimer::timeout, this, &FolderField::showState);
    connect(m_edit, &QLineEdit::textChanged, this, [this]() {
        updateRestoreButton();
        m_stateTimer->start();
        emit folderChanged();
    });
    connect(m_chooseButton, &QPushButton::clicked, this, &FolderField::choose);
    connect(m_restoreButton, &QPushButton::clicked, this,
            [this]() { setFolder(m_defaultFolder); });
}

void FolderField::setDefaultFolder(const QString& folder) {
    m_defaultFolder = ProgramFolders::cleanPath(folder);
    m_edit->setPlaceholderText(QDir::toNativeSeparators(m_defaultFolder));
    m_restoreButton->setToolTip(tr("Put the default folder in the field: %1")
                                    .arg(QDir::toNativeSeparators(m_defaultFolder)));
    updateRestoreButton();
}

void FolderField::setChooseTitle(const QString& title) {
    m_chooseTitle = title;
}

QString FolderField::folder() const {
    const QString typed = ProgramFolders::cleanPath(m_edit->text());
    return typed.isEmpty() ? m_defaultFolder : typed;
}

void FolderField::setFolder(const QString& folder) {
    m_edit->setText(QDir::toNativeSeparators(ProgramFolders::cleanPath(folder)));
    showState();
}

QString FolderField::problem() const {
    return ProgramFolders::problemOf(folder());
}

QString FolderField::stateText() const {
    return m_stateText->isVisibleTo(this) ? m_stateText->text() : QString();
}

void FolderField::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        showState();
    }
}

void FolderField::choose() {
    // A folder that does not exist yet opens the window in the nearest one that does
    QString start = ProgramFolders::existingFolderAt(folder());
    if (start.isEmpty()) {
        start = ProgramFolders::documentsFolder();
    }
    const QString chosen = QFileDialog::getExistingDirectory(
        this, m_chooseTitle, start, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!chosen.isEmpty()) {
        setFolder(chosen);
    }
}

void FolderField::updateRestoreButton() {
    m_restoreButton->setEnabled(!ProgramFolders::samePath(folder(), m_defaultFolder));
}

void FolderField::showState() {
    m_stateTimer->stop();
    QString text;
    bool warning = false;
    if (isEnabled()) {
        text = problem();
        warning = !text.isEmpty();
        if (!warning && !QFileInfo(folder()).isDir()) {
            text = tr("Kalahari will create this folder when it is first needed.");
        }
    }

    m_stateIcon->setPixmap(
        warning ? core::ArtProvider::getInstance()
                      .getIcon(QStringLiteral("common.warning"))
                      .pixmap(16, 16)
                : QPixmap());
    m_stateIcon->setVisible(warning);
    // A problem reads like the other texts of the page, a note about a new folder is muted
    m_stateText->setStyleSheet(
        warning ? QString()
                : QStringLiteral("color: %1;")
                      .arg(core::ThemeManager::getInstance()
                               .getCurrentTheme()
                               .palette.placeholderText.name()));
    m_stateText->setText(text);
    m_stateText->setVisible(!text.isEmpty());
}

}  // namespace gui
}  // namespace kalahari
