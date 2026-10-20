/// @file test_material_card.cpp
/// @brief The card of a material in the Properties panel, its preview tab and Link

#include "document_window.h"

#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/link_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/panels/image_preview_panel.h"
#include "kalahari/gui/panels/material_page.h"

#include <catch2/catch_test_macros.hpp>

#include <QAbstractItemDelegate>
#include <QBuffer>
#include <QCompleter>
#include <QDir>
#include <QDropEvent>
#include <QFileInfo>
#include <QFocusEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QListWidget>
#include <QLocale>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QStringListModel>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <cstdio>
#include <functional>
#include <memory>

using namespace kalahari;

namespace {

/// A picture of @p size in folder @p dir: red in its top left quarter, blue elsewhere
QString writePicture(const QString& dir, const QString& name, const QSize& size) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::blue);
    for (int y = 0; y < size.height() / 2; ++y) {
        for (int x = 0; x < size.width() / 2; ++x) {
            image.setPixelColor(x, y, Qt::red);
        }
    }
    const QString path = QDir(dir).filePath(name);
    REQUIRE(image.save(path, "PNG"));
    return path;
}

/// A file with @p bytes in folder @p dir
QString writeFile(const QString& dir, const QString& name, const QByteArray& bytes) {
    const QString path = QDir(dir).filePath(name);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(bytes) == bytes.size());
    return path;
}

/// A picture in folder @p dir cut off after its first part: its image plugin knows its format
/// and size, and cannot read it
QString writeCutPicture(const QString& dir, const QString& name) {
    QImage image(64, 64, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixel(x, y, qRgb((x * 37 + y * 11) % 256, (x * y) % 256, (x + y * 53) % 256));
        }
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    REQUIRE(image.save(&buffer, "PNG"));
    return writeFile(dir, name, bytes.left(bytes.size() / 2));
}

/// Whether the pixel at @p x, @p y of @p image is red, and not blue
bool isRed(const QImage& image, int x, int y) {
    const QRgb pixel = image.pixel(x, y);
    return qRed(pixel) > 200 && qBlue(pixel) < 50;
}

/// A copy of file @p path in the Workshop of the open book, as material @p title
QString addMaterial(core::ProjectManager& pm, const QString& path, const QString& title) {
    const QString id = pm.addMaterial(
        path, pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("material")),
        title);
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// The .klh file of the open book, as it is on the disk
QByteArray manifestFile(const core::ProjectManager& pm) {
    QFile file(pm.getManifestPath());
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

/// Key @p key with @p modifiers sent to @p widget, as it gets it when it has the keys
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QCoreApplication::sendEvent(widget, &event);
}

/// The editor of the cell at @p row, @p column of @p table, open now
QLineEdit* cellEditor(const QTableWidget* table, int row, int column) {
    auto* editor = qobject_cast<QLineEdit*>(table->indexWidget(table->model()->index(row, column)));
    REQUIRE(editor != nullptr);
    return editor;
}

/// Type @p text in the open editor of a cell and leave it as @p hint says: Tab goes to the
/// next cell
void finishCell(QTableWidget* table, QLineEdit* editor, const QString& text,
                QAbstractItemDelegate::EndEditHint hint) {
    editor->setText(text);
    QAbstractItemDelegate* delegate = table->itemDelegate();
    emit delegate->commitData(editor);
    emit delegate->closeEditor(editor, hint);
}

/// Run @p command; in the Link window it opens, @p choose checks the boxes, then Link is
/// clicked
/// @return The number of Link windows the command opened
int linking(const std::function<void()>& command,
            const std::function<void(gui::dialogs::LinkDialog&)>& choose) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        auto* dialog = qobject_cast<gui::dialogs::LinkDialog*>(test::shownDialog());
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

/// The item of the Navigator with the element or the type @p idOrType
QTreeWidgetItem* navigatorItem(const QTreeWidget* tree, const QString& idOrType) {
    for (QTreeWidgetItemIterator it(const_cast<QTreeWidget*>(tree)); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == idOrType ||
            (*it)->data(0, Qt::UserRole + 1).toString() == idOrType) {
            return *it;
        }
    }
    return nullptr;
}

/// The data that dragging @p item of the Navigator carries
std::unique_ptr<QMimeData> dragged(const gui::NavigatorPanel& navigator, QTreeWidgetItem* item) {
    const QTreeWidget* tree = navigator.tree();
    std::unique_ptr<QMimeData> data(
        tree->model()->mimeData({tree->indexFromItem(item)}));
    REQUIRE(data != nullptr);
    return data;
}

/// The setting of how a material opens, as it was before the test
class OpenMaterialsSetting {
public:
    explicit OpenMaterialsSetting(const std::string& value)
        : m_saved(core::SettingsManager::getInstance().get<std::string>(
              gui::NavigatorPanel::OPEN_MATERIALS_SETTING, "card")) {
        core::SettingsManager::getInstance().set<std::string>(
            gui::NavigatorPanel::OPEN_MATERIALS_SETTING, value);
    }
    ~OpenMaterialsSetting() {
        try {
            core::SettingsManager::getInstance().set<std::string>(
                gui::NavigatorPanel::OPEN_MATERIALS_SETTING, m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            std::fputs("OpenMaterialsSetting: the setting of the test stays\n", stderr);
        }
    }
    OpenMaterialsSetting(const OpenMaterialsSetting&) = delete;
    OpenMaterialsSetting& operator=(const OpenMaterialsSetting&) = delete;

private:
    std::string m_saved;
};

} // namespace

TEST_CASE("Material file: its name, format, size and why the program does not show it",
          "[gui][materials]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString kilobytes = QLocale().formattedDataSize(QFileInfo(writePicture(
        dir.path(), QStringLiteral("map.png"), QSize(40, 20))).size(), 0,
        QLocale::DataSizeTraditionalFormat);

    // A picture the program shows
    const gui::MaterialFile map = gui::MaterialFile::of(QDir(dir.path()).filePath("map.png"));
    CHECK(map.exists);
    CHECK(map.picture);
    CHECK(map.readable);
    CHECK(map.size == QSize(40, 20));
    CHECK(map.summary() == QStringLiteral("map.png · PNG · 40 × 20 px · ") + kilobytes);
    CHECK(map.problem().isEmpty());

    // A file that is no picture: no sentence, the card shows the icon of its kind
    const QByteArray text("Notes about the map\n");
    const gui::MaterialFile notes =
        gui::MaterialFile::of(writeFile(dir.path(), QStringLiteral("notes.txt"), text));
    CHECK_FALSE(notes.picture);
    CHECK(notes.summary() == QStringLiteral("notes.txt · TXT · ") +
                                 QLocale().formattedDataSize(text.size(), 0,
                                                             QLocale::DataSizeTraditionalFormat));
    CHECK(notes.problem().isEmpty());

    // A picture of a format without an image plugin of the program
    const gui::MaterialFile sketch = gui::MaterialFile::of(
        writeFile(dir.path(), QStringLiteral("sketch.xcf"), QByteArray("gimp xcf v011")));
    CHECK(sketch.picture);
    CHECK_FALSE(sketch.readable);
    CHECK(sketch.problem() == QStringLiteral(
              "The program does not show XCF pictures. Open the file in the system program."));

    // A damaged picture of a format the program shows
    const gui::MaterialFile damaged = gui::MaterialFile::of(
        writeFile(dir.path(), QStringLiteral("damaged.png"), QByteArray("PNG")));
    CHECK(damaged.picture);
    CHECK_FALSE(damaged.readable);
    CHECK(damaged.problem().startsWith(QStringLiteral("The picture cannot be read (")));
    CHECK(damaged.problem().endsWith(QStringLiteral("). It may be damaged.")));

    // A picture whose beginning its image plugin knows, which it cannot read after all
    const QString cutPath = writeCutPicture(dir.path(), QStringLiteral("cut.png"));
    gui::MaterialFile cut = gui::MaterialFile::of(cutPath);
    CHECK(cut.readable);
    QString readError;
    CHECK(gui::readPicture(cutPath, 0, QSize(), &readError).isNull());
    CHECK_FALSE(readError.isEmpty());
    cut.unreadable(readError);
    CHECK(cut.picture);
    CHECK_FALSE(cut.readable);
    CHECK_FALSE(cut.size.isValid());
    CHECK(cut.summary().startsWith(QStringLiteral("cut.png · PNG · ")));
    CHECK_FALSE(cut.summary().contains(QStringLiteral(" px")));
    CHECK(cut.problem() ==
          QStringLiteral("The picture cannot be read (%1). It may be damaged.").arg(readError));

    // A file missing from the book's folder
    const gui::MaterialFile gone = gui::MaterialFile::of(QDir(dir.path()).filePath("gone.png"));
    CHECK_FALSE(gone.exists);
    CHECK(gone.summary() == QStringLiteral("gone.png · PNG"));
    CHECK(gone.problem() == QStringLiteral("The file is not in the book's folder."));
}

TEST_CASE("Material file: a picture is read turned and within its bounds", "[gui][materials]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = writePicture(dir.path(), QStringLiteral("map.png"), QSize(40, 20));

    const QImage picture = gui::readPicture(path);
    REQUIRE(picture.size() == QSize(40, 20));
    CHECK(isRed(picture, 5, 5));
    CHECK_FALSE(isRed(picture, 35, 15));

    // Turned clockwise: the top left quarter goes to the top right
    const QImage right = gui::readPicture(path, 90);
    REQUIRE(right.size() == QSize(20, 40));
    CHECK(isRed(right, 15, 5));
    CHECK_FALSE(isRed(right, 5, 5));
    const QImage upsideDown = gui::readPicture(path, 180);
    REQUIRE(upsideDown.size() == QSize(40, 20));
    CHECK(isRed(upsideDown, 35, 15));
    const QImage left = gui::readPicture(path, 270);
    REQUIRE(left.size() == QSize(20, 40));
    CHECK(isRed(left, 5, 35));

    // Smaller within the bounds, never larger
    CHECK(gui::readPicture(path, 0, QSize(10, 10)).size() == QSize(10, 5));
    CHECK(gui::readPicture(path, 90, QSize(10, 10)).size() == QSize(5, 10));
    CHECK(gui::readPicture(path, 0, QSize(100, 100)).size() == QSize(40, 20));

    CHECK(gui::readPicture(QDir(dir.path()).filePath("gone.png")).isNull());
}

TEST_CASE("Material card: the picture, the file, the title and the description, saved in the "
          "book", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString map =
        addMaterial(pm, writePicture(files.path(), QStringLiteral("map.png"), QSize(400, 200)),
                    QStringLiteral("Map"));

    gui::MaterialPage page;
    QStringList previews;
    QStringList systemOpens;
    QStringList titles;
    QObject::connect(&page, &gui::MaterialPage::requestPreview, [&](const QString& id) {
        previews << id;
    });
    QObject::connect(&page, &gui::MaterialPage::requestOpenWithSystem, [&](const QString& id) {
        systemOpens << id;
    });
    QObject::connect(&page, &gui::MaterialPage::titleChanged, [&](const QString& id) {
        titles << id;
    });
    page.resize(320, 900);
    page.showMaterial(map);
    page.show();
    QApplication::processEvents();

    // The picture across the card, at most so high; the file under it
    CHECK(page.elementId() == map);
    const QSize thumbnail = page.thumbnail()->iconSize();
    CHECK(thumbnail.width() <= page.thumbnail()->width());
    CHECK(thumbnail.width() > 200);
    CHECK(thumbnail.height() == thumbnail.width() / 2);
    CHECK(thumbnail.height() <= gui::MaterialPage::MAXIMUM_THUMBNAIL_HEIGHT);
    CHECK(page.thumbnailProblem()->isHidden());
    CHECK(page.fileLabel()->text().startsWith(QStringLiteral("map.png · PNG · 400 × 200 px · ")));
    CHECK(page.titleEdit()->text() == QStringLiteral("Map"));
    CHECK(page.pathLabel()->text() == pm.findElement(map)->file);

    // The picture and Preview open the preview; System Program the system program
    page.thumbnail()->click();
    page.previewButton()->click();
    page.systemButton()->click();
    CHECK(previews == QStringList{map, map});
    CHECK(systemOpens == QStringList{map});

    // The description is saved when the field loses the keys, in the .klh file
    page.descriptionEdit()->setPlainText(QStringLiteral("The valley from the north"));
    QFocusEvent focusOut(QEvent::FocusOut, Qt::TabFocusReason);
    QCoreApplication::sendEvent(page.descriptionEdit(), &focusOut);
    CHECK(pm.findElement(map)->description == QStringLiteral("The valley from the north"));
    CHECK(manifestFile(pm).contains("The valley from the north"));

    // Enter in the title renames the material; an empty title is not taken
    page.titleEdit()->setText(QStringLiteral("Old Map"));
    press(page.titleEdit(), Qt::Key_Return);
    CHECK(pm.findElement(map)->title == QStringLiteral("Old Map"));
    CHECK(titles == QStringList{map});
    page.titleEdit()->setText(QStringLiteral("  "));
    press(page.titleEdit(), Qt::Key_Return);
    CHECK(pm.findElement(map)->title == QStringLiteral("Old Map"));
    CHECK(page.titleEdit()->text() == QStringLiteral("Old Map"));

    // Opened again, the book has them
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
    REQUIRE(pm.findElement(map) != nullptr);
    CHECK(pm.findElement(map)->description == QStringLiteral("The valley from the north"));
    page.showMaterial(map);
    CHECK(page.descriptionEdit()->toPlainText() == QStringLiteral("The valley from the north"));
    CHECK(page.titleEdit()->text() == QStringLiteral("Old Map"));

    // A tall picture is as high as the card lets it be; a small one keeps its size
    const QString tower =
        addMaterial(pm, writePicture(files.path(), QStringLiteral("tower.png"), QSize(100, 1000)),
                    QStringLiteral("Tower"));
    page.showMaterial(tower);
    CHECK(page.thumbnail()->iconSize().height() == gui::MaterialPage::MAXIMUM_THUMBNAIL_HEIGHT);
    const QString stamp =
        addMaterial(pm, writePicture(files.path(), QStringLiteral("stamp.png"), QSize(40, 20)),
                    QStringLiteral("Stamp"));
    page.showMaterial(stamp);
    CHECK(page.thumbnail()->iconSize() == QSize(40, 20));

    // A picture the program cannot show says why, and has no preview
    const QString sketch = addMaterial(
        pm, writeFile(files.path(), QStringLiteral("sketch.xcf"), QByteArray("gimp xcf v011")),
        QStringLiteral("Sketch"));
    page.showMaterial(sketch);
    CHECK_FALSE(page.thumbnailProblem()->isHidden());
    CHECK(page.thumbnailProblem()->text() == QStringLiteral(
              "The program does not show XCF pictures. Open the file in the system program."));
    CHECK_FALSE(page.previewButton()->isEnabled());
    CHECK(page.systemButton()->isEnabled());
    previews.clear();
    page.thumbnail()->click();
    CHECK(previews.isEmpty());

    // So does a damaged picture of a format the program shows
    const QString cut = addMaterial(
        pm, writeCutPicture(files.path(), QStringLiteral("cut.png")), QStringLiteral("Cut"));
    page.showMaterial(cut);
    CHECK_FALSE(page.thumbnailProblem()->isHidden());
    CHECK(page.thumbnailProblem()->text().startsWith(
        QStringLiteral("The picture cannot be read (")));
    CHECK(page.thumbnailProblem()->text().endsWith(QStringLiteral("). It may be damaged.")));
    CHECK(page.previewButton()->toolTip() == page.thumbnailProblem()->text());
    CHECK_FALSE(page.previewButton()->isEnabled());
    CHECK(page.thumbnail()->toolTip().isEmpty());
    page.thumbnail()->click();
    CHECK(previews.isEmpty());

    // A picture again: the preview opens
    page.showMaterial(stamp);
    CHECK(page.thumbnailProblem()->isHidden());
    CHECK(page.previewButton()->isEnabled());
    page.thumbnail()->click();
    CHECK(previews == QStringList{stamp});
}

TEST_CASE("Material card: own fields are added with their names suggested, saved in their "
          "order and deleted", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString source = writePicture(files.path(), QStringLiteral("map.png"), QSize(40, 20));
    const QString other = addMaterial(pm, source, QStringLiteral("Other"));
    REQUIRE(pm.setFields(other, {core::ElementField{QStringLiteral("Region"),
                                                    QStringLiteral("North"), {}}}));
    const QString map = addMaterial(pm, source, QStringLiteral("Map"));

    gui::MaterialPage page;
    page.showMaterial(map);
    QTableWidget* table = page.fieldsTable();
    CHECK(table->isHidden());
    CHECK(page.addFieldButton()->isEnabled());
    CHECK_FALSE(page.removeFieldButton()->isEnabled());

    // "+" adds a field with the keys in its name, which suggests the names of the book;
    // Tab goes to its value
    page.addFieldButton()->click();
    REQUIRE(table->rowCount() == 1);
    CHECK_FALSE(table->isHidden());
    QLineEdit* name = cellEditor(table, 0, 0);
    REQUIRE(name->completer() != nullptr);
    const auto* names = qobject_cast<QStringListModel*>(name->completer()->model());
    REQUIRE(names != nullptr);
    CHECK(names->stringList().contains(QStringLiteral("Region")));
    finishCell(table, name, QStringLiteral("Scale"), QAbstractItemDelegate::EditNextItem);
    REQUIRE(pm.findElement(map)->fields.size() == 1);
    CHECK(pm.findElement(map)->fields.first().name == QStringLiteral("Scale"));
    finishCell(table, cellEditor(table, 0, 1), QStringLiteral("1 : 50 000"),
               QAbstractItemDelegate::SubmitModelCache);
    REQUIRE(pm.findElement(map)->fields.size() == 1);
    CHECK(pm.findElement(map)->fields.first().value == QStringLiteral("1 : 50 000"));
    CHECK(manifestFile(pm).contains("1 : 50 000"));

    // Insert adds one too; left without a name and a value, it goes
    press(table, Qt::Key_Insert);
    REQUIRE(table->rowCount() == 2);
    finishCell(table, cellEditor(table, 1, 0), QString(), QAbstractItemDelegate::NoHint);
    CHECK(table->rowCount() == 1);
    CHECK(pm.findElement(map)->fields.size() == 1);

    // A field with a name only goes at once with Delete
    press(table, Qt::Key_Insert);
    finishCell(table, cellEditor(table, 1, 0), QStringLiteral("Year"),
               QAbstractItemDelegate::SubmitModelCache);
    REQUIRE(pm.findElement(map)->fields.size() == 2);
    CHECK(pm.findElement(map)->fields.at(1).name == QStringLiteral("Year"));
    table->setCurrentCell(1, 0);
    press(table, Qt::Key_Delete);
    CHECK(pm.findElement(map)->fields.size() == 1);

    // A field with a value goes only when the writer agrees
    table->setCurrentCell(0, 0);
    CHECK(page.removeFieldButton()->isEnabled());
    QStringList said;
    using Answer = gui::dialogs::MessageDialog::Answer;
    CHECK(test::answering([&]() { press(table, Qt::Key_Delete); }, Answer::Cancel, said) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("Delete the field \"Scale\" with its value?")));
    CHECK(pm.findElement(map)->fields.size() == 1);
    CHECK(test::answering([&]() { page.removeFieldButton()->click(); }, Answer::Accept, said) ==
          1);
    CHECK(pm.findElement(map)->fields.isEmpty());
    CHECK(table->isHidden());

    // The fields are saved in the order of the table
    REQUIRE(pm.setFields(map, {core::ElementField{QStringLiteral("A"), QStringLiteral("1"), {}},
                               core::ElementField{QStringLiteral("B"), QStringLiteral("2"), {}}}));
    page.refresh();
    REQUIRE(table->rowCount() == 2);
    table->item(1, 1)->setText(QStringLiteral("3"));
    const QList<core::ElementField> fields = pm.findElement(map)->fields;
    REQUIRE(fields.size() == 2);
    CHECK(fields.at(0).name == QStringLiteral("A"));
    CHECK(fields.at(1).name == QStringLiteral("B"));
    CHECK(fields.at(1).value == QStringLiteral("3"));
}

TEST_CASE("Material card: links are added, dropped from the Navigator, opened and taken out",
          "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString map = addMaterial(
        pm, writePicture(files.path(), QStringLiteral("map.png"), QSize(40, 20)),
        QStringLiteral("Map"));
    test::Window window;
    const QString body = pm.project()->sectionLink(core::BookPlace::Main);

    gui::MaterialPage page;
    QStringList opened;
    QObject::connect(&page, &gui::MaterialPage::requestOpenLink, [&](const QString& link) {
        opened << link;
    });
    page.showMaterial(map);
    QTreeWidget* links = page.linksList();
    CHECK(links->isHidden());
    CHECK_FALSE(page.removeLinkButton()->isEnabled());

    page.addLinks({novel.chapterId});
    CHECK(pm.findElement(map)->links == QStringList{novel.chapterId});
    REQUIRE(links->topLevelItemCount() == 1);
    CHECK(links->topLevelItem(0)->text(0) == QStringLiteral("The Storm"));
    CHECK_FALSE(links->topLevelItem(0)->text(1).isEmpty());
    CHECK_FALSE(links->isHidden());

    // A node of the Navigator carries its link: a section, an element
    const gui::NavigatorPanel& navigator = *window.navigatorPanel;
    QTreeWidgetItem* bodyItem = navigatorItem(navigator.tree(), QStringLiteral("section_body"));
    REQUIRE(bodyItem != nullptr);
    CHECK(gui::NavigatorPanel::linkOf(bodyItem) == body);
    CHECK(gui::NavigatorPanel::linkOf(navigatorItem(navigator.tree(), map)) == map);
    const std::unique_ptr<QMimeData> section = dragged(navigator, bodyItem);
    CHECK(gui::NavigatorPanel::linksOf(section.get()) == QStringList{body});

    // Dragged over the links, it is copied where the drag lets it be, else moved, so that the
    // Navigator keeps it; other data is not taken
    QDragEnterEvent enterBoth(QPoint(5, 5), Qt::CopyAction | Qt::MoveAction, section.get(),
                              Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(links->viewport(), &enterBoth);
    CHECK(enterBoth.isAccepted());
    CHECK(enterBoth.dropAction() == Qt::CopyAction);
    QDragEnterEvent enterMove(QPoint(5, 5), Qt::MoveAction, section.get(), Qt::LeftButton,
                              Qt::NoModifier);
    QCoreApplication::sendEvent(links->viewport(), &enterMove);
    CHECK(enterMove.isAccepted());
    CHECK(enterMove.dropAction() == Qt::MoveAction);
    QMimeData text;
    text.setText(QStringLiteral("The Storm"));
    QDragEnterEvent enterText(QPoint(5, 5), Qt::CopyAction, &text, Qt::LeftButton,
                              Qt::NoModifier);
    QCoreApplication::sendEvent(links->viewport(), &enterText);
    CHECK_FALSE(enterText.isAccepted());

    // Dropped, it is linked after the links the material has
    QDropEvent drop(QPointF(5, 5), Qt::CopyAction | Qt::MoveAction, section.get(),
                    Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(links->viewport(), &drop);
    CHECK(drop.isAccepted());
    CHECK(pm.findElement(map)->links == (QStringList{novel.chapterId, body}));
    CHECK(links->topLevelItemCount() == 2);
    CHECK(manifestFile(pm).contains(body.toUtf8()));

    // The same node again, or the material itself, links nothing more
    const std::unique_ptr<QMimeData> chapter =
        dragged(navigator, navigatorItem(navigator.tree(), novel.chapterId));
    QDropEvent again(QPointF(5, 5), Qt::CopyAction, chapter.get(), Qt::LeftButton,
                     Qt::NoModifier);
    QCoreApplication::sendEvent(links->viewport(), &again);
    page.addLinks({map});
    CHECK(pm.findElement(map)->links == (QStringList{novel.chapterId, body}));

    // Enter opens the link; Delete takes it out, and the next link is the current one
    links->setCurrentItem(links->topLevelItem(0));
    press(links, Qt::Key_Return);
    CHECK(opened == QStringList{novel.chapterId});
    CHECK(page.removeLinkButton()->isEnabled());
    press(links, Qt::Key_Delete);
    CHECK(pm.findElement(map)->links == QStringList{body});
    REQUIRE(links->currentItem() != nullptr);
    CHECK(links->currentItem()->text(0) == links->topLevelItem(0)->text(0));
    page.removeLinkButton()->click();
    CHECK(pm.findElement(map)->links.isEmpty());
    CHECK(links->isHidden());

    // Insert opens Link, where the checked elements are linked
    CHECK(linking([&]() { press(links, Qt::Key_Insert); },
                  [&](gui::dialogs::LinkDialog& dialog) {
                      REQUIRE(dialog.itemOf(novel.chapterId) != nullptr);
                      dialog.itemOf(novel.chapterId)->setCheckState(0, Qt::Checked);
                  }) == 1);
    CHECK(pm.findElement(map)->links == QStringList{novel.chapterId});
}

TEST_CASE("Link: a material is linked with the sections and the elements, not with itself",
          "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString source = writePicture(files.path(), QStringLiteral("map.png"), QSize(40, 20));
    const QString map = addMaterial(pm, source, QStringLiteral("Map"));
    const QString photo = addMaterial(pm, source, QStringLiteral("Photo"));
    REQUIRE(pm.setLinks(map, {novel.chapterId}));
    const QString body = pm.project()->sectionLink(core::BookPlace::Main);

    gui::dialogs::LinkDialog dialog(gui::dialogs::LinkDialog::Mode::Targets, map);
    QTreeWidget* tree = dialog.tree();

    // The book with its sections and their elements, and the Workshop with the other materials
    REQUIRE(tree->topLevelItemCount() == 2);
    const QTreeWidgetItem* book = tree->topLevelItem(0);
    CHECK_FALSE(book->flags().testFlag(Qt::ItemIsUserCheckable));
    REQUIRE(book->childCount() == 3);
    QTreeWidgetItem* bodyItem = dialog.itemOf(body);
    REQUIRE(bodyItem != nullptr);
    CHECK(bodyItem->parent() == book);
    CHECK(bodyItem->flags().testFlag(Qt::ItemIsUserCheckable));
    QTreeWidgetItem* chapter = dialog.itemOf(novel.chapterId);
    REQUIRE(chapter != nullptr);
    CHECK(chapter->parent() == bodyItem);
    CHECK(chapter->text(0) == QStringLiteral("The Storm"));
    CHECK(dialog.itemOf(map) == nullptr);
    REQUIRE(dialog.itemOf(photo) != nullptr);
    const QTreeWidgetItem* group = dialog.itemOf(photo)->parent();
    REQUIRE(group != nullptr);
    CHECK(group->text(0) == QStringLiteral("Materials"));
    CHECK_FALSE(group->flags().testFlag(Qt::ItemIsUserCheckable));
    CHECK(group->parent() == tree->topLevelItem(1));

    // What is linked now is checked
    CHECK(chapter->checkState(0) == Qt::Checked);
    CHECK(bodyItem->checkState(0) == Qt::Unchecked);
    CHECK(dialog.initialLinks() == QStringList{novel.chapterId});

    // The letters typed in the tree go to the filter, which shows the items with them and those
    // above; the current item is one of them
    press(tree, Qt::Key_S, Qt::NoModifier, QStringLiteral("s"));
    press(tree, Qt::Key_T, Qt::NoModifier, QStringLiteral("t"));
    CHECK(dialog.filterEdit()->text() == QStringLiteral("st"));
    press(tree, Qt::Key_Backspace);
    press(tree, Qt::Key_T, Qt::NoModifier, QStringLiteral("t"));
    press(tree, Qt::Key_O, Qt::NoModifier, QStringLiteral("o"));
    press(tree, Qt::Key_R, Qt::NoModifier, QStringLiteral("r"));
    CHECK(dialog.filterEdit()->text() == QStringLiteral("stor"));
    CHECK_FALSE(chapter->isHidden());
    CHECK_FALSE(bodyItem->isHidden());
    CHECK(dialog.itemOf(photo)->isHidden());
    CHECK(tree->currentItem() == chapter);
    dialog.filterEdit()->clear();
    CHECK_FALSE(dialog.itemOf(photo)->isHidden());

    // The checked links, in the order of the tree
    bodyItem->setCheckState(0, Qt::Checked);
    dialog.itemOf(photo)->setCheckState(0, Qt::Checked);
    CHECK(dialog.checkedLinks() == (QStringList{body, novel.chapterId, photo}));
}

TEST_CASE("Materials of an element: the chapter, the section and the part link them, in Link "
          "with the materials of the Workshop", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString source = writePicture(files.path(), QStringLiteral("map.png"), QSize(40, 20));
    const QString map = addMaterial(pm, source, QStringLiteral("Map"));
    const QString photo = addMaterial(pm, source, QStringLiteral("Photo"));
    REQUIRE(pm.setFields(map, {core::ElementField{QStringLiteral("Scale"),
                                                  QStringLiteral("1 : 50 000"), {}}}));
    REQUIRE(pm.setLinks(map, {novel.chapterId}));
    const QString part = pm.addElement(
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QStringLiteral("part")),
        QStringLiteral("Part One"), core::BookPlace::Main);
    REQUIRE_FALSE(part.isEmpty());

    gui::PropertiesPanel properties;
    QStringList opened;
    QObject::connect(&properties, &gui::PropertiesPanel::requestOpenMaterial,
                     [&](const QString& id) { opened << id; });

    // A chapter: its materials, each with its first field under its title
    properties.showChapterProperties(novel.chapterId);
    CHECK(properties.currentPage() == gui::PropertiesPanel::Page::Chapter);
    gui::LinkedMaterialsBox* box = properties.chapterMaterials();
    CHECK(box->link() == novel.chapterId);
    REQUIRE(box->list()->count() == 1);
    CHECK(box->list()->item(0)->text() == QStringLiteral("Map\nScale: 1 : 50 000"));
    CHECK(box->emptyLabel()->isHidden());

    // Enter opens the material
    box->list()->setCurrentRow(0);
    press(box->list(), Qt::Key_Return);
    CHECK(opened == QStringList{map});

    // "+" opens Link with the materials of the Workshop, the linked ones checked
    CHECK(linking([&]() { box->addButton()->click(); },
                  [&](gui::dialogs::LinkDialog& dialog) {
                      REQUIRE(dialog.tree()->topLevelItemCount() == 1);
                      CHECK(dialog.itemOf(novel.chapterId) == nullptr);
                      REQUIRE(dialog.itemOf(map) != nullptr);
                      CHECK(dialog.itemOf(map)->checkState(0) == Qt::Checked);
                      REQUIRE(dialog.itemOf(photo) != nullptr);
                      CHECK(dialog.itemOf(photo)->checkState(0) == Qt::Unchecked);
                      dialog.itemOf(map)->setCheckState(0, Qt::Unchecked);
                      dialog.itemOf(photo)->setCheckState(0, Qt::Checked);
                  }) == 1);
    CHECK(pm.findElement(map)->links.isEmpty());
    CHECK(pm.findElement(photo)->links == QStringList{novel.chapterId});
    REQUIRE(box->list()->count() == 1);
    CHECK(box->list()->item(0)->text() == QStringLiteral("Photo"));

    // Delete takes the link out of the material
    box->list()->setCurrentRow(0);
    press(box->list(), Qt::Key_Delete);
    CHECK(pm.findElement(photo)->links.isEmpty());
    CHECK(box->list()->isHidden());
    CHECK(box->emptyLabel()->text() == QStringLiteral("This chapter has no linked materials."));

    // A section and a part have them too
    REQUIRE(pm.setLinks(map, {pm.project()->sectionLink(core::BookPlace::Main), part}));
    properties.showSectionProperties(QStringLiteral("section_body"));
    CHECK(properties.sectionMaterials()->link() ==
          pm.project()->sectionLink(core::BookPlace::Main));
    CHECK(properties.sectionMaterials()->list()->count() == 1);
    properties.showPartProperties(part);
    CHECK(properties.partMaterials()->link() == part);
    CHECK(properties.partMaterials()->list()->count() == 1);
    REQUIRE(pm.setLinks(map, {}));
    properties.refresh();
    CHECK(properties.partMaterials()->emptyLabel()->text() ==
          QStringLiteral("This part has no linked materials."));

    // A material shows its card
    properties.showChapterProperties(map);
    CHECK(properties.currentPage() == gui::PropertiesPanel::Page::Material);
    CHECK(properties.materialPage()->elementId() == map);
}

TEST_CASE("Preview: the zoom, the fit, the turn saved with the material, the negative and the "
          "keys", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString map = addMaterial(
        pm, writePicture(files.path(), QStringLiteral("map.png"), QSize(400, 200)),
        QStringLiteral("Map"));

    gui::ImagePreviewPanel preview(map);
    QList<double> zooms;
    QObject::connect(&preview, &gui::ImagePreviewPanel::zoomChanged,
                     [&](double zoom) { zooms << zoom; });
    preview.resize(300, 300);
    preview.show();
    QApplication::processEvents();
    REQUIRE(preview.hasPicture());
    CHECK(preview.property("elementId").toString() == map);
    CHECK(preview.problem().isEmpty());

    // Fitted to a small window, smaller; to a large one, at 100% at most
    CHECK(preview.fitsWindow());
    CHECK(preview.zoom() < 1.0);
    CHECK(preview.zoom() > 0.5);
    CHECK_FALSE(zooms.isEmpty());
    preview.resize(1200, 900);
    QApplication::processEvents();
    CHECK(preview.zoom() == 1.0);
    CHECK(preview.zoomLabel()->text() == QStringLiteral("100%"));

    // The steps of Zoom In and Zoom Out, up to the largest
    preview.zoomIn();
    CHECK_FALSE(preview.fitsWindow());
    CHECK(preview.zoom() == 1.25);
    preview.zoomOut();
    preview.zoomOut();
    CHECK(preview.zoom() == 0.75);
    for (int i = 0; i < 20; ++i) {
        preview.zoomIn();
    }
    CHECK(preview.zoom() == gui::ImagePreviewPanel::zoomSteps().last());
    preview.zoomReset();
    CHECK(preview.zoom() == 1.0);
    preview.zoomToFit();
    CHECK(preview.fitsWindow());

    // R and L turn it by a right angle, saved with the material; Ctrl+L is no turn
    press(preview.view(), Qt::Key_R, Qt::NoModifier, QStringLiteral("r"));
    CHECK(preview.rotation() == 90);
    CHECK(preview.pictureSize() == QSize(200, 400));
    CHECK(pm.findElement(map)->rotation == 90);
    press(preview.view(), Qt::Key_L, Qt::NoModifier, QStringLiteral("l"));
    press(preview.view(), Qt::Key_L, Qt::NoModifier, QStringLiteral("l"));
    CHECK(preview.rotation() == 270);
    CHECK(pm.findElement(map)->rotation == 270);
    press(preview.view(), Qt::Key_L, Qt::ControlModifier);
    CHECK(preview.rotation() == 270);
    CHECK(manifestFile(pm).contains("270"));

    // N shows the negative, which is the tab's own
    press(preview.view(), Qt::Key_N, Qt::NoModifier, QStringLiteral("n"));
    CHECK(preview.isNegative());
    gui::ImagePreviewPanel another(map);
    CHECK_FALSE(another.isNegative());
    CHECK(another.rotation() == 270);
    preview.toggleNegative();
    CHECK_FALSE(preview.isNegative());
}

TEST_CASE("Preview: a file the program cannot show says why and opens in the system program",
          "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString sketch = addMaterial(
        pm, writeFile(files.path(), QStringLiteral("sketch.xcf"), QByteArray("gimp xcf v011")),
        QStringLiteral("Sketch"));
    const QString notes = addMaterial(
        pm, writeFile(files.path(), QStringLiteral("notes.txt"), QByteArray("Notes\n")),
        QStringLiteral("Notes"));

    gui::ImagePreviewPanel preview(sketch);
    QStringList opened;
    QObject::connect(&preview, &gui::ImagePreviewPanel::openWithSystemRequested,
                     [&](const QString& id) { opened << id; });
    CHECK_FALSE(preview.hasPicture());
    CHECK(preview.problem() == QStringLiteral(
              "The program does not show XCF pictures. Open the file in the system program."));
    auto* button = preview.findChild<QPushButton*>(QStringLiteral("imagePreviewOpenSystem"));
    REQUIRE(button != nullptr);
    button->click();
    CHECK(opened == QStringList{sketch});

    // Nothing turns or zooms
    preview.rotateRight();
    preview.zoomIn();
    CHECK(preview.rotation() == 0);
    CHECK(pm.findElement(sketch)->rotation == 0);

    gui::ImagePreviewPanel text(notes);
    CHECK(text.problem() == QStringLiteral("The program does not show files of this kind. Open "
                                           "the file in the system program."));

    // A damaged picture of a format the program shows
    const QString cut = addMaterial(
        pm, writeCutPicture(files.path(), QStringLiteral("cut.png")), QStringLiteral("Cut"));
    gui::ImagePreviewPanel damaged(cut);
    CHECK_FALSE(damaged.hasPicture());
    CHECK(damaged.problem().startsWith(QStringLiteral("The picture cannot be read (")));
    CHECK(damaged.problem().endsWith(QStringLiteral("). It may be damaged.")));
}

TEST_CASE("Navigator: a material clicked shows its card, and Alt+Enter brings the Properties "
          "panel forward with the keys in it", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString map = addMaterial(
        pm, writePicture(files.path(), QStringLiteral("map.png"), QSize(40, 20)),
        QStringLiteral("Map"));
    test::Window window;
    gui::NavigatorPanel& navigator = *window.navigatorPanel;
    int panels = 0;
    int focuses = 0;
    QStringList shown;
    QObject::connect(&navigator, &gui::NavigatorPanel::requestProperties,
                     [&](const QString& id) { shown << id; });
    QObject::connect(&navigator, &gui::NavigatorPanel::requestSectionProperties,
                     [&](const QString& type) { shown << type; });
    QObject::connect(&navigator, &gui::NavigatorPanel::requestPropertiesPanel,
                     [&]() { ++panels; });
    QObject::connect(&navigator, &gui::NavigatorPanel::requestPropertiesFocus,
                     [&]() { ++focuses; });
    QTreeWidget* tree = navigator.tree();
    QTreeWidgetItem* mapItem = navigatorItem(tree, map);
    REQUIRE(mapItem != nullptr);
    CHECK(gui::NavigatorPanel::isMaterialItem(mapItem));
    QTreeWidgetItem* chapterItem = navigatorItem(tree, novel.chapterId);
    REQUIRE(chapterItem != nullptr);
    CHECK_FALSE(gui::NavigatorPanel::isMaterialItem(chapterItem));

    // Made current by the program, it shows its card and leaves the panel where it is
    tree->setCurrentItem(mapItem);
    CHECK(shown == QStringList{map});
    CHECK(panels == 0);

    // Clicked, it brings the panel forward; a chapter clicked does not
    emit tree->itemClicked(mapItem, 0);
    CHECK(panels == 1);
    emit tree->itemClicked(chapterItem, 0);
    CHECK(panels == 1);
    CHECK(shown.last() == novel.chapterId);
    CHECK(focuses == 0);

    // Alt+Enter on any node: the panel forward, with the keys in it
    tree->setCurrentItem(chapterItem);
    press(tree, Qt::Key_Return, Qt::AltModifier);
    CHECK(panels == 2);
    CHECK(focuses == 1);
    tree->setCurrentItem(navigatorItem(tree, QStringLiteral("section_body")));
    press(tree, Qt::Key_Return, Qt::AltModifier);
    CHECK(shown.last() == QStringLiteral("section_body"));
    CHECK(panels == 3);
    CHECK(focuses == 2);
}

TEST_CASE("Links opened: a section in the Navigator, an element in its tab, a material in its "
          "preview", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Rain"));
    auto& pm = novel.pm;
    QTemporaryDir files;
    REQUIRE(files.isValid());
    const QString map = addMaterial(
        pm, writePicture(files.path(), QStringLiteral("map.png"), QSize(40, 20)),
        QStringLiteral("Map"));
    OpenMaterialsSetting card("card");
    test::Window window;
    QTreeWidget* tree = window.navigatorPanel->tree();

    window.navigator.onRequestOpenLink(pm.project()->sectionLink(core::BookPlace::Back));
    REQUIRE(tree->currentItem() != nullptr);
    CHECK(tree->currentItem()->data(0, Qt::UserRole + 1).toString() ==
          QStringLiteral("section_backmatter"));
    CHECK(window.tabs->count() == 0);

    window.navigator.onRequestOpenLink(novel.chapterId);
    CHECK(tree->currentItem() == navigatorItem(tree, novel.chapterId));
    REQUIRE(window.tabs->count() == 1);
    CHECK(window.tabs->currentWidget()->property("elementId").toString() == novel.chapterId);

    window.navigator.onRequestOpenLink(map);
    REQUIRE(window.tabs->count() == 2);
    auto* preview = qobject_cast<gui::ImagePreviewPanel*>(window.tabs->currentWidget());
    REQUIRE(preview != nullptr);
    CHECK(preview->elementId() == map);
    CHECK(window.tabs->tabText(window.tabs->currentIndex()) == QStringLiteral("Map"));

    // The card's Preview, connected as the main window connects it, opens the same tab
    QObject::connect(window.properties, &gui::PropertiesPanel::requestPreview, &window.navigator,
                     &gui::NavigatorCoordinator::onRequestPreview);
    window.properties->showChapterProperties(map);
    window.tabs->setCurrentIndex(0);
    window.properties->materialPage()->previewButton()->click();
    CHECK(window.tabs->count() == 2);
    CHECK(window.tabs->currentWidget() == preview);

    // The picture turned in the preview is turned on the card too
    CHECK(window.properties->materialPage()->thumbnail()->iconSize() == QSize(40, 20));
    preview->rotateRight();
    CHECK(pm.findElement(map)->rotation == 90);
    CHECK(window.properties->materialPage()->thumbnail()->iconSize() == QSize(20, 40));
}
