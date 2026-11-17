// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_import_texts.cpp
/// @brief The texts of an imported file added to a book, and taken out again

#include <kalahari/gui/book_import_texts.h>

#include <kalahari/core/logger.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/editor/image_store.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>

namespace kalahari::gui {

namespace {

constexpr std::array<core::BookPlace, 3> BOOK_PLACES{core::BookPlace::Front, core::BookPlace::Main,
                                                     core::BookPlace::Back};

bool sameKind(const core::KindRef& a, const core::KindRef& b) {
    return a.kind == b.kind && a.package == b.package;
}

/// Whether a kind is offered in a list of the book (or inside its group) now
bool offeredAt(const core::ProjectManager& book, const core::KindRef& kind, core::BookPlace place,
               const QString& groupId) {
    const QList<core::KindRef> kinds = book.kindsFor(place, groupId);
    return std::any_of(kinds.begin(), kinds.end(), [&kind](const core::KindRef& each) { return sameKind(each, kind); });
}

/// The place a text of a kind goes to when the writer's place is not for it: its kind's place in
/// the part of the book the place is in, else in the first part that offers it
std::optional<core::ElementPlace> placeOfKind(const core::ProjectManager& book, const core::KindRef& kind,
                                              core::BookPlace preferred) {
    if (offeredAt(book, kind, preferred, QString())) {
        return book.newPlaceOf(kind, preferred);
    }
    for (const core::BookPlace place : BOOK_PLACES) {
        if (offeredAt(book, kind, place, QString())) {
            return book.newPlaceOf(kind, place);
        }
    }
    return std::nullopt;
}

/// Whether a KML text has no characters in it, only its elements
bool withoutText(const QString& kml) {
    static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
    return QString(kml).remove(tags).trimmed().isEmpty();
}

/// The files of the book's folder of images, by name
QSet<QString> imageFiles(const QString& folder) {
    QSet<QString> names;
    const QDir images(folder + QLatin1Char('/') + QLatin1String(editor::IMAGES_FOLDER));
    for (const QString& name : images.entryList(QDir::Files)) {
        names.insert(name);
    }
    return names;
}

/// Add the texts to a list of the book from a place on
/// @param top Texts of the import itself, not of a part it adds: each one is in @p added; of a
///        part only those that go out of it (a prologue) are, as the part takes the others with it
bool addTexts(core::ProjectManager& book, const std::vector<ImportedText>& texts, core::ElementPlace at, bool top,
              QStringList* added) {
    for (const ImportedText& text : texts) {
        if (!text.kind) {
            return false;
        }
        const bool group = text.kind.kind->form == core::ElementForm::Group;
        // A kind with a place of its own, or one the writer's place cannot have, goes to its place
        core::ElementPlace place = at;
        const core::KindPosition position = group ? core::KindPosition::Any : text.kind.kind->position;
        if (!offeredAt(book, text.kind, at.place, at.groupId) ||
            (position != core::KindPosition::Any && at.groupId.isEmpty())) {
            const std::optional<core::ElementPlace> other = placeOfKind(book, text.kind, at.place);
            if (!other) {
                return false;
            }
            place = *other;
        }
        const std::optional<QString> content = group ? std::nullopt : std::optional<QString>(text.kml);
        const QString id = book.addElement(text.kind, text.title, place.place, place.groupId, place.index, {}, content);
        if (id.isEmpty()) {
            core::Logger::getInstance().warn("Import: '{}' could not be added to the book", text.title.toStdString());
            return false;
        }
        const bool sameList = place.place == at.place && place.groupId == at.groupId;
        if (added && (top || !sameList)) {
            added->append(id);
        }
        // The next text goes after it; a text of the start (a prologue) put before the place moves
        // the place on, one of the end (an epilogue) put at it does not
        if (sameList && (place.index < at.index || (place.index == at.index && position != core::KindPosition::End))) {
            ++at.index;
        }
        if (group && !addTexts(book, text.texts, {place.place, id, 0}, false, added)) {
            return false;
        }
    }
    return true;
}

/// Add the texts, those of the parts too, as files of the book outside its text
bool addFiles(core::ProjectManager& book, const std::vector<ImportedText>& texts, QStringList* added) {
    for (const ImportedText& text : texts) {
        if (text.kind && text.kind.kind->form == core::ElementForm::Group) {
            if (!addFiles(book, text.texts, added)) {
                return false;
            }
            continue;
        }
        const QString id = book.addBookFile(text.title);
        if (id.isEmpty()) {
            return false;
        }
        if (added) {
            added->append(id);
        }
        if (!text.kml.isEmpty()) {
            book.setChapterContent(id, text.kml);
            if (!book.saveChapterContent(id)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

std::vector<editor::KindTitle> importKindTitles(const core::ProjectManager& book) {
    const core::BookProject* project = book.project();
    if (!project) {
        return {};
    }
    QList<core::KindRef> kinds;
    for (const core::BookPlace place : BOOK_PLACES) {
        kinds += project->kindsIn(book.bookTypes(), place);
    }
    return importKindTitles(kinds, book.mainTextKind());
}

std::vector<editor::KindTitle> importKindTitles(const QList<core::KindRef>& kinds, const core::KindRef& main) {
    std::vector<editor::KindTitle> titles;
    QSet<QString> seen;
    for (const core::KindRef& kind : kinds) {
        if (!kind || kind.kind->form != core::ElementForm::Text || kind.kind->isCard() || sameKind(kind, main) ||
            seen.contains(kind.reference())) {
            continue;
        }
        seen.insert(kind.reference());
        editor::KindTitle title;
        title.kind = kind.reference();
        for (const QString& name : kind.kind->name.values) {
            if (!name.trimmed().isEmpty() && !title.titles.contains(name, Qt::CaseInsensitive)) {
                title.titles << name;
            }
        }
        if (!title.titles.isEmpty()) {
            titles.push_back(std::move(title));
        }
    }
    return titles;
}

std::vector<QString> saveImportedImages(const core::ProjectManager& book, const editor::ImportDocument& document,
                                        QStringList* written) {
    std::vector<QString> sources(document.images.size());
    editor::ImageStore store;
    store.setFolder(book.imagesBaseFolder());
    if (!store.hasFolder()) {
        return sources;
    }
    const QSet<QString> before = imageFiles(store.folder());
    for (size_t i = 0; i < document.images.size(); ++i) {
        const editor::ImportImage& image = document.images[i];
        if (image.data.isEmpty()) {
            continue;
        }
        QString name = QFileInfo(image.fileName).fileName();
        if (name.isEmpty()) {
            name = QStringLiteral("image%1.png").arg(i + 1);
        }
        sources[i] = store.addData(image.data, name);
        const QString file = QFileInfo(sources[i]).fileName();
        if (written && !sources[i].isEmpty() && !before.contains(file) && !written->contains(sources[i])) {
            written->append(sources[i]);
        }
    }
    return sources;
}

bool addImportedTexts(core::ProjectManager& book, const std::vector<ImportedText>& texts, const ImportPlace& place,
                      ImportedElements* added) {
    if (!book.project()) {
        return false;
    }
    QStringList ids;
    const bool done = place.otherFiles ? addFiles(book, texts, &ids) : addTexts(book, texts, place.at, true, &ids);
    if (added) {
        added->elements += ids;
    }
    return done;
}

void clearNewBookStart(core::ProjectManager& book, const std::vector<ImportedText>& texts) {
    const core::ProjectBook* first = book.book();
    if (!first) {
        return;
    }
    QSet<const core::ElementKind*> imported;
    const std::function<void(const std::vector<ImportedText>&)> collect = [&](const std::vector<ImportedText>& list) {
        for (const ImportedText& text : list) {
            if (text.kind) {
                imported.insert(text.kind.kind);
            }
            collect(text.texts);
        }
    };
    collect(texts);
    const core::KindRef main = book.mainTextKind();
    QStringList start;
    for (const QList<core::ProjectElement>* elements : {&first->frontElements, &first->mainElements, &first->backElements}) {
        for (const core::ProjectElement& element : *elements) {
            const core::KindRef kind = book.kindOf(element);
            if (!kind || kind.kind->form != core::ElementForm::Text) {
                continue;
            }
            if (imported.contains(kind.kind) || (sameKind(kind, main) && withoutText(book.chapterText(element.id)))) {
                start << element.id;
            }
        }
    }
    for (const QString& id : std::as_const(start)) {
        book.discardChapterContent(id);
        if (const std::optional<core::ProjectElement> element = book.removeElement(id)) {
            for (const QString& file : book.filesOf(*element)) {
                QFile::remove(file);
            }
        }
    }
}

bool removeImported(core::ProjectManager& book, const ImportedElements& imported) {
    bool removedAll = true;
    for (auto it = imported.elements.crbegin(); it != imported.elements.crend(); ++it) {
        book.discardChapterContent(*it);
        const std::optional<core::ProjectElement> element = book.removeElement(*it);
        if (!element) {
            removedAll = false;
            continue;
        }
        for (const QString& file : book.filesOf(*element)) {
            removedAll = QFile::remove(file) && removedAll;
        }
    }
    editor::ImageStore store;
    store.setFolder(book.imagesBaseFolder());
    for (const QString& source : imported.images) {
        if (const QString file = store.filePath(source); !file.isEmpty() && QFileInfo::exists(file)) {
            removedAll = QFile::remove(file) && removedAll;
        }
    }
    if (!imported.styles.isEmpty()) {
        QList<core::BookParagraphStyle> styles = book.paragraphStyles();
        styles.removeIf([&imported](const core::BookParagraphStyle& style) { return imported.styles.contains(style.id); });
        removedAll = book.setParagraphStyles(styles) && removedAll;
    }
    return removedAll;
}

}  // namespace kalahari::gui
