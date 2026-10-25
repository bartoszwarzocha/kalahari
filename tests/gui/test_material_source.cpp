/// @file test_material_source.cpp
/// @brief Where a material comes from: what comes to the Navigator from outside the program,
/// the downloads for a material with a web address, and its card for each source

#include "document_window.h"

#include "kalahari/core/material_kinds.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_downloader.h"
#include "kalahari/gui/material_source.h"
#include "kalahari/gui/panels/material_page.h"

#include <catch2/catch_test_macros.hpp>

#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFormLayout>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QMimeData>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

#include <cstdio>
#include <optional>

using namespace kalahari;

namespace {

/// A PNG picture of @p size
QByteArray pngOf(const QSize& size) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::darkYellow);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    REQUIRE(image.save(&buffer, "PNG"));
    return bytes;
}

/// A file with @p bytes in folder @p dir
QString writeFile(const QString& dir, const QString& name, const QByteArray& bytes) {
    const QString path = QDir(dir).filePath(name);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(bytes) == bytes.size());
    return path;
}

/// What download @p start of @p downloader gives, waiting for it at most 10 seconds
std::optional<gui::MaterialDownloader::Result> downloaded(
    gui::MaterialDownloader& downloader, const std::function<void()>& start) {
    std::optional<gui::MaterialDownloader::Result> result;
    QEventLoop loop;
    const QMetaObject::Connection connection = QObject::connect(
        &downloader, &gui::MaterialDownloader::finished,
        [&](const gui::MaterialDownloader::Result& ended) {
            result = ended;
            loop.quit();
        });
    QTimer::singleShot(10000, &loop, &QEventLoop::quit);
    start();
    if (!result) {
        loop.exec();
    }
    QObject::disconnect(connection);
    return result;
}

/// The setting of the downloads, as it was before the test
class DownloadsSetting {
public:
    explicit DownloadsSetting(const std::string& value)
        : m_saved(core::SettingsManager::getInstance().get<std::string>(
              gui::MaterialDownloader::SETTING, "ask")) {
        core::SettingsManager::getInstance().set<std::string>(gui::MaterialDownloader::SETTING,
                                                              value);
    }
    ~DownloadsSetting() {
        try {
            core::SettingsManager::getInstance().set<std::string>(
                gui::MaterialDownloader::SETTING, m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            std::fputs("DownloadsSetting: the setting of the test stays\n", stderr);
        }
    }
    DownloadsSetting(const DownloadsSetting&) = delete;
    DownloadsSetting& operator=(const DownloadsSetting&) = delete;

private:
    std::string m_saved;
};

/// Add @p material to the Workshop of the open book, of the kind its source gives it; a quote
/// without one
QString addMaterial(core::ProjectManager& pm, const core::NewMaterial& material) {
    QString kindId = core::MaterialKinds::ofSource(
        QUrl(material.url), material.filePath.isEmpty() ? material.fileName : material.filePath);
    if (kindId.isEmpty()) {
        kindId = QString::fromLatin1(core::MaterialKinds::QUOTE);
    }
    const QString id = pm.addMaterial(
        material, pm.bookTypes().findKind(QStringLiteral("kalahari.base"), kindId));
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// Whether @p widget would be seen when @p page is
bool shown(const QWidget* widget, const QWidget& page) {
    return widget->isVisibleTo(&page);
}

} // namespace

TEST_CASE("Material source: a web address, a title and a file name",
          "[gui][materials]") {
    // A web address has http or https and a host, and no spaces in it
    CHECK(gui::webAddress(QStringLiteral("  https://en.wikipedia.org/wiki/Kalahari  ")) ==
          QUrl(QStringLiteral("https://en.wikipedia.org/wiki/Kalahari")));
    CHECK_FALSE(gui::webAddress(QStringLiteral("http://example.com")).isEmpty());
    CHECK(gui::webAddress(QStringLiteral("www.example.com")).isEmpty());
    CHECK(gui::webAddress(QStringLiteral("ftp://example.com/map.png")).isEmpty());
    CHECK(gui::webAddress(QStringLiteral("https:///map.png")).isEmpty());
    CHECK(gui::webAddress(QStringLiteral("https://example.com/a b")).isEmpty());
    CHECK(gui::webAddress(QStringLiteral("https://example.com/\nhttps://example.org/")).isEmpty());
    CHECK(gui::webAddress(QStringLiteral("The Kalahari is a desert")).isEmpty());

    // The title the end of an address gives
    CHECK(gui::titleOfAddress(
              QUrl(QStringLiteral("https://en.wikipedia.org/wiki/Kalahari_Desert"))) ==
          QStringLiteral("Kalahari Desert"));
    CHECK(gui::titleOfAddress(QUrl(QStringLiteral("https://example.com/x/Camel_on_a_dune.jpg"))) ==
          QStringLiteral("Camel on a dune"));
    CHECK(gui::titleOfAddress(
              QUrl(QStringLiteral("https://pl.wikipedia.org/wiki/Kalahari_%E2%80%93_pustynia"))) ==
          QString::fromUtf8("Kalahari – pustynia"));
    CHECK(gui::titleOfAddress(QUrl(QStringLiteral("https://www.example.com/"))) ==
          QStringLiteral("example.com"));

    // A name for the copy of a picture
    CHECK(gui::fileNameOfAddress(QUrl(QStringLiteral("https://example.com/Camel_on_a_dune.JPG"))) ==
          QStringLiteral("Camel_on_a_dune.jpg"));
    CHECK(gui::fileNameOfAddress(QUrl(QStringLiteral("https://example.com/photo")),
                                 QStringLiteral("png")) == QStringLiteral("photo.png"));
    CHECK(gui::fileNameOfAddress(QUrl(QStringLiteral("https://example.com/photo.jpeg")),
                                 QStringLiteral("jpg")) == QStringLiteral("photo.jpg"));
    CHECK(gui::fileNameOfAddress(QUrl(QStringLiteral("https://example.com/")),
                                 QStringLiteral("jpg")) == QStringLiteral("picture.jpg"));
    CHECK(gui::fileNameOfAddress(QUrl(QStringLiteral("https://example.com/a%3Ab%22c%7C.png"))) ==
          QStringLiteral("a_b_c_.png"));
    const QString longName =
        gui::fileNameOfAddress(QUrl(QStringLiteral("https://example.com/") +
                                    QString(150, QLatin1Char('x')) + QStringLiteral(".png")));
    CHECK(longName == QString(100, QLatin1Char('x')) + QStringLiteral(".png"));
}

TEST_CASE("Material source: what a material comes from, and what is dropped or pasted",
          "[gui][materials]") {
    core::ProjectElement element;
    CHECK(gui::materialSourceOf(element) == gui::MaterialSource::None);
    element.file = QStringLiteral("workshop/map.png");
    CHECK(gui::materialSourceOf(element) == gui::MaterialSource::File);
    element.url = QStringLiteral("https://example.com/map");
    // A copy of a picture on the web
    CHECK(gui::materialSourceOf(element) == gui::MaterialSource::WebPicture);
    element.file.clear();
    CHECK(gui::materialSourceOf(element) == gui::MaterialSource::Page);
    element.url = QStringLiteral("https://example.com/map.png");
    CHECK(gui::materialSourceOf(element) == gui::MaterialSource::WebPicture);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString notes = writeFile(dir.path(), QStringLiteral("notes.txt"), "Dunes");

    SECTION("files and web addresses of a list of addresses") {
        QMimeData data;
        data.setUrls({QUrl::fromLocalFile(notes),
                      QUrl::fromLocalFile(QDir(dir.path()).filePath(QStringLiteral("gone.txt"))),
                      QUrl::fromLocalFile(dir.path()),
                      QUrl(QStringLiteral("https://example.com/kalahari")),
                      QUrl(QStringLiteral("mailto:someone@example.com"))});
        const gui::IncomingMaterials incoming = gui::IncomingMaterials::of(&data);
        CHECK(incoming.files == QStringList{QFileInfo(notes).absoluteFilePath()});
        CHECK(incoming.addresses ==
              QList<QUrl>{QUrl(QStringLiteral("https://example.com/kalahari"))});
        CHECK(incoming.picture.isNull());
        CHECK(incoming.hasMaterials());
    }
    SECTION("a picture without a file") {
        QMimeData data;
        data.setImageData(QImage::fromData(pngOf(QSize(30, 20)), "PNG"));
        const gui::IncomingMaterials incoming = gui::IncomingMaterials::of(&data);
        CHECK(incoming.picture.size() == QSize(30, 20));
        CHECK(incoming.files.isEmpty());
        CHECK(incoming.hasMaterials());
    }
    SECTION("a text that is a web address, a path in quotes, or neither") {
        QMimeData address;
        address.setText(QStringLiteral(" https://example.com/dune.jpg\n"));
        CHECK(gui::IncomingMaterials::of(&address).addresses ==
              QList<QUrl>{QUrl(QStringLiteral("https://example.com/dune.jpg"))});

        QMimeData path;
        path.setText(QLatin1Char('"') + QDir::toNativeSeparators(notes) + QLatin1Char('"'));
        CHECK(gui::IncomingMaterials::of(&path).files ==
              QStringList{QFileInfo(notes).absoluteFilePath()});

        QMimeData sentence;
        sentence.setText(QStringLiteral("The Kalahari is a desert"));
        const gui::IncomingMaterials incoming = gui::IncomingMaterials::of(&sentence);
        CHECK_FALSE(incoming.hasMaterials());
        CHECK(incoming.text == QStringLiteral("The Kalahari is a desert"));

        CHECK_FALSE(gui::IncomingMaterials::of(nullptr).hasMaterials());
    }

    // A picture without a file is named by when it was pasted
    const QDateTime time(QDate(2026, 10, 20), QTime(17, 5, 12));
    CHECK(gui::IncomingMaterials::pictureTitle(time) ==
          QStringLiteral("Picture from the clipboard ") +
              QLocale().toString(time, QLocale::ShortFormat));
    CHECK(gui::IncomingMaterials::pictureFileName(time) ==
          QStringLiteral("clipboard_20261020_170512.png"));
}

TEST_CASE("Material downloads: the title of a page, the kind of a picture, and what a download "
          "gives", "[gui][materials]") {
    using gui::MaterialDownloader;

    // The title, with the characters its entities name, in its charset
    CHECK(MaterialDownloader::pageTitle(
              "<html><head><TITLE lang=\"en\">\n  Kalahari &amp; Namib &#8211; deserts\n"
              "</title></head><body><title>Not this</title></body></html>") ==
          QString::fromUtf8("Kalahari & Namib – deserts"));
    CHECK(MaterialDownloader::pageTitle("<title>\xc5\x81\xc3\xb3" "d\xc5\xba</title>") ==
          QString::fromUtf8("Łódź"));
    CHECK(MaterialDownloader::pageTitle("<title>Caf\xe9</title>",
                                        "text/html; charset=ISO-8859-1") ==
          QString::fromUtf8("Café"));
    CHECK(MaterialDownloader::pageTitle(
              "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">"
              "<title>Caf\xe9</title>") == QString::fromUtf8("Café"));
    // Letters of a charset the program cannot read give no title
    CHECK(MaterialDownloader::pageTitle("<title>\xb3\xf3" "d\x9f</title>",
                                        "text/html; charset=x-unknown-charset")
              .isEmpty());
    CHECK(MaterialDownloader::pageTitle("<html><body>No title</body></html>").isEmpty());
    CHECK(MaterialDownloader::pageTitle("<title></title>").isEmpty());

    // The kind of a picture by its data, not its name
    CHECK(MaterialDownloader::pictureSuffix(pngOf(QSize(4, 4))) == QStringLiteral("png"));
    {
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::white);
        QByteArray jpeg;
        QBuffer buffer(&jpeg);
        REQUIRE(buffer.open(QIODevice::WriteOnly));
        REQUIRE(image.save(&buffer, "JPEG"));
        CHECK(MaterialDownloader::pictureSuffix(jpeg) == QStringLiteral("jpg"));
    }
    CHECK(MaterialDownloader::pictureSuffix("<html>not a picture</html>").isEmpty());

    // What a download gives: here from files, as the program has no network in the tests
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString page = writeFile(dir.path(), QStringLiteral("dunes.html"),
                                   "<html><head><title>The Red Dunes</title></head></html>");
    const QString picture =
        writeFile(dir.path(), QStringLiteral("Camel_on_a_dune.png"), pngOf(QSize(12, 8)));
    MaterialDownloader downloader;

    auto title = downloaded(downloader, [&]() {
        downloader.fetchTitle(QStringLiteral("m1"), QUrl::fromLocalFile(page));
        CHECK(downloader.isFetching(QStringLiteral("m1")));
    });
    REQUIRE(title.has_value());
    CHECK(title->elementId == QStringLiteral("m1"));
    CHECK_FALSE(title->picture);
    CHECK(title->error.isEmpty());
    CHECK(title->title == QStringLiteral("The Red Dunes"));
    CHECK_FALSE(downloader.isFetching(QStringLiteral("m1")));

    auto copy = downloaded(downloader, [&]() {
        downloader.fetchPicture(QStringLiteral("m2"), QUrl::fromLocalFile(picture));
    });
    REQUIRE(copy.has_value());
    CHECK(copy->picture);
    CHECK(copy->error.isEmpty());
    CHECK(copy->data == pngOf(QSize(12, 8)));
    CHECK(copy->fileName == QStringLiteral("Camel_on_a_dune.png"));

    // A page is no picture; an address with nothing at it says so
    auto notPicture = downloaded(downloader, [&]() {
        downloader.fetchPicture(QStringLiteral("m3"), QUrl::fromLocalFile(page));
    });
    REQUIRE(notPicture.has_value());
    CHECK(notPicture->data.isEmpty());
    CHECK(notPicture->error ==
          QStringLiteral("There is no picture the program knows at this address."));
    auto missing = downloaded(downloader, [&]() {
        downloader.fetchTitle(QStringLiteral("m4"), QUrl::fromLocalFile(QDir(dir.path()).filePath(
                                                        QStringLiteral("gone"))));
    });
    REQUIRE(missing.has_value());
    CHECK_FALSE(missing->error.isEmpty());
    CHECK(missing->title.isEmpty());
}

TEST_CASE("Material card: a page, a picture on the web with and without its copy, and a material "
          "without a file", "[gui][materials]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Caravan"));
    auto& pm = novel.pm;
    DownloadsSetting downloads("ask");

    const QString pageUrl = QStringLiteral("https://en.wikipedia.org/wiki/Kalahari_Desert");
    const QString pictureUrl = QStringLiteral("https://upload.example.org/Camel_on_a_dune.jpg");
    const QString pageId =
        addMaterial(pm, {QStringLiteral("Kalahari Desert"), {}, {}, {}, pageUrl, {}});
    const QString pictureId =
        addMaterial(pm, {QStringLiteral("Camel on a dune"), {}, {}, {}, pictureUrl, {}});
    const QString quoteId =
        addMaterial(pm, {QStringLiteral("What the guide said"), {}, {}, {}, {}, {}});

    gui::MaterialPage page;
    QStringList browsers;
    QStringList copies;
    QStringList messages;
    QObject::connect(&page, &gui::MaterialPage::requestOpenInBrowser,
                     [&](const QString& id) { browsers << id; });
    QObject::connect(&page, &gui::MaterialPage::requestSaveCopy,
                     [&](const QString& id) { copies << id; });
    QObject::connect(&page, &gui::MaterialPage::message,
                     [&](const QString& text) { messages << text; });
    page.resize(320, 900);
    page.show();

    // A page: its host, Open in Browser and Copy Address; no file
    page.showMaterial(pageId);
    QApplication::processEvents();
    CHECK(page.fileLabel()->text() == QStringLiteral("en.wikipedia.org"));
    CHECK_FALSE(shown(page.thumbnailProblem(), page));
    CHECK(shown(page.browserButton(), page));
    CHECK(shown(page.copyAddressButton(), page));
    CHECK_FALSE(shown(page.previewButton(), page));
    CHECK_FALSE(shown(page.systemButton(), page));
    CHECK_FALSE(shown(page.saveCopyButton(), page));
    CHECK(page.addressLabel()->text() == pageUrl);
    CHECK(shown(page.addressLabel(), page));
    CHECK_FALSE(shown(page.pathLabel(), page));
    page.browserButton()->click();
    CHECK(browsers == QStringList{pageId});
    page.copyAddressButton()->click();
    CHECK(QGuiApplication::clipboard()->text() == pageUrl);
    CHECK(messages == QStringList{QStringLiteral("Address copied: ") + pageUrl});

    // A picture on the web without its copy: Save Copy, which waits for its download
    page.showMaterial(pictureId);
    CHECK(page.thumbnailProblem()->text() ==
          QStringLiteral("The program shows the picture once the book has its copy."));
    CHECK(shown(page.thumbnailProblem(), page));
    CHECK(page.fileLabel()->text() == QStringLiteral("upload.example.org"));
    CHECK(shown(page.browserButton(), page));
    CHECK_FALSE(shown(page.copyAddressButton(), page));
    CHECK_FALSE(shown(page.previewButton(), page));
    REQUIRE(shown(page.saveCopyButton(), page));
    CHECK(page.saveCopyButton()->isEnabled());
    page.saveCopyButton()->click();
    CHECK(copies == QStringList{pictureId});
    page.setDownloading(QStringLiteral("another"), true);
    CHECK(page.saveCopyButton()->isEnabled());
    page.setDownloading(pictureId, true);
    CHECK_FALSE(page.saveCopyButton()->isEnabled());
    CHECK(page.saveCopyButton()->text() == QStringLiteral("Downloading..."));
    page.setDownloading(pictureId, false);
    CHECK(page.saveCopyButton()->isEnabled());
    CHECK(page.saveCopyButton()->text() == QStringLiteral("Save Copy"));
    {
        // Without downloads there is no Save Copy
        DownloadsSetting never("never");
        page.refresh();
        CHECK_FALSE(shown(page.saveCopyButton(), page));
        CHECK(shown(page.browserButton(), page));
    }
    page.refresh();
    CHECK(shown(page.saveCopyButton(), page));

    // With its copy: the picture, Preview, and the copy's file
    REQUIRE(pm.setMaterialCopy(pictureId, pngOf(QSize(80, 40)),
                               QStringLiteral("Camel_on_a_dune.png")));
    page.refresh();
    CHECK_FALSE(shown(page.thumbnailProblem(), page));
    CHECK(page.thumbnail()->iconSize().height() == page.thumbnail()->iconSize().width() / 2);
    CHECK(page.fileLabel()->text().endsWith(QStringLiteral(" · copy from the web")));
    CHECK(shown(page.previewButton(), page));
    CHECK(page.previewButton()->isEnabled());
    CHECK_FALSE(shown(page.saveCopyButton(), page));
    CHECK_FALSE(shown(page.systemButton(), page));
    CHECK(shown(page.browserButton(), page));
    CHECK(page.pathLabel()->text() == pm.findElement(pictureId)->file);
    CHECK(shown(page.pathLabel(), page));
    auto* form = page.findChild<QFormLayout*>();
    REQUIRE(form != nullptr);
    auto* caption = qobject_cast<QLabel*>(form->labelForField(page.pathLabel()));
    REQUIRE(caption != nullptr);
    CHECK(caption->text() == QStringLiteral("Copy:"));

    // Neither a file nor an address: a sentence, no buttons
    page.showMaterial(quoteId);
    CHECK(page.thumbnailProblem()->text() ==
          QStringLiteral("No file and no address. Its description and own fields tell where it "
                         "comes from."));
    CHECK_FALSE(shown(page.fileLabel(), page));
    for (QPushButton* button : {page.previewButton(), page.systemButton(), page.browserButton(),
                                page.copyAddressButton(), page.saveCopyButton()}) {
        CHECK_FALSE(shown(button, page));
    }
    CHECK_FALSE(shown(page.pathLabel(), page));
    CHECK_FALSE(shown(page.addressLabel(), page));

    // The full card on a tab has the icon of the source
    gui::MaterialCardPanel card(pageId);
    CHECK(card.elementId() == pageId);
    CHECK(card.page()->isFull());
    CHECK(card.property("elementId").toString() == pageId);
    CHECK(card.property("tabIconId").toString() == QStringLiteral("material.page"));
    gui::MaterialCardPanel quoteCard(quoteId);
    CHECK(quoteCard.property("tabIconId").toString() == QStringLiteral("material.noFile"));
    gui::MaterialCardPanel pictureCard(pictureId);
    CHECK(pictureCard.property("tabIconId").toString() == QStringLiteral("material.webPicture"));
}
