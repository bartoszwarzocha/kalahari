// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_chapters.cpp
/// @brief The chapters a card is in and the materials linked with it on the full card, Find in
/// Book of the card, and the files and pictures dropped on a card

#include "document_window.h"
#include "test_support/window_parts.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/link_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_source.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/panels/card_panel.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/widgets/card_chapters_box.h"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDropEvent>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>

#include <cstdio>
#include <functional>
#include <memory>
#include <optional>

namespace kalahari {

namespace {

using gui::dialogs::LinkDialog;

/// A new card @p title of kind @p kindId of the Base in the Workshop
QString addCard(core::ProjectManager& pm, const char* kindId, const QString& title) {
    const core::KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QLatin1String(kindId));
    REQUIRE(kind);
    const QString id = pm.addElement(kind, title, core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// A new chapter @p title at the end of the main part
QString addChapter(core::ProjectManager& pm, const QString& title) {
    const QString id =
        pm.addElement(pm.chapterKindFor(core::BookPlace::Main), title, core::BookPlace::Main);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// The text of chapter @p chapterId: paragraphs @p paragraphs, saved
void writeChapter(core::ProjectManager& pm, const QString& chapterId,
                  const QStringList& paragraphs) {
    pm.setChapterContent(chapterId, QStringLiteral("<kml><p>") +
                                        paragraphs.join(QStringLiteral("</p><p>")) +
                                        QStringLiteral("</p></kml>"));
    REQUIRE(pm.saveChapterContent(chapterId));
}

/// A picture file @p name of 40 x 30 pixels in folder @p dir
QString writePicture(const QString& dir, const QString& name) {
    QImage image(QSize(40, 30), QImage::Format_RGB32);
    image.fill(Qt::red);
    const QString path = QDir(dir).filePath(name);
    REQUIRE(image.save(path, "PNG"));
    return path;
}

/// A picture material of file @p path titled @p title, linked with @p links
QString addPicture(core::ProjectManager& pm, const QString& path, const QString& title,
                   const QStringList& links) {
    const core::KindRef kind = pm.bookTypes().findKind(
        QStringLiteral("kalahari.base"), QLatin1String(core::MaterialKinds::PICTURE));
    REQUIRE(kind);
    core::NewMaterial material;
    material.title = title;
    material.filePath = path;
    material.links = links;
    const QString id = pm.addMaterial(material, kind);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// Key @p key sent to @p widget, as it gets it when it has the keys: a shortcut override
/// first
void press(QWidget* widget, Qt::Key key) {
    QKeyEvent override(QEvent::ShortcutOverride, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &override);
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

/// The rows of @p list, each "chapter | role | note"
QStringList rowsOf(const QTreeWidget* list) {
    QStringList rows;
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        const QTreeWidgetItem* item = list->topLevelItem(i);
        rows << QStringLiteral("%1 | %2 | %3").arg(item->text(0), item->text(1), item->text(2));
    }
    return rows;
}

/// The texts of the items of @p combo
QStringList itemsOf(const QComboBox* combo) {
    QStringList texts;
    for (int i = 0; i < combo->count(); ++i) {
        texts << combo->itemText(i);
    }
    return texts;
}

/// The texts of the actions of @p menu, "(off)" after those that do nothing
QStringList actionsOf(const QMenu* menu) {
    QStringList texts;
    for (const QAction* action : menu->actions()) {
        texts << (action->isEnabled() || action->isSeparator()
                      ? action->text()
                      : action->text() + QStringLiteral(" (off)"));
    }
    return texts;
}

/// The action of @p menu with text @p text
QAction* actionOf(const QMenu* menu, const QString& text) {
    for (QAction* action : menu->actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

/// Run @p command; in each Link window it opens, @p choose sets the links, then Link is
/// clicked
/// @return The number of Link windows the command opened
int linking(const std::function<void()>& command, const std::function<void(LinkDialog&)>& choose) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &choose]() {
        auto* dialog = qobject_cast<LinkDialog*>(test::shownDialog());
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        ++shown;
        choose(*dialog);
        dialog->acceptButton()->click();
    });
    timer.start();
    command();
    return shown;
}

/// The setting of whether a link names a paragraph, as a test sets it until it ends
class ParagraphsShown {
public:
    explicit ParagraphsShown(bool shown)
        : m_saved(core::SettingsManager::getInstance().get<bool>(
              core::LinkRoles::PARAGRAPHS_SETTING, true)) {
        core::SettingsManager::getInstance().set<bool>(core::LinkRoles::PARAGRAPHS_SETTING,
                                                       shown);
    }
    ~ParagraphsShown() {
        try {
            core::SettingsManager::getInstance().set<bool>(core::LinkRoles::PARAGRAPHS_SETTING,
                                                           m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            static_cast<void>(std::fputs("ParagraphsShown: the setting stays\n", stderr));
        }
    }
    ParagraphsShown(const ParagraphsShown&) = delete;
    ParagraphsShown& operator=(const ParagraphsShown&) = delete;
    ParagraphsShown(ParagraphsShown&&) = delete;
    ParagraphsShown& operator=(ParagraphsShown&&) = delete;

private:
    bool m_saved;
};

/// What a drag of the files @p paths carries
std::unique_ptr<QMimeData> filesData(const QStringList& paths) {
    auto data = std::make_unique<QMimeData>();
    QList<QUrl> urls;
    for (const QString& path : paths) {
        urls << QUrl::fromLocalFile(path);
    }
    data->setUrls(urls);
    return data;
}

}  // namespace

TEST_CASE("Link: the chapters of a card, with the roles of its kind, beside its relations",
          "[gui][cards][links]") {
    gui::registerAllIcons();
    const ParagraphsShown paragraphs(true);
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const QString storm = novel.chapterId;
    const QString calm = addChapter(pm, QStringLiteral("The Calm"));
    writeChapter(pm, storm,
                 {QStringLiteral("The storm came at night."),
                  QStringLiteral("Anna watched it from the window.")});
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    const QString jan = addCard(pm, "character", QStringLiteral("Jan"));
    const QString krakow = addCard(pm, "location", QStringLiteral("Kraków"));
    REQUIRE(pm.addRelation(anna, jan, QStringLiteral("sibling"), QString()));

    // The book with its sections and their chapters; only the chapters can be checked, and
    // the Workshop is not there
    LinkDialog dialog(kalahari::test::book(), LinkDialog::Mode::Chapters, anna);
    QTreeWidget* tree = dialog.tree();
    REQUIRE(tree->topLevelItemCount() == 1);
    const QTreeWidgetItem* book = tree->topLevelItem(0);
    CHECK_FALSE(book->flags().testFlag(Qt::ItemIsUserCheckable));
    REQUIRE(book->childCount() == 3);
    for (int i = 0; i < book->childCount(); ++i) {
        CHECK_FALSE(book->child(i)->flags().testFlag(Qt::ItemIsUserCheckable));
    }
    QTreeWidgetItem* stormItem = dialog.itemOf(storm);
    REQUIRE(stormItem != nullptr);
    CHECK(stormItem->flags().testFlag(Qt::ItemIsUserCheckable));
    CHECK(stormItem->checkState(0) == Qt::Unchecked);
    CHECK(dialog.itemOf(calm) != nullptr);
    CHECK(dialog.itemOf(jan) == nullptr);
    CHECK(dialog.itemOf(krakow) == nullptr);
    CHECK(dialog.acceptButton()->text() == QStringLiteral("&Link"));

    // The roles of a character; a new link appears in the chapter, with the paragraphs of its
    // text to choose
    CHECK(itemsOf(dialog.roleCombo()) ==
          QStringList{QStringLiteral("No role"), QStringLiteral("point of view"),
                      QStringLiteral("appears"), QStringLiteral("mentioned")});
    dialog.setCurrentLink(storm);
    stormItem->setCheckState(0, Qt::Checked);
    CHECK(dialog.roleCombo()->currentText() == QStringLiteral("appears"));
    CHECK(dialog.paragraphCombo()->count() == 3);
    CHECK(dialog.linkOf(storm).to == storm);
    CHECK(dialog.linkOf(storm).role == QStringLiteral("appears"));

    // A location is where a chapter takes place
    LinkDialog place(kalahari::test::book(), LinkDialog::Mode::Chapters, krakow);
    CHECK(itemsOf(place.roleCombo()) ==
          QStringList{QStringLiteral("No role"), QStringLiteral("setting"),
                      QStringLiteral("mentioned")});
    place.setCurrentLink(calm);
    place.itemOf(calm)->setCheckState(0, Qt::Checked);
    CHECK(place.roleCombo()->currentText() == QStringLiteral("setting"));

    // What the window chose, with the relations of the card as they were
    std::optional<QList<core::ElementLink>> chosen;
    CHECK(linking(
              [&]() {
                  chosen = gui::dialogs::chooseChapters(kalahari::test::book(), anna, QString(),
                                                        nullptr);
              },
              [&storm](LinkDialog& window) {
                  window.setCurrentLink(storm);
                  window.itemOf(storm)->setCheckState(0, Qt::Checked);
                  window.roleCombo()->setCurrentIndex(
                      window.roleCombo()->findData(QStringLiteral("pov")));
                  window.paragraphCombo()->setCurrentIndex(2);
                  window.noteEdit()->setText(QStringLiteral("at night"));
                  emit window.noteEdit()->textEdited(window.noteEdit()->text());
              }) == 1);
    REQUIRE(chosen.has_value());
    REQUIRE(chosen->size() == 2);
    CHECK(chosen->at(0).to == jan);
    CHECK(chosen->at(0).role == QStringLiteral("sibling"));
    CHECK(chosen->at(1).to == storm);
    CHECK(chosen->at(1).role == QStringLiteral("pov"));
    CHECK(chosen->at(1).paragraph == 2);
    CHECK(chosen->at(1).excerpt == QStringLiteral("Anna watched it from the window."));
    CHECK(chosen->at(1).note == QStringLiteral("at night"));
}

TEST_CASE("In Chapters: the chapters of the full card in the order of the book, changed with "
          "the keys, the buttons and the menu",
          "[gui][cards][links]") {
    gui::registerAllIcons();
    const ParagraphsShown paragraphs(true);
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const QString storm = novel.chapterId;
    const QString calm = addChapter(pm, QStringLiteral("The Calm"));
    writeChapter(pm, storm,
                 {QStringLiteral("The storm came at night."),
                  QStringLiteral("Anna watched it from the window.")});
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    const QString jan = addCard(pm, "character", QStringLiteral("Jan"));
    REQUIRE(pm.addRelation(anna, jan, QStringLiteral("sibling"), QString()));
    // The later chapter linked first: the list has the order of the book
    QList<core::ElementLink> links = pm.findElement(anna)->links;
    links << pm.newLink(anna, calm);
    core::ElementLink night = pm.newLink(anna, storm);
    night.role = QStringLiteral("pov");
    night.paragraph = 2;
    night.excerpt = QStringLiteral("Anna watched it from the window.");
    night.note = QStringLiteral("at night");
    links << night;
    REQUIRE(pm.setLinks(anna, links));

    gui::CardChaptersBox box(kalahari::test::book());
    QStringList opened;
    QObject::connect(&box, &gui::CardChaptersBox::requestOpenChapter,
                     [&opened](const QString& id) { opened << id; });
    box.showCard(anna);
    CHECK(box.title() == QStringLiteral("In Chapters"));
    QTreeWidget* list = box.list();
    CHECK(rowsOf(list) == QStringList{QStringLiteral("The Storm | point of view · paragraph 2 | "
                                                     "at night"),
                                      QStringLiteral("The Calm | appears | ")});
    CHECK(list->topLevelItem(0)->toolTip(0) == QStringLiteral("at night"));
    CHECK_FALSE(list->topLevelItem(0)->icon(0).isNull());
    CHECK_FALSE(box.emptyLabel()->isVisibleTo(&box));
    CHECK_FALSE(box.removeButton()->isEnabled());

    // Without the paragraphs of the links, the role alone
    {
        const ParagraphsShown hidden(false);
        QCoreApplication::processEvents();
        CHECK(list->topLevelItem(0)->text(1) == QStringLiteral("point of view"));
    }
    QCoreApplication::processEvents();
    CHECK(list->topLevelItem(0)->text(1) == QStringLiteral("point of view · paragraph 2"));

    // A new title of a chapter is shown at once
    REQUIRE(pm.renameElement(calm, QStringLiteral("The Quiet")));
    CHECK(list->topLevelItem(1)->text(0) == QStringLiteral("The Quiet"));

    // Enter and a double-click open the chapter
    list->setCurrentItem(list->topLevelItem(0));
    CHECK(box.removeButton()->isEnabled());
    press(list, Qt::Key_Return);
    emit list->itemDoubleClicked(list->topLevelItem(0), 0);
    CHECK(opened == QStringList{storm, storm});

    // The menu of a row has it all
    emit list->customContextMenuRequested(QPoint(4, 4));
    CHECK(actionsOf(box.rowMenu()) ==
          QStringList{QStringLiteral("Open the Chapter"), QStringLiteral("Change the Link... (F2)"),
                      QStringLiteral("Remove from Chapter (Delete)"), QString(),
                      QStringLiteral("Link with Chapters... (Insert)")});
    box.rowMenu()->close();

    // F2: the window Link at the current chapter, with its link
    CHECK(linking([list]() { press(list, Qt::Key_F2); },
                  [&storm](LinkDialog& dialog) {
                      REQUIRE(dialog.tree()->currentItem() != nullptr);
                      CHECK(dialog.tree()->currentItem() == dialog.itemOf(storm));
                      CHECK(dialog.roleCombo()->currentText() == QStringLiteral("point of view"));
                      CHECK(dialog.noteEdit()->text() == QStringLiteral("at night"));
                      dialog.roleCombo()->setCurrentIndex(
                          dialog.roleCombo()->findData(QStringLiteral("mentioned")));
                  }) == 1);
    CHECK(rowsOf(list).first() ==
          QStringLiteral("The Storm | mentioned · paragraph 2 | at night"));

    // Delete: the chapter goes, the relation stays
    list->setCurrentItem(list->topLevelItem(0));
    press(list, Qt::Key_Delete);
    CHECK(rowsOf(list) == QStringList{QStringLiteral("The Quiet | appears | ")});
    CHECK(pm.findElement(anna)->linkTo(storm) == nullptr);
    CHECK(pm.findElement(anna)->linkTo(jan) != nullptr);
    press(list, Qt::Key_Delete);
    CHECK(list->topLevelItemCount() == 0);
    CHECK_FALSE(list->isVisibleTo(&box));
    CHECK(box.emptyLabel()->isVisibleTo(&box));
    CHECK(box.emptyLabel()->text() ==
          QStringLiteral("The card is in no chapter yet. \"+\" or Insert links it."));

    // Insert and "+": the window Link with the chapters
    CHECK(linking([list]() { press(list, Qt::Key_Insert); },
                  [&calm](LinkDialog& dialog) {
                      dialog.itemOf(calm)->setCheckState(0, Qt::Checked);
                  }) == 1);
    CHECK(rowsOf(list) == QStringList{QStringLiteral("The Quiet | appears | ")});
    CHECK(linking([&box]() { box.addButton()->click(); },
                  [&storm](LinkDialog& dialog) {
                      dialog.itemOf(storm)->setCheckState(0, Qt::Checked);
                  }) == 1);
    CHECK(rowsOf(list) == QStringList{QStringLiteral("The Storm | appears | "),
                                      QStringLiteral("The Quiet | appears | ")});
    CHECK(pm.findElement(anna)->linkTo(jan) != nullptr);
}

TEST_CASE("Card: Find in Book, the materials of the card with Set as Portrait, and the files "
          "and pictures dropped on it",
          "[gui][cards][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna Wolska"));
    const QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString map = addPicture(pm, writePicture(files.path(), QStringLiteral("map.png")),
                                   QStringLiteral("Map"), {anna});

    gui::CardPage page(kalahari::test::book(), true);
    QStringList found;
    QStringList openedMaterials;
    QStringList added;
    QList<gui::IncomingMaterials> dropped;
    QStringList droppedOn;
    QObject::connect(&page, &gui::CardPage::requestFindInBook,
                     [&found](const QString& cardId) { found << cardId; });
    QObject::connect(&page, &gui::CardPage::requestOpenMaterial,
                     [&openedMaterials](const QString& id) { openedMaterials << id; });
    QObject::connect(&page, &gui::CardPage::materialAdded,
                     [&added](const QString& id) { added << id; });
    QObject::connect(&page, &gui::CardPage::requestAddMaterials,
                     [&dropped, &droppedOn](const gui::IncomingMaterials& incoming,
                                            const QString& link) {
                         dropped << incoming;
                         droppedOn << link;
                     });
    page.resize(1100, 900);
    page.showCard(anna);
    page.show();
    QApplication::processEvents();

    // Find in Book: the name of the card, all its forms and its other names
    REQUIRE(page.findButton() != nullptr);
    CHECK(page.findButton()->text() == QStringLiteral("Find in Book"));
    page.findButton()->click();
    CHECK(found == QStringList{anna});

    // The materials of the card, with Set as Portrait for a picture
    gui::LinkedMaterialsBox* materials = page.materialsBox();
    REQUIRE(materials != nullptr);
    REQUIRE(page.chaptersBox() != nullptr);
    QListWidget* list = materials->list();
    REQUIRE(list->count() == 1);
    CHECK(list->item(0)->text().startsWith(QStringLiteral("Map")));
    list->setCurrentRow(0);
    press(list, Qt::Key_Return);
    CHECK(openedMaterials == QStringList{map});
    emit list->customContextMenuRequested(QPoint(4, 4));
    CHECK(actionsOf(materials->rowMenu()) ==
          QStringList{QStringLiteral("Open the Material"), QStringLiteral("Remove Link (Delete)"),
                      QString(), QStringLiteral("Link Material... (Insert)"), QString(),
                      QStringLiteral("Set as &Portrait")});
    QAction* portrait = actionOf(materials->rowMenu(), QStringLiteral("Set as &Portrait"));
    REQUIRE(portrait != nullptr);
    portrait->trigger();
    materials->rowMenu()->close();
    CHECK(pm.findElement(anna)->card.portraitImage() == map);
    emit list->customContextMenuRequested(QPoint(4, 4));
    CHECK(actionsOf(materials->rowMenu()).last() == QStringLiteral("Set as &Portrait (off)"));
    materials->rowMenu()->close();

    // A picture dropped on the portrait: a material linked with the card, and its portrait
    const QString face = writePicture(files.path(), QStringLiteral("face.png"));
    gui::IncomingMaterials onPortrait;
    onPortrait.files = {face};
    const QPoint portraitCenter =
        page.portraitButton()->mapTo(&page, page.portraitButton()->rect().center());
    CHECK(page.dropMaterials(onPortrait, portraitCenter));
    REQUIRE(added.size() == 1);
    const core::ProjectElement* faceMaterial = pm.findElement(added.first());
    REQUIRE(faceMaterial != nullptr);
    CHECK(faceMaterial->title == QStringLiteral("face"));
    CHECK(faceMaterial->linkTo(anna) != nullptr);
    CHECK(pm.findElement(anna)->card.portraitImage() == added.first());
    CHECK(list->count() == 2);
    CHECK(dropped.isEmpty());

    // A picture without a file too
    gui::IncomingMaterials picture;
    picture.picture = QImage(QSize(20, 20), QImage::Format_RGB32);
    picture.picture.fill(Qt::blue);
    CHECK(page.dropMaterials(picture, portraitCenter));
    REQUIRE(added.size() == 2);
    CHECK(pm.findElement(anna)->card.portraitImage() == added.last());

    // Elsewhere, or more than a picture: new materials linked with the card
    const QPoint elsewhere = page.nameEdit()->mapTo(&page, QPoint(2, 2));
    CHECK(page.dropMaterials(onPortrait, elsewhere));
    gui::IncomingMaterials two;
    two.files = {face, writePicture(files.path(), QStringLiteral("hand.png"))};
    CHECK(page.dropMaterials(two, portraitCenter));
    REQUIRE(dropped.size() == 2);
    CHECK(dropped.at(0).files == QStringList{face});
    CHECK(dropped.at(1).files.size() == 2);
    CHECK(droppedOn == QStringList{anna, anna});
    CHECK(added.size() == 2);
    // Nothing to add
    CHECK_FALSE(page.dropMaterials(gui::IncomingMaterials(), elsewhere));

    // A drag of files is welcome; the drop gives them
    const std::unique_ptr<QMimeData> data = filesData({face});
    QDragEnterEvent enter(elsewhere, Qt::CopyAction, data.get(), Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&page, &enter);
    CHECK(enter.isAccepted());
    QDropEvent drop(QPointF(elsewhere), Qt::CopyAction, data.get(), Qt::LeftButton,
                    Qt::NoModifier);
    QCoreApplication::sendEvent(&page, &drop);
    CHECK(drop.isAccepted());
    CHECK(dropped.size() == 3);
}

TEST_CASE("Card: on its tab, the chapters open, the dropped files are its materials and Find in "
          "Book searches for its name",
          "[gui][cards][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    const QString anna = addCard(pm, "character", QStringLiteral("Anna"));
    const QList<core::ElementLink> links{pm.newLink(anna, novel.chapterId)};
    REQUIRE(pm.setLinks(anna, links));
    const QTemporaryDir files;
    REQUIRE(files.isValid());
    test::Window window;
    QStringList found;
    QObject::connect(&window.navigator, &gui::NavigatorCoordinator::findInBookRequested,
                     [&found](const QString& cardId) { found << cardId; });

    window.navigator.onElementSelected(anna, QStringLiteral("Anna"));
    auto* panel = qobject_cast<gui::CardPanel*>(window.tabs->currentWidget());
    REQUIRE(panel != nullptr);
    gui::CardPage* page = panel->page();

    // Enter on a chapter of the card opens it
    QTreeWidget* chapters = page->chaptersBox()->list();
    REQUIRE(chapters->topLevelItemCount() == 1);
    chapters->setCurrentItem(chapters->topLevelItem(0));
    press(chapters, Qt::Key_Return);
    CHECK(window.tabs->count() == 2);
    CHECK(qobject_cast<gui::CardPanel*>(window.tabs->currentWidget()) == nullptr);

    // A file dropped on the card is a new material linked with it
    gui::IncomingMaterials incoming;
    incoming.files = {writePicture(files.path(), QStringLiteral("hat.png"))};
    CHECK(page->dropMaterials(incoming, page->nameEdit()->mapTo(page, QPoint(2, 2))));
    const QList<const core::ProjectElement*> linked = pm.materialsLinkedTo(anna);
    REQUIRE(linked.size() == 1);
    CHECK(linked.first()->title == QStringLiteral("hat"));
    CHECK(page->materialsBox()->list()->count() == 1);

    page->findButton()->click();
    CHECK(found == QStringList{anna});
}

}  // namespace kalahari
