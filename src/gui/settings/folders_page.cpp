/// @file folders_page.cpp
/// @brief Settings page: Files > Folders
///
/// The folders of the books and of the archives are kept empty while they are the default
/// ones, so that they follow the Documents folder. The windows choosing files start in them
/// until they remember the folder used last (ProgramFolders). The backups of the database of
/// a book are made when it is closed (core::ProjectManager::closeProject()).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/backup_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/utils/program_folders.h"
#include "kalahari/gui/widgets/folder_field.h"

#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

using json = nlohmann::json;

FoldersPage::FoldersPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* books = addGroup(tr("Books"));
    addFolder(books, tr("Books folder:"), ProgramFolders::BOOKS_SETTING,
              ProgramFolders::defaultBooksFolder(), tr("Choose the Folder of the Books"));
    addNote(books, tr("The New Book window suggests this folder for a new book, and the Open "
                      "Book window and the window choosing where an imported book goes open in "
                      "it. Later each of them starts in the folder chosen in it last, until this "
                      "setting changes."));

    QFormLayout* archives = addGroup(tr("Archives"));
    addFolder(archives, tr("Archives folder:"), ProgramFolders::ARCHIVES_SETTING,
              ProgramFolders::defaultArchivesFolder(), tr("Choose the Folder of the Archives"));
    addNote(archives, tr("The windows exporting a book and choosing an archive to import open "
                         "in this folder. Later each of them starts in the folder chosen in it "
                         "last, until this setting changes."));

    QFormLayout* backups = addGroup(tr("Database Backups"));
    m_inEachBook = new QRadioButton(tr("In the folder of each book (its .backups folder)"));
    m_inEachBook->setObjectName(QStringLiteral("backupsInEachBook"));
    m_inOneFolder = new QRadioButton(tr("In one folder for all books:"));
    m_inOneFolder->setObjectName(QStringLiteral("backupsInOneFolder"));
    backups->addRow(m_inEachBook);
    backups->addRow(m_inOneFolder);

    m_backupFolder = new FolderField();
    m_backupFolder->setObjectName(QStringLiteral("backupFolder"));
    m_backupFolder->setDefaultFolder(ProgramFolders::defaultBackupsFolder());
    m_backupFolder->setChooseTitle(tr("Choose the Folder of the Backups"));
    m_backupFolder->lineEdit()->setAccessibleName(tr("Folder of the backups of all books"));
    // Under the option it belongs to
    auto* folderRow = new QWidget();
    auto* folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
                                         style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing),
                                     0, 0, 0);
    folderLayout->addWidget(m_backupFolder);
    backups->addRow(folderRow);
    connect(m_inOneFolder, &QRadioButton::toggled, m_backupFolder, &QWidget::setEnabled);

    // One setting: empty for the folder of each book, else the common folder
    auto& settings = core::SettingsManager::getInstance();
    Binding backupFolder;
    backupFolder.key = []() { return std::string(core::BackupManager::FOLDER_SETTING); };
    backupFolder.stored = [&settings]() {
        return json(settings.get<std::string>(core::BackupManager::FOLDER_SETTING));
    };
    backupFolder.shown = [this]() {
        return json(m_inOneFolder->isChecked() ? m_backupFolder->folder().toStdString()
                                               : std::string());
    };
    backupFolder.show = [this](const json& value) {
        const QString folder = ProgramFolders::cleanPath(
            QString::fromStdString(value.is_string() ? value.get<std::string>() : std::string()));
        (folder.isEmpty() ? m_inEachBook : m_inOneFolder)->setChecked(true);
        m_backupFolder->setFolder(folder.isEmpty() ? m_backupFolder->defaultFolder() : folder);
        m_backupFolder->setEnabled(!folder.isEmpty());
    };
    backupFolder.store = [&settings](const json& value) {
        settings.set<json>(core::BackupManager::FOLDER_SETTING, value);
    };
    m_folders.push_back({m_backupFolder, &bindCustom(std::move(backupFolder))});

    auto* count = new QSpinBox();
    count->setObjectName(QStringLiteral("backupCount"));
    count->setRange(1, 100);
    count->setToolTip(tr("How many of the newest backups of each book Kalahari keeps; it "
                         "deletes the older ones"));
    addField(backups, tr("Copies to keep:"), count, core::BackupManager::COUNT_SETTING);
    addNote(backups, tr("When a book is closed, Kalahari copies its database (project.db) and "
                        "keeps the given number of the newest copies. In the common folder each "
                        "book has a folder of its own. The copies made before a change stay "
                        "where they are."));

    pageLayout()->addStretch();
}

void FoldersPage::addFolder(QFormLayout* form, const QString& label, const char* key,
                            const QString& defaultFolder, const QString& chooseTitle) {
    auto* field = new FolderField();
    field->setObjectName(QString::fromLatin1(key));
    field->setDefaultFolder(defaultFolder);
    field->setChooseTitle(chooseTitle);
    auto* labelWidget = new QLabel(label);
    labelWidget->setBuddy(field->lineEdit());
    // The path gets the whole width of the page, under its label
    form->addRow(labelWidget);
    form->addRow(field);

    // The default folder is kept as an empty setting, which follows the Documents folder
    auto& settings = core::SettingsManager::getInstance();
    Binding binding;
    binding.key = [key]() { return std::string(key); };
    binding.stored = [&settings, key]() { return json(settings.get<std::string>(key)); };
    binding.shown = [field]() {
        const QString folder = field->folder();
        return json(ProgramFolders::samePath(folder, field->defaultFolder())
                        ? std::string()
                        : folder.toStdString());
    };
    binding.show = [field](const json& value) {
        const QString folder =
            QString::fromStdString(value.is_string() ? value.get<std::string>() : std::string());
        field->setFolder(folder.trimmed().isEmpty() ? field->defaultFolder() : folder);
    };
    binding.store = [&settings, key](const json& value) { settings.set<json>(key, value); };
    m_folders.push_back({field, &bindCustom(std::move(binding))});
}

std::vector<std::string> FoldersPage::apply() {
    std::vector<std::string> written = SettingsPage::apply();
    for (const std::string& key : written) {
        ProgramFolders::forgetFolders(key);
    }
    return written;
}

SettingsPage::Problem FoldersPage::problem() const {
    for (const CheckedFolder& checked : m_folders) {
        // A folder that was not changed keeps its place, also on a drive that is not connected
        if (!checked.field->isEnabled() || !bindingChanged(*checked.binding)) {
            continue;
        }
        const QString reason = checked.field->problem();
        if (!reason.isEmpty()) {
            return {tr("The folder '%1' cannot be used. %2")
                        .arg(QDir::toNativeSeparators(checked.field->folder()), reason),
                    checked.field->lineEdit()};
        }
    }
    return {};
}

}  // namespace gui
}  // namespace kalahari
