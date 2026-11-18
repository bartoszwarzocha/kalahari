// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file project_manager_series.cpp
/// @brief ProjectManager: a series, several books of one project with one Workshop – its name,
/// new books, their order and removal, a book taken out of it and a book joined to it

#include "kalahari/core/project_manager.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_database.h"
#include "kalahari/core/volume_copies.h"
#include "kalahari/core/volume_materials.h"
#include "kalahari/core/writing_goals.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLatin1String>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUuid>

#include <algorithm>
#include <functional>
#include <optional>
#include <ranges>

namespace kalahari::core {

namespace {

/// The most books a project looks for a free folder among: book2 ... book9999
constexpr int MOST_BOOKS = 9999;

/// The file of the database of a project, in its folder (ProjectDatabase)
constexpr const char* DATABASE_FILE = "project.db";

/// The goals of the book in folder @p bookDir (BookGoals), read from a copy of its database:
/// opening a database may change it, and the book stays as it is; empty: none
QString goalsOfBook(const QString& bookDir) {
    const QString database = QDir(bookDir).filePath(QString::fromLatin1(DATABASE_FILE));
    const QTemporaryDir copy;
    if (!QFileInfo::exists(database) || !copy.isValid() ||
        !QFile::copy(database, QDir(copy.path()).filePath(QString::fromLatin1(DATABASE_FILE)))) {
        return {};
    }
    ProjectDatabase other;
    if (!other.open(copy.path())) {
        return {};
    }
    QString goals = other.getSetting(QString::fromLatin1(BookGoals::SETTING_KEY)).toString();
    other.close();
    return goals;
}

/// Whether @p material can be for volume @p bookId of @p project: a material of the cover or a
/// text for publishers, and a book of the project, or the whole series (an empty id)
bool canBeFor(const BookProject& project, const ProjectElement* material,
              const QString& bookId) {
    return material != nullptr && VolumeMaterials::isVolumeMaterial(*material) &&
           (bookId.isEmpty() || project.bookIndex(bookId) >= 0);
}

/// Whether the template of @p kind has the title or the author of the book, as the template of
/// the title page has
bool templateNamesBook(const KindRef& kind) {
    if (!kind || kind.kind->templateFile.isEmpty()) {
        return false;
    }
    const std::optional<ChapterDocument> chapter = ChapterDocument::load(
        QDir(kind.package->directory).filePath(kind.kind->templateFile));
    return chapter && (chapter->kml().contains(QLatin1String("{title}")) ||
                       chapter->kml().contains(QLatin1String("{author}")));
}

/// Call @p visit for each element of @p elements and for the elements inside them, in reading
/// order (each group before the elements inside it)
void forEachIn(QList<ProjectElement>& elements,
               const std::function<void(ProjectElement&)>& visit) {
    // The elements still to visit, the next one last
    QList<ProjectElement*> waiting;
    for (ProjectElement& element : std::views::reverse(elements)) {
        waiting.append(&element);
    }
    while (!waiting.isEmpty()) {
        ProjectElement* element = waiting.takeLast();
        visit(*element);
        for (ProjectElement& inside : std::views::reverse(element->elements)) {
            waiting.append(&inside);
        }
    }
}

/// The kind of the first text element of @p elements in reading order, also inside their
/// groups; none when they have no text element
KindRef firstTextKind(const BookTypeRegistry& registry, const QList<ProjectElement>& elements) {
    QList<const ProjectElement*> waiting;
    for (const ProjectElement& element : std::views::reverse(elements)) {
        waiting.append(&element);
    }
    while (!waiting.isEmpty()) {
        const ProjectElement* element = waiting.takeLast();
        if (BookProject::formOf(registry, *element) == ElementForm::Text) {
            return BookProject::kindOf(registry, *element);
        }
        for (const ProjectElement& inside : std::views::reverse(element->elements)) {
            waiting.append(&inside);
        }
    }
    return {};
}

/// Call @p visit for each element of @p project: of its books and of its Workshop
void forEachOf(BookProject& project, const std::function<void(ProjectElement&)>& visit) {
    for (ProjectBook& book : project.books) {
        forEachIn(book.frontElements, visit);
        forEachIn(book.mainElements, visit);
        forEachIn(book.backElements, visit);
        forEachIn(book.files, visit);
    }
    forEachIn(project.workshop.elements, visit);
}

/// The ids of the elements of book @p index of @p project, and the links of its sections
/// (BookProject::sectionLink())
QSet<QString> linkTargetsOf(BookProject& project, qsizetype index) {
    QSet<QString> targets;
    ProjectBook& book = project.books[index];
    const auto add = [&targets](const ProjectElement& element) { targets.insert(element.id); };
    forEachIn(book.frontElements, add);
    forEachIn(book.mainElements, add);
    forEachIn(book.backElements, add);
    forEachIn(book.files, add);
    for (const BookPlace place : {BookPlace::Front, BookPlace::Main, BookPlace::Back}) {
        targets.insert(project.sectionLink(place, index));
    }
    return targets;
}

/// Remove the links of the elements of @p project to @p targets
/// @return Whether an element lost a link
bool unlinkFrom(BookProject& project, const QSet<QString>& targets) {
    bool lost = false;
    forEachOf(project, [&targets, &lost](ProjectElement& element) {
        if (element.links.removeIf([&targets](const ElementLink& link) {
                return targets.contains(link.to);
            }) > 0) {
            lost = true;
        }
    });
    return lost;
}

/// Whether an element of the sections of @p book has a link
bool hasLinks(ProjectBook& book) {
    bool linked = false;
    const auto visit = [&linked](const ProjectElement& element) {
        if (!element.links.isEmpty()) {
            linked = true;
        }
    };
    forEachIn(book.frontElements, visit);
    forEachIn(book.mainElements, visit);
    forEachIn(book.backElements, visit);
    return linked;
}

/// Remove the files @p paths, written for a book that the project did not get
void removeWritten(const QStringList& paths) {
    for (const QString& path : paths) {
        QFile::remove(path);
    }
}

}  // namespace

QString ProjectManager::seriesName() const {
    if (!m_project) {
        return {};
    }
    if (!m_project->series.isEmpty()) {
        return m_project->series;
    }
    return m_project->books.isEmpty() ? QString() : m_project->books.first().title;
}

bool ProjectManager::setSeriesName(const QString& name) {
    if (!m_project || refusesChange("setSeriesName")) {
        return false;
    }
    const QString previous = m_project->series;
    m_project->series = name.trimmed();
    if (m_project->series == previous) {
        return true;
    }
    if (!saveManifest()) {
        m_project->series = previous;
        return false;
    }
    return true;
}

QString ProjectManager::addBook(const NewBook& request) {
    auto& logger = Logger::getInstance();
    if (!m_project || refusesChange("addBook")) {
        return {};
    }
    const QString title = request.title.trimmed();
    if (title.isEmpty()) {
        logger.error("addBook: A book needs a title");
        return {};
    }

    // Its place: after the current book, unless the writer chose another one; it takes the
    // look of the book before it, or of the first book when it goes first
    const qsizetype count = m_project->books.size();
    const qsizetype at =
        request.index < 0 ? currentBookIndex() + 1 : std::min(request.index, count);
    const ProjectBook& source = m_project->books.at(std::max<qsizetype>(at - 1, 0));

    ProjectBook added;
    added.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    added.title = title;
    added.author = request.author;
    added.language = request.language.isEmpty() ? source.language : request.language;
    added.partsLayer = source.partsLayer;
    added.sectionSet = source.sectionSet;
    added.sectionNames = source.sectionNames;
    added.captions = source.captions;
    added.footnotes = source.footnotes;
    added.bibliographyStyle = source.bibliographyStyle;
    added.publisher = source.publisher;
    added.folder = freeBookFolder();

    QStringList written;
    if (!addStartingElements(added, source, request.pagesOfPrevious, written)) {
        removeWritten(written);
        return {};
    }
    // The current book stays the current one also when the new book goes before it; the
    // materials of the cover and the texts for publishers of a book that becomes a series are
    // for its first volume
    m_currentBook = book()->id;
    const QList<ProjectElement> workshop = m_project->workshop.elements;
    VolumeMaterials::toFirstVolume(*m_project);
    m_project->books.insert(at, added);
    if (!saveManifest()) {
        m_project->books.removeAt(at);
        m_project->workshop.elements = workshop;
        removeWritten(written);
        return {};
    }
    logger.info("addBook: Added '{}' (id: {}) as book {} of {}", title.toStdString(),
                added.id.toStdString(), at + 1, count + 1);
    return added.id;
}

bool ProjectManager::moveBook(const QString& bookId, qsizetype index) {
    if (!m_project || refusesChange("moveBook")) {
        return false;
    }
    const qsizetype from = m_project->bookIndex(bookId);
    if (from < 0 || index < 0 || index >= m_project->books.size()) {
        return false;
    }
    if (from == index) {
        return true;
    }
    // The current book stays the current one also when another book goes first
    m_currentBook = book()->id;
    m_project->books.move(from, index);
    if (!saveManifest()) {
        m_project->books.move(index, from);
        return false;
    }
    return true;
}

std::optional<ProjectBook> ProjectManager::removeBook(const QString& bookId) {
    if (!m_project || refusesChange("removeBook")) {
        return std::nullopt;
    }
    const qsizetype index = m_project->bookIndex(bookId);
    if (index < 0 || m_project->books.size() < 2) {
        return std::nullopt;
    }

    // The project as it was comes back when the change cannot be saved
    const BookProject previous = *m_project;
    const bool wasCurrent = currentBookIndex() == index;
    const QSet<QString> gone = linkTargetsOf(*m_project, index);
    ProjectBook removed = m_project->books.takeAt(index);
    VolumeMaterials::release(*m_project, bookId);

    // The links of the other elements to its elements and sections go with it
    const bool unlinked = unlinkFrom(*m_project, gone);
    if (!saveManifest()) {
        *m_project = previous;
        return std::nullopt;
    }

    // The program forgets its elements; their files stay
    const bool linksGone = unlinked || hasLinks(removed);
    for (const QString& id : gone) {
        m_elementStates.remove(id);
    }
    Logger::getInstance().info("removeBook: Removed '{}' (id: {})", removed.title.toStdString(),
                               bookId.toStdString());
    releaseCardNames(gone);
    if (wasCurrent) {
        m_currentBook.clear();
        emit currentBookChanged();
    }
    if (linksGone) {
        emit linksChanged();
    }
    return removed;
}

bool ProjectManager::setMaterialBook(const QString& elementId, const QString& bookId,
                                     QStringList* drafts) {
    if (drafts != nullptr) {
        drafts->clear();
    }
    if (!m_project || refusesChange("setMaterialBook")) {
        return false;
    }
    if (!canBeFor(*m_project, findElement(elementId), bookId)) {
        Logger::getInstance().warn("setMaterialBook: No material {} of the cover or for "
                                   "publishers, or no book {}",
                                   elementId.toStdString(), bookId.toStdString());
        return false;
    }
    // The project as it was, to put back when the file cannot be written; the material is
    // found again after the copy, in the lists of the project's own
    const BookProject before = *m_project;
    ProjectElement* material = findElement(elementId);
    material->bookId = bookId;
    const QStringList others = VolumeMaterials::unchooseOthers(*m_project, *material);
    if (*m_project == before) {
        return true;
    }
    if (!saveManifest()) {
        *m_project = before;
        return false;
    }
    if (drafts != nullptr) {
        *drafts = others;
    }
    // The sets of the volumes have other versions
    emit publishingSetChanged();
    return true;
}

QString ProjectManager::takeOutBook(const QString& bookId, const QString& parentDir,
                                    bool createSubfolder, QStringList* problems) {
    QStringList found;
    const QString projectDir = takeOutFolder(bookId, parentDir, createSubfolder, found);
    QString manifest;
    if (!projectDir.isEmpty()) {
        const bool folderExisted = QFileInfo::exists(projectDir);
        manifest = writeTakenOut(bookId, projectDir, found);
        if (manifest.isEmpty()) {
            removeNewProject(projectDir, folderExisted);
        }
    }
    for (const QString& problem : found) {
        Logger::getInstance().warn("takeOutBook: {}", problem.toStdString());
    }
    if (problems != nullptr) {
        problems->append(found);
    }
    return manifest;
}

QString ProjectManager::takeOutFolder(const QString& bookId, const QString& parentDir,
                                      bool createSubfolder, QStringList& problems) const {
    const qsizetype index = m_project ? m_project->bookIndex(bookId) : -1;
    if (index < 0 || m_project->books.size() < 2) {
        problems.append(QStringLiteral("%1: no such book of a series").arg(bookId));
        return {};
    }
    QString projectDir =
        newProjectFolder(parentDir, m_project->books.at(index).title, createSubfolder);
    if (projectDir.isEmpty() || !canHoldNewProject(projectDir)) {
        problems.append(QStringLiteral("%1: the folder exists and is not empty")
                            .arg(QDir::toNativeSeparators(projectDir)));
        return {};
    }
    return projectDir;
}

QString ProjectManager::writeTakenOut(const QString& bookId, const QString& projectDir,
                                      QStringList& problems) {
    std::optional<TakenOutVolume> taken = VolumeCopies::takeOut(*m_project, bookId);
    const ProjectBook& volume = m_project->books.at(m_project->bookIndex(bookId));
    if (!taken || !QDir().mkpath(projectDir)) {
        problems.append(QStringLiteral("%1: the folder cannot be made")
                            .arg(QDir::toNativeSeparators(projectDir)));
        return {};
    }
    // The copies of the packages of the type, and the book dictionary
    const QString types = QString::fromLatin1(BookProject::TYPES_FOLDER);
    taken->folders.append({types, types});
    taken->files.append({BookDictionary::fileName(), BookDictionary::fileName()});
    if (!VolumeCopies::copy(getProjectPath(), projectDir, taken->files, taken->folders,
                            problems)) {
        return {};
    }
    BookProject& project = taken->project;
    project.created =
        QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch(), QTimeZone::utc());
    project.modified = project.created;
    QString manifest = manifestPathOf(projectDir, volume.title);
    if (!project.save(manifest)) {
        problems.append(QStringLiteral("%1: the file cannot be written")
                            .arg(QDir::toNativeSeparators(manifest)));
        return {};
    }
    // The goals of the book, in its own project.db
    const QString goals = m_database && m_database->isOpen()
                              ? m_database->getSetting(BookGoals::settingKeyOf(volume)).toString()
                              : QString();
    if (!goals.isEmpty()) {
        ProjectDatabase database;
        if (!database.open(projectDir)) {
            problems.append(QStringLiteral("%1: the database cannot be made")
                                .arg(QDir::toNativeSeparators(projectDir)));
            return {};
        }
        database.setSetting(QString::fromLatin1(BookGoals::SETTING_KEY), goals);
        database.close();
    }
    return manifest;
}

QList<ElementPlace> ProjectManager::movePlacesIn(const QString& elementId,
                                                 const QString& bookId) const {
    return m_project ? m_project->movePlacesIn(bookTypes(), elementId, bookId)
                     : QList<ElementPlace>{};
}

QString ProjectManager::otherBookThan(const QString& elementId, const QString& bookId) const {
    const ProjectBook* own = bookOf(elementId);
    return own != nullptr && own->id == bookId ? QString() : bookId;
}

QString ProjectManager::volumeOfNew(const ProjectElement& element) const {
    const ProjectBook* current = book();
    return isSeries() && current != nullptr && VolumeMaterials::isVolumeMaterial(element)
               ? current->id
               : QString();
}

QString ProjectManager::freeBookFolder() const {
    const QDir projectDir(getProjectPath());
    for (int number = 2; number <= MOST_BOOKS; ++number) {
        const QString folder = QStringLiteral("book%1").arg(number);
        const bool used = std::any_of(
            m_project->books.cbegin(), m_project->books.cend(), [&folder](const ProjectBook& each) {
                return each.folder.compare(folder, Qt::CaseInsensitive) == 0;
            });
        if (!used && !projectDir.exists(folder)) {
            return folder;
        }
    }
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool ProjectManager::addStartingElements(ProjectBook& book, const ProjectBook& source,
                                         bool pages, QStringList& written) const {
    if (pages) {
        for (const BookPlace place : {BookPlace::Front, BookPlace::Back}) {
            if (!copyPages(book, source, place, written)) {
                return false;
            }
        }
    }
    for (const KindRef& kind : firstKinds(source)) {
        const std::optional<ProjectElement> first =
            newTextIn(book, kind, kind.kind->defaultTitle(book.language, 1),
                      startingText(kind, book), written);
        if (!first) {
            return false;
        }
        book.mainElements.append(*first);
    }
    return true;
}

bool ProjectManager::copyPages(ProjectBook& book, const ProjectBook& source, BookPlace place,
                               QStringList& written) const {
    const bool front = place == BookPlace::Front;
    const QList<ProjectElement>& pages = front ? source.frontElements : source.backElements;
    QList<ProjectElement>& copies = front ? book.frontElements : book.backElements;
    for (const ProjectElement& page : pages) {
        const KindRef kind = kindOf(page);
        if (!kind || kind.kind->form != ElementForm::Text) {
            continue;
        }
        // The title page anew, with the title of the new book; the others with their text
        const QString kml =
            templateNamesBook(kind) ? startingText(kind, book) : chapterText(page.id);
        const std::optional<ProjectElement> copy = newTextIn(book, kind, page.title, kml, written);
        if (!copy) {
            return false;
        }
        copies.append(*copy);
    }
    return true;
}

QList<KindRef> ProjectManager::firstKinds(const ProjectBook& source) const {
    QList<KindRef> kinds;
    const BookTypeRegistry& registry = bookTypes();
    const std::optional<ProjectType>& type = m_project->type;
    if (type.has_value()) {
        for (const StartElement& start : registry.startElements(type->id)) {
            if (start.place == BookPlace::Main && start.kind &&
                start.kind.kind->form == ElementForm::Text) {
                kinds.append(start.kind);
            }
        }
        return kinds;
    }
    if (const KindRef kind = firstTextKind(registry, source.mainElements)) {
        kinds.append(kind);
    }
    return kinds;
}

std::optional<ProjectElement> ProjectManager::newTextIn(const ProjectBook& book,
                                                        const KindRef& kind,
                                                        const QString& title, const QString& kml,
                                                        QStringList& written) const {
    ProjectElement element;
    element.id = m_project->newElementId();
    element.kind = KindReference{kind.package->id, kind.kind->id};
    element.title = title;
    element.status = QStringLiteral("draft");
    element.file = newChapterFileIn(book.folder, kind.kind->id);

    ChapterDocument chapter;
    chapter.setKml(kml);
    chapter.setTitle(title);
    chapter.setStatus(element.status);
    const QString path = QDir(getProjectPath()).filePath(element.file);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !chapter.save(path)) {
        Logger::getInstance().error("addBook: {} cannot be written",
                                    QDir::toNativeSeparators(path).toStdString());
        return std::nullopt;
    }
    written.append(path);
    return element;
}

// =============================================================================
// A book joined as a volume
// =============================================================================

JoinProblem ProjectManager::joinProblemOf(const BookProject& book) const {
    return m_project ? VolumeCopies::joinProblem(*m_project, book) : JoinProblem::Series;
}

QList<CardPair> ProjectManager::sameCardsOf(const BookProject& book) const {
    if (!m_project) {
        return {};
    }
    const auto card = [this](const ProjectElement& element) { return isCard(element); };
    return VolumeCopies::sameCards(*m_project, book, card);
}

QString ProjectManager::joinBook(const QString& manifestPath, qsizetype index,
                                 const QList<CardPair>& sameCards, QStringList* problems) {
    QStringList found;
    QString bookId;
    if (!m_project || refusesChange("joinBook")) {
        found.append(QStringLiteral("no book that can be changed is open"));
    } else if (const std::optional<BookProject> book = BookProject::load(manifestPath, found)) {
        const QDir projectDir(getProjectPath());
        const JoinRequest request{
            index, freeBookFolder(), sameCards,
            [&projectDir](const QString& path) { return projectDir.exists(path); }};
        bookId = writeJoined(*book, QFileInfo(manifestPath).absolutePath(), request, found);
    }
    for (const QString& problem : found) {
        Logger::getInstance().warn("joinBook: {}", problem.toStdString());
    }
    if (problems != nullptr) {
        problems->append(found);
    }
    return bookId;
}

QString ProjectManager::writeJoined(const BookProject& book, const QString& bookDir,
                                    const JoinRequest& request, QStringList& problems) {
    std::optional<JoinedVolume> joined = VolumeCopies::join(*m_project, book, request);
    if (!joined) {
        problems.append(QStringLiteral("%1: the book cannot join the series")
                            .arg(QDir::toNativeSeparators(bookDir)));
        return {};
    }
    QStringList written;
    if (!VolumeCopies::copy(bookDir, getProjectPath(), joined->files, joined->folders, problems,
                            &written)) {
        removeJoined(written, request.folder);
        return {};
    }
    const BookProject before = *m_project;
    *m_project = joined->project;
    if (!saveManifest()) {
        *m_project = before;
        removeJoined(written, request.folder);
        problems.append(QStringLiteral("the project file cannot be written"));
        return {};
    }
    loadNewElementStates();
    joinDictionaryOf(bookDir, joined->ids);
    joinGoalsOf(bookDir, joined->bookId);
    // The cards of the series that are cards of the book gained the links of theirs
    emit linksChanged();
    Logger::getInstance().info("joinBook: Joined '{}' (id: {}) as book {} of {}",
                               QDir::toNativeSeparators(bookDir).toStdString(),
                               joined->bookId.toStdString(),
                               m_project->bookIndex(joined->bookId) + 1, m_project->books.size());
    return joined->bookId;
}

void ProjectManager::joinDictionaryOf(const QString& bookDir,
                                      const QHash<QString, QString>& cards) {
    const QString path = QDir(bookDir).filePath(BookDictionary::fileName());
    BookDictionary other;
    if (!QFileInfo::exists(path) || !other.load(path) || other.isEmpty()) {
        return;
    }
    changeBookDictionary([&other, &cards](BookDictionary& own) {
        VolumeCopies::joinDictionary(own, other, cards);
    });
}

void ProjectManager::joinGoalsOf(const QString& bookDir, const QString& bookId) {
    const qsizetype index = m_project->bookIndex(bookId);
    const QString goals = goalsOfBook(bookDir);
    if (index >= 0 && !goals.isEmpty() && m_database && m_database->isOpen()) {
        m_database->setSetting(BookGoals::settingKeyOf(m_project->books.at(index)), goals);
    }
}

void ProjectManager::removeJoined(const QStringList& written, const QString& folder) const {
    const QDir projectDir(getProjectPath());
    for (const QString& file : written) {
        QFile::remove(file);
        projectDir.rmpath(projectDir.relativeFilePath(QFileInfo(file).absolutePath()));
    }
    if (!folder.isEmpty()) {
        QDir(projectDir.filePath(folder)).removeRecursively();
    }
}

}  // namespace kalahari::core
