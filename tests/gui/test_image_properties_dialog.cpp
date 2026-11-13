/// @file test_image_properties_dialog.cpp
/// @brief Image > Image Properties...: the pages of the window, the look they give the image,
///        their keys and the one size of the window

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_images.h"
#include "kalahari/gui/dialogs/image_properties_dialog.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/widgets/color_menu.h"
#include "kalahari/gui/widgets/length_spin_box.h"

#include "document_window.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QShortcut>
#include <QSlider>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>

#include <map>

using namespace kalahari;
using namespace kalahari::editor;
using Catch::Approx;
using kalahari::gui::LengthSpinBox;
using kalahari::gui::dialogs::ImagePropertiesDialog;

namespace {

constexpr double POINTS_PER_CENTIMETER = 72.0 / 2.54;

/// The field a label names
template <typename Field>
Field* fieldOf(const QWidget& parent, const QString& label)
{
    for (QLabel* each : parent.findChildren<QLabel*>()) {
        if (each->text() == label) {
            return qobject_cast<Field*>(each->buddy());
        }
    }
    return nullptr;
}

/// The button with the text
template <typename Button>
Button* buttonOf(const QWidget& parent, const QString& text)
{
    for (Button* each : parent.findChildren<Button*>()) {
        if (each->text() == text) {
            return each;
        }
    }
    return nullptr;
}

/// The widget for a screen reader by its name
template <typename Widget>
Widget* namedWidget(const QWidget& parent, const QString& name)
{
    for (Widget* each : parent.findChildren<Widget*>()) {
        if (each->accessibleName() == name) {
            return each;
        }
    }
    return nullptr;
}

/// The access key of a text: the letter after a single "&"
QChar accessKey(const QString& text)
{
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            if (text.at(i + 1) == QLatin1Char('&')) {
                ++i;
                continue;
            }
            return text.at(i + 1).toLower();
        }
    }
    return QChar();
}

/// The texts with an access key that is not a Latin letter or digit (Alt does not reach "ś")
QString foreignKeys(const QWidget& root, const QStringList& more = {})
{
    QStringList foreign;
    const auto check = [&foreign](const QString& text) {
        const QChar key = accessKey(text);
        if (!key.isNull() && (key.unicode() >= 0x80 || !key.isLetterOrNumber())) {
            foreign << text;
        }
    };
    for (const QString& text : more) {
        check(text);
    }
    for (const QWidget* widget : root.findChildren<QWidget*>()) {
        if (const auto* label = qobject_cast<const QLabel*>(widget)) {
            check(label->text());
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            check(button->text());
        } else if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
            check(group->title());
        }
    }
    return foreign.join(QStringLiteral(" / "));
}

/// The texts of the widgets under @p root (not under @p skipped) with the same access key,
/// with @p more; empty when each key is the only one
QString sameKeys(const QWidget& root, const QWidget* skipped = nullptr,
                 const QStringList& more = {})
{
    std::map<QChar, QStringList> texts;
    const auto add = [&texts](const QString& text) {
        const QChar key = accessKey(text);
        if (!key.isNull()) {
            texts[key] << text;
        }
    };
    for (const QString& text : more) {
        add(text);
    }
    for (const QWidget* widget : root.findChildren<QWidget*>()) {
        bool inSkipped = false;
        for (const QWidget* up = widget; up != nullptr && up != &root; up = up->parentWidget()) {
            inSkipped = inSkipped || up == skipped;
        }
        if (inSkipped) {
            continue;
        }
        if (const auto* label = qobject_cast<const QLabel*>(widget); label && label->buddy()) {
            add(label->text());
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            add(button->text());
        } else if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
            add(group->title());
        }
    }
    QStringList same;
    for (const auto& [key, each] : texts) {
        if (each.size() > 1) {
            same << each.join(QStringLiteral(" / "));
        }
    }
    return same.join(QStringLiteral("; "));
}

/// 1366x768 at 150%: 911 pixels wide and 460 high (a Mac's larger controls need a few more)
void checkFitsSmallScreen(const QWidget& dialog)
{
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 480;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    CHECK(dialog.sizeHint().width() <= 911);
    CHECK(dialog.sizeHint().height() <= MAX_HEIGHT);
}

/// The windows in Polish while it lives: the translation the program ships
class Polish {
public:
    Polish()
    {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~Polish() { QCoreApplication::removeTranslator(&m_translator); }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

/// Let the layouts take in what changed
void settle()
{
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
    }
}

/// Press the key @p keys of the window, as the writer does
void press(QWidget& window, const QKeySequence& keys)
{
    for (QShortcut* shortcut : window.findChildren<QShortcut*>()) {
        if (shortcut->key() == keys) {
            emit shortcut->activated();
            return;
        }
    }
    FAIL("No key " << keys.toString().toStdString());
}

/// A picture of two halves: red on the left, blue on the right
QImage halves(const QSize& size)
{
    QImage picture(size, QImage::Format_ARGB32);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            picture.setPixelColor(x, y, x < size.width() / 2 ? QColor(Qt::red) : QColor(Qt::blue));
        }
    }
    return picture;
}

/// A book's folder of images with pictures in it
struct Pictures {
    QTemporaryDir dir;

    /// The path of a new picture in images/
    QString add(const QString& name, const QSize& size) const
    {
        QDir(dir.path()).mkpath(QStringLiteral("images"));
        const QString path = dir.path() + QStringLiteral("/images/") + name;
        REQUIRE(halves(size).save(path));
        return path;
    }
};

/// The data of an image of a picture of 800 x 400 pixels (600 x 300 points) in lines of
/// 450 points
ImagePropertiesData dataOf(const QString& path)
{
    ImagePropertiesData data;
    data.position = 0;
    data.image.source = QStringLiteral("images/") + QFileInfo(path).fileName();
    data.image.alt = QStringLiteral("The bridge");
    data.filePath = path;
    data.filePixels = QSize(800, 400);
    data.fileBytes = QFileInfo(path).size();
    data.lineWidth = 450.0;
    data.shownWidth = 450.0;
    return data;
}

/// The names of the pages, with their keys
QStringList pageNames(const ImagePropertiesDialog& dialog)
{
    QStringList names;
    for (int row = 0; row < dialog.pageList()->count(); ++row) {
        names << dialog.pageList()->item(row)->data(Qt::UserRole).toString();
    }
    return names;
}

/// Answer the window shown next: @p answer gets it, as the type expected
template <typename Dialog>
void answerNext(const std::function<void(Dialog&)>& answer)
{
    QTimer::singleShot(0, [answer]() {
        auto* dialog = qobject_cast<Dialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            if (QDialog* other = test::shownDialog()) {
                other->reject();
            }
            return;
        }
        answer(*dialog);
    });
}

}  // namespace

// =============================================================================
// The window
// =============================================================================

TEST_CASE("Image Properties window: its pages, their keys and one size on all of them",
          "[gui][images][dialog]") {
    const Pictures pictures;
    const ImagePropertiesData given = dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400}));
    ImagePropertiesDialog dialog(given, QColor(Qt::black));
    dialog.show();
    settle();
    CHECK(dialog.windowTitle() == QStringLiteral("Image Properties"));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("OK"));
    const QStringList pages = pageNames(dialog);
    CHECK(pages == QStringList{QStringLiteral("&Image"), QStringLiteral("&Size and Rotation"),
                               QStringLiteral("&Crop"), QStringLiteral("&Border"),
                               QStringLiteral("C&olors"), QStringLiteral("&Description"),
                               QStringLiteral("Ca&ption")});
    // The list shows them without the "&"
    CHECK(dialog.pageList()->item(1)->text() == QStringLiteral("Size and Rotation"));
    CHECK(dialog.currentPage() == ImagePropertiesDialog::ImagePage);

    // Nothing changed: the image as it was
    CHECK(dialog.data().image == given.image);
    CHECK(dialog.data().newFile.isEmpty());
    CHECK(dialog.data().position == given.position);

    // The file: its format, pixels and size
    QStringList texts;
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        texts << label->text();
    }
    CHECK(texts.join(QLatin1Char('\n')).contains(QStringLiteral("PNG · 800 × 400 pixels · ")));
    CHECK(texts.contains(QStringLiteral("File: images/bridge.png")));

    // Each page with keys of its own, with the names of the pages
    for (int page = 0; page < dialog.pages()->count(); ++page) {
        INFO("page " << page);
        CHECK(sameKeys(*dialog.pages()->widget(page), nullptr, pages).isEmpty());
    }
    CHECK(sameKeys(dialog, dialog.pages(), pages).isEmpty());
    CHECK(foreignKeys(dialog, pages).isEmpty());

    // One size on every page, the largest's; the page as its item and its key show it
    const QSize size = dialog.size();
    for (int page = 0; page < ImagePropertiesDialog::PageCount; ++page) {
        INFO("page " << page);
        dialog.showPage(static_cast<ImagePropertiesDialog::Page>(page));
        settle();
        CHECK(dialog.currentPage() == page);
        CHECK(dialog.pages()->currentIndex() == page);
        CHECK(dialog.size() == size);
        CHECK_FALSE(dialog.grab().isNull());
    }
    press(dialog, QKeySequence(QStringLiteral("Alt+C")));
    CHECK(dialog.currentPage() == ImagePropertiesDialog::CropPage);
    press(dialog, QKeySequence(Qt::CTRL | Qt::Key_Tab));
    CHECK(dialog.currentPage() == ImagePropertiesDialog::BorderPage);
    press(dialog, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    press(dialog, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    CHECK(dialog.currentPage() == ImagePropertiesDialog::SizePage);
    dialog.showPage(ImagePropertiesDialog::ImagePage);
    press(dialog, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    CHECK(dialog.currentPage() == ImagePropertiesDialog::CaptionPage);
    dialog.showPage(ImagePropertiesDialog::SizePage);
    press(dialog, QKeySequence(QStringLiteral("Alt+P")));
    CHECK(dialog.currentPage() == ImagePropertiesDialog::CaptionPage);
    checkFitsSmallScreen(dialog);
}

TEST_CASE("Image Properties window: a missing file", "[gui][images][dialog]") {
    ImagePropertiesData given;
    given.position = 4;
    given.image.source = QStringLiteral("images/gone.png");
    given.image.unit = ImageWidthUnit::Centimeters;
    given.image.width = 5.0;
    given.lineWidth = 450.0;
    given.shownWidth = 5.0 * POINTS_PER_CENTIMETER;
    ImagePropertiesDialog dialog(given, QColor());
    dialog.show();
    settle();
    QStringList texts;
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        texts << label->text();
    }
    CHECK(texts.contains(QStringLiteral("Image file missing")));
    QWidget& size = *dialog.pages()->widget(ImagePropertiesDialog::SizePage);
    CHECK_FALSE(buttonOf<QPushButton>(size, QStringLiteral("Original Si&ze"))->isEnabled());
    // Only a length: the original has no size
    auto* width = fieldOf<LengthSpinBox>(size, QStringLiteral("&Width"));
    REQUIRE(width != nullptr);
    CHECK(width->storedValue() == Approx(5.0));
    // Without a chooser and a saver, no buttons for them
    QWidget& image = *dialog.pages()->widget(ImagePropertiesDialog::ImagePage);
    CHECK(buttonOf<QPushButton>(image, QStringLiteral("C&hange Image..."))->isHidden());
    CHECK(buttonOf<QPushButton>(image, QStringLiteral("Sa&ve as Image..."))->isHidden());
    for (int page = 0; page < ImagePropertiesDialog::PageCount; ++page) {
        dialog.showPage(static_cast<ImagePropertiesDialog::Page>(page));
        CHECK_FALSE(dialog.grab().isNull());
    }
    CHECK(dialog.data().image == given.image);
}

// =============================================================================
// Image
// =============================================================================

TEST_CASE("Image Properties window: Change Image... and Save as Image...", "[gui][images][dialog]") {
    const Pictures pictures;
    const QString bridge = pictures.add(QStringLiteral("bridge.png"), {800, 400});
    const QString tower = pictures.add(QStringLiteral("tower.png"), {300, 600});
    ImagePropertiesData given = dataOf(bridge);
    given.image.unit = ImageWidthUnit::Centimeters;
    given.image.width = 10.0;
    given.image.crop = ImageCrop{10.0, 0.0, 0.0, 0.0};
    ImagePropertiesDialog dialog(given, QColor());
    QString chosen;
    int saved = 0;
    dialog.setFileChooser([&chosen]() { return chosen; });
    dialog.setImageSaver([&saved]() { ++saved; });
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::ImagePage);
    auto* change = buttonOf<QPushButton>(page, QStringLiteral("C&hange Image..."));
    auto* save = buttonOf<QPushButton>(page, QStringLiteral("Sa&ve as Image..."));
    auto* keep = buttonOf<QCheckBox>(page, QStringLiteral("&Keep the size and crop when the image is changed"));
    REQUIRE(change != nullptr);
    REQUIRE(save != nullptr);
    REQUIRE(keep != nullptr);
    CHECK_FALSE(change->isHidden());
    CHECK_FALSE(save->isHidden());
    CHECK(save->isEnabled());
    save->click();
    CHECK(saved == 1);

    // None chosen: nothing changes
    change->click();
    CHECK(dialog.data().newFile.isEmpty());

    SECTION("the size and the crop kept") {
        keep->setChecked(true);
        chosen = tower;
        change->click();
        const ImagePropertiesData data = dialog.data();
        CHECK(data.newFile == tower);
        CHECK(data.filePixels == QSize(300, 600));
        CHECK(data.image.width == Approx(10.0));
        CHECK(data.image.crop == given.image.crop);
        CHECK(data.keepSizeAndCrop);
        // The new file cannot be saved before it is the image's
        CHECK_FALSE(save->isEnabled());
        QStringList texts;
        for (const QLabel* label : page.findChildren<QLabel*>()) {
            texts << label->text();
        }
        CHECK(texts.contains(QStringLiteral("New file: tower.png (copied into the folder of images with OK)")));
    }

    SECTION("the file's own size, uncropped") {
        keep->setChecked(false);
        chosen = tower;
        change->click();
        const ImagePropertiesData data = dialog.data();
        CHECK(data.newFile == tower);
        CHECK(data.image.unit == ImageWidthUnit::Natural);
        CHECK(data.image.crop.isEmpty());
        keep->setChecked(true);
    }
}

// =============================================================================
// Size and Rotation
// =============================================================================

TEST_CASE("Image Properties window: the width in percent of the text, of the original, or a length",
          "[gui][images][dialog]") {
    const Pictures pictures;
    ImagePropertiesDialog dialog(dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400})), QColor());
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::SizePage);
    auto* unit = namedWidget<QComboBox>(page, QStringLiteral("Unit of the width"));
    auto* heightUnit = namedWidget<QComboBox>(page, QStringLiteral("Unit of the height"));
    auto* percent = fieldOf<QDoubleSpinBox>(page, QStringLiteral("&Width"));
    auto* keep = buttonOf<QCheckBox>(page, QStringLiteral("&Keep proportions"));
    auto* original = buttonOf<QPushButton>(page, QStringLiteral("Original Si&ze"));
    REQUIRE(unit != nullptr);
    REQUIRE(heightUnit != nullptr);
    REQUIRE(percent != nullptr);
    REQUIRE(keep != nullptr);
    REQUIRE(original != nullptr);
    // Its own size: 100% of the original
    CHECK(unit->currentIndex() == 1);
    CHECK(heightUnit->currentIndex() == 0);
    CHECK(percent->value() == Approx(100.0));
    CHECK(keep->isChecked());
    CHECK_FALSE(original->isEnabled());
    QStringList texts;
    for (const QLabel* label : page.findChildren<QLabel*>()) {
        texts << label->text();
    }
    CHECK(texts.contains(QStringLiteral("Scale: 100% of the original")));

    // Half of the original: a length, the height with it
    percent->setValue(50.0);
    CHECK(dialog.data().image.unit == ImageWidthUnit::Centimeters);
    CHECK(dialog.data().image.width * POINTS_PER_CENTIMETER == Approx(300.0));
    CHECK(dialog.data().image.height == 0.0);
    CHECK(original->isEnabled());
    // The text's width: the image keeps its width, now a percent of the text
    unit->setCurrentIndex(0);
    REQUIRE(fieldOf<QDoubleSpinBox>(page, QStringLiteral("&Width")) == percent);
    CHECK(dialog.data().image.unit == ImageWidthUnit::Percent);
    CHECK(dialog.data().image.width == Approx(300.0 / 450.0 * 100.0));
    percent->setValue(80.0);
    CHECK(dialog.data().image.width == Approx(80.0));
    CHECK(percent->maximum() == Approx(MAX_IMAGE_PERCENT));
    // A length
    unit->setCurrentIndex(2);
    auto* length = fieldOf<LengthSpinBox>(page, QStringLiteral("&Width"));
    REQUIRE(length != nullptr);
    CHECK(dialog.data().image.unit == ImageWidthUnit::Centimeters);
    CHECK(dialog.data().image.width * POINTS_PER_CENTIMETER == Approx(360.0));
    length->setValue(convertLength(10.0, gui::LengthUnit::Centimeters, length->displayUnit()));
    CHECK(dialog.data().image.width == Approx(10.0));

    // Original Size: the file's own size again
    original->click();
    CHECK(dialog.data().image.unit == ImageWidthUnit::Natural);
    CHECK(dialog.data().image.width == 0.0);
}

TEST_CASE("Image Properties window: an own height, and the proportions kept",
          "[gui][images][dialog]") {
    const Pictures pictures;
    ImagePropertiesData given = dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400}));
    given.image.unit = ImageWidthUnit::Centimeters;
    given.image.width = 10.0;
    ImagePropertiesDialog dialog(given, QColor());
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::SizePage);
    auto* width = fieldOf<LengthSpinBox>(page, QStringLiteral("&Width"));
    auto* heightUnit = namedWidget<QComboBox>(page, QStringLiteral("Unit of the height"));
    auto* keep = buttonOf<QCheckBox>(page, QStringLiteral("&Keep proportions"));
    REQUIRE(width != nullptr);
    REQUIRE(heightUnit != nullptr);
    REQUIRE(keep != nullptr);
    const auto setLength = [](LengthSpinBox& field, double centimeters) {
        field.setValue(convertLength(centimeters, gui::LengthUnit::Centimeters, field.displayUnit()));
    };

    // In proportion: the height of the picture follows the width (half of it)
    heightUnit->setCurrentIndex(1);
    auto* height = fieldOf<LengthSpinBox>(page, QStringLiteral("&Height"));
    REQUIRE(height != nullptr);
    CHECK(height->storedValue() == Approx(5.0));
    setLength(*height, 4.0);
    CHECK(dialog.data().image.width == Approx(8.0));
    CHECK(dialog.data().image.height == 0.0);

    // Without proportions: a height of its own, that stays when the width changes
    keep->setChecked(false);
    CHECK(dialog.data().image.height == Approx(4.0));
    setLength(*height, 3.0);
    CHECK(dialog.data().image.height == Approx(3.0));
    setLength(*width, 12.0);
    CHECK(dialog.data().image.width == Approx(12.0));
    CHECK(dialog.data().image.height == Approx(3.0));
    // In proportion again: the stretch stays, the height changes with the width
    keep->setChecked(true);
    setLength(*width, 6.0);
    CHECK(dialog.data().image.height == Approx(1.5));
    QStringList texts;
    for (const QLabel* label : page.findChildren<QLabel*>()) {
        texts << label->text();
    }
    // 6 cm of 600 points, 1.5 cm of 300 points
    const int byWidth = qRound(6.0 * POINTS_PER_CENTIMETER / 600.0 * 100.0);
    const int byHeight = qRound(1.5 * POINTS_PER_CENTIMETER / 300.0 * 100.0);
    CHECK(texts.contains(QStringLiteral("Scale: %1% × %2% of the original").arg(byWidth).arg(byHeight)));

    // A turn: the width becomes the height
    buttonOf<QPushButton>(page, QStringLiteral("Rotate &Right 90°"))->click();
    CHECK(dialog.data().image.rotation == 1);
    CHECK(dialog.data().image.width == Approx(1.5));
    CHECK(dialog.data().image.height == Approx(6.0));
    buttonOf<QPushButton>(page, QStringLiteral("Rotate &Left 90°"))->click();
    CHECK(dialog.data().image.rotation == 0);
    CHECK(dialog.data().image.width == Approx(6.0));
    CHECK(dialog.data().image.height == Approx(1.5));
}

TEST_CASE("Image Properties window: the angle and the mirror", "[gui][images][dialog]") {
    const Pictures pictures;
    ImagePropertiesDialog dialog(dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400})), QColor());
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::SizePage);
    auto* angle = fieldOf<QComboBox>(page, QStringLiteral("&Angle"));
    REQUIRE(angle != nullptr);
    REQUIRE(angle->count() == 4);
    CHECK(angle->itemText(1) == QStringLiteral("90°"));
    buttonOf<QPushButton>(page, QStringLiteral("Rotate &Left 90°"))->click();
    CHECK(dialog.data().image.rotation == 3);
    CHECK(angle->currentIndex() == 3);
    angle->setCurrentIndex(2);
    emit angle->activated(2);
    CHECK(dialog.data().image.rotation == 2);
    // Its own size turns with it: the original is 400 x 800 pixels on its side
    angle->setCurrentIndex(1);
    emit angle->activated(1);
    QStringList texts;
    for (const QLabel* label : page.findChildren<QLabel*>()) {
        texts << label->text();
    }
    CHECK(texts.join(QLatin1Char('\n')).contains(QStringLiteral("400 × 800 pixels = ")));
    buttonOf<QCheckBox>(page, QStringLiteral("Flip Horizo&ntally"))->setChecked(true);
    buttonOf<QCheckBox>(page, QStringLiteral("Flip &Vertically"))->setChecked(true);
    CHECK(dialog.data().image.flipHorizontal);
    CHECK(dialog.data().image.flipVertical);
    buttonOf<QCheckBox>(page, QStringLiteral("Flip Horizo&ntally"))->setChecked(false);
    CHECK_FALSE(dialog.data().image.flipHorizontal);
}

// =============================================================================
// Crop
// =============================================================================

TEST_CASE("Image Properties window: the crop of the sides as they are shown",
          "[gui][images][dialog]") {
    const Pictures pictures;
    ImagePropertiesData given = dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400}));
    given.image.unit = ImageWidthUnit::Centimeters;
    given.image.width = 10.0;
    ImagePropertiesDialog dialog(given, QColor());
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::CropPage);
    auto* left = fieldOf<QDoubleSpinBox>(page, QStringLiteral("&Left"));
    auto* top = fieldOf<QDoubleSpinBox>(page, QStringLiteral("&Top"));
    auto* aspect = fieldOf<QComboBox>(page, QStringLiteral("&Aspect ratio"));
    auto* keepSize = buttonOf<QRadioButton>(page, QStringLiteral("Keep the image's si&ze in the text"));
    auto* keepScale = buttonOf<QRadioButton>(page, QStringLiteral("Keep the scal&e (the image gets smaller)"));
    auto* remove = buttonOf<QPushButton>(page, QStringLiteral("Remo&ve Crop"));
    REQUIRE(left != nullptr);
    REQUIRE(top != nullptr);
    REQUIRE(aspect != nullptr);
    REQUIRE(keepSize != nullptr);
    REQUIRE(keepScale != nullptr);
    REQUIRE(remove != nullptr);
    // As the image is in the text: its size kept
    CHECK(keepSize->isChecked());
    CHECK_FALSE(remove->isEnabled());

    SECTION("the scale kept: the image smaller") {
        keepScale->setChecked(true);
        left->setValue(20.0);
        CHECK(dialog.data().image.crop == ImageCrop{20.0, 0.0, 0.0, 0.0});
        CHECK(dialog.data().image.width == Approx(8.0));
        CHECK(remove->isEnabled());
    }

    SECTION("the size kept: the rest of the picture larger") {
        left->setValue(20.0);
        CHECK(dialog.data().image.width == Approx(10.0));
        top->setValue(10.0);
        CHECK(dialog.data().image.crop == ImageCrop{20.0, 10.0, 0.0, 0.0});
        CHECK(dialog.data().image.width == Approx(10.0));
    }

    SECTION("Crop in the Text: the window closes with the changes, the marks in the text") {
        auto* inText = buttonOf<QPushButton>(page, QStringLiteral("Crop in the Te&xt"));
        REQUIRE(inText != nullptr);
        CHECK(inText->isHidden());
        dialog.setCropInTextAvailable(true);
        CHECK_FALSE(inText->isHidden());
        CHECK(inText->isEnabled());
        left->setValue(10.0);
        CHECK_FALSE(dialog.cropInTextAsked());
        inText->click();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK(dialog.cropInTextAsked());
        CHECK(dialog.data().image.crop.left == Approx(10.0));
    }

    SECTION("a turned and mirrored image: the side shown is another side of the file") {
        QWidget& size = *dialog.pages()->widget(ImagePropertiesDialog::SizePage);
        buttonOf<QPushButton>(size, QStringLiteral("Rotate &Right 90°"))->click();
        // The left as it is shown is the bottom of the file
        left->setValue(15.0);
        CHECK(dialog.data().image.crop == ImageCrop{0.0, 0.0, 0.0, 15.0});
        CHECK(left->value() == Approx(15.0));
    }

    SECTION("an aspect ratio: the largest part of it in the middle") {
        aspect->setCurrentIndex(2);  // 1 : 1
        emit aspect->activated(2);
        // 800 x 400: a quarter of the width off each side
        CHECK(dialog.data().image.crop == ImageCrop{25.0, 0.0, 25.0, 0.0});
        CHECK(aspect->currentIndex() == 2);
        // A side changed by hand: free
        top->setValue(5.0);
        CHECK(aspect->currentIndex() == 0);
        // As the original: no crop
        aspect->setCurrentIndex(1);
        emit aspect->activated(1);
        CHECK(dialog.data().image.crop.isEmpty());
        aspect->setCurrentIndex(5);  // 16 : 9
        emit aspect->activated(5);
        const ImageCrop crop = dialog.data().image.crop;
        // 2 : 1 is wider: the sides off
        CHECK(crop.top == 0.0);
        CHECK(crop.left == Approx((1.0 - 400.0 * 16.0 / 9.0 / 800.0) * 50.0).margin(0.01));
        CHECK(crop.right == Approx(crop.left));
        remove->click();
        CHECK(dialog.data().image.crop.isEmpty());
        CHECK(dialog.data().image.width == Approx(10.0));
    }
}

// =============================================================================
// Border
// =============================================================================

TEST_CASE("Image Properties window: the border's line, width, color, room and shadow",
          "[gui][images][dialog]") {
    const Pictures pictures;
    ImagePropertiesDialog dialog(dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400})),
                                 QColor(Qt::darkGreen));
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::BorderPage);
    auto* line = fieldOf<QComboBox>(page, QStringLiteral("&Line"));
    auto* width = fieldOf<QComboBox>(page, QStringLiteral("&Width"));
    auto* color = fieldOf<QPushButton>(page, QStringLiteral("Colo&r"));
    auto* padding = fieldOf<LengthSpinBox>(page, QStringLiteral("Space i&nside"));
    auto* shadow = buttonOf<QCheckBox>(page, QStringLiteral("S&hadow"));
    REQUIRE(line != nullptr);
    REQUIRE(width != nullptr);
    REQUIRE(color != nullptr);
    REQUIRE(padding != nullptr);
    REQUIRE(shadow != nullptr);
    CHECK(line->currentIndex() == 0);
    CHECK_FALSE(width->isEnabled());
    CHECK_FALSE(color->isEnabled());
    CHECK_FALSE(padding->isEnabled());
    CHECK(color->text() == QStringLiteral("Automatic"));
    CHECK(width->currentText() == QStringLiteral("0.5 pt"));

    line->setCurrentIndex(4);
    CHECK(dialog.data().image.borderLine == ImageBorderLine::Double);
    CHECK(width->isEnabled());
    CHECK(color->isEnabled());
    CHECK(padding->isEnabled());
    width->setCurrentIndex(width->findData(1.5));
    CHECK(dialog.data().image.borderWidth == Approx(1.5));
    auto* menu = qobject_cast<gui::ColorMenu*>(color->menu());
    REQUIRE(menu != nullptr);
    menu->choose(QColor(QStringLiteral("#336699")));
    CHECK(dialog.data().image.borderColor == QColor(QStringLiteral("#336699")));
    CHECK(color->text() != QStringLiteral("Automatic"));
    padding->setValue(convertLength(4.0, gui::LengthUnit::Points, padding->displayUnit()));
    // As the unit of the settings shows it
    CHECK(dialog.data().image.borderPadding == Approx(4.0).margin(0.1));
    shadow->setChecked(true);
    CHECK(dialog.data().image.shadow);
    menu->choose(QColor());
    CHECK_FALSE(dialog.data().image.borderColor.isValid());
    CHECK(color->text() == QStringLiteral("Automatic"));

    // A shadow alone: a frame with its room and color, no line
    line->setCurrentIndex(0);
    CHECK_FALSE(width->isEnabled());
    CHECK(color->isEnabled());
    CHECK(padding->isEnabled());
    CHECK_FALSE(dialog.grab().isNull());
}

TEST_CASE("Image Properties window: a border's width not in the list is added to it",
          "[gui][images][dialog]") {
    const Pictures pictures;
    ImagePropertiesData given = dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400}));
    given.image.borderLine = ImageBorderLine::Solid;
    given.image.borderWidth = 2.0;
    ImagePropertiesDialog dialog(given, QColor());
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::BorderPage);
    auto* width = fieldOf<QComboBox>(page, QStringLiteral("&Width"));
    REQUIRE(width != nullptr);
    CHECK(width->currentText() == QStringLiteral("2 pt"));
    CHECK(width->itemText(width->currentIndex() - 1) == QStringLiteral("1.5 pt"));
    CHECK(width->itemText(width->currentIndex() + 1) == QStringLiteral("2.25 pt"));
    CHECK(dialog.data().image == given.image);
}

// =============================================================================
// Colors
// =============================================================================

TEST_CASE("Image Properties window: the filter's tiles and the sliders", "[gui][images][dialog]") {
    const Pictures pictures;
    ImagePropertiesDialog dialog(dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400})), QColor());
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::ColorsPage);
    auto* original = buttonOf<QToolButton>(page, QStringLiteral("Origin&al"));
    auto* sepia = buttonOf<QToolButton>(page, QStringLiteral("S&epia"));
    auto* blackWhite = buttonOf<QToolButton>(page, QStringLiteral("Black and &White"));
    auto* brightness = fieldOf<QSlider>(page, QStringLiteral("B&rightness"));
    auto* contrast = fieldOf<QSlider>(page, QStringLiteral("Co&ntrast"));
    auto* threshold = fieldOf<QSlider>(page, QStringLiteral("&Threshold"));
    auto* transparency = fieldOf<QSlider>(page, QStringLiteral("Transparenc&y"));
    auto* reset = buttonOf<QPushButton>(page, QStringLiteral("Reset to Origina&l"));
    REQUIRE(original != nullptr);
    REQUIRE(sepia != nullptr);
    REQUIRE(blackWhite != nullptr);
    REQUIRE(brightness != nullptr);
    REQUIRE(contrast != nullptr);
    REQUIRE(threshold != nullptr);
    REQUIRE(transparency != nullptr);
    REQUIRE(reset != nullptr);
    CHECK(original->isChecked());
    // Each tile is a picture of its filter
    CHECK_FALSE(sepia->icon().isNull());
    CHECK_FALSE(threshold->isEnabled());

    sepia->click();
    CHECK(dialog.data().image.filter == ImageFilter::Sepia);
    CHECK(sepia->isChecked());
    CHECK_FALSE(original->isChecked());
    blackWhite->click();
    CHECK(dialog.data().image.filter == ImageFilter::BlackWhite);
    CHECK(threshold->isEnabled());
    threshold->setValue(60);
    brightness->setValue(10);
    contrast->setValue(-20);
    transparency->setValue(30);
    const ImageProperties image = dialog.data().image;
    CHECK(image.threshold == 60);
    CHECK(image.brightness == 10);
    CHECK(image.contrast == -20);
    CHECK(image.transparency == 30);
    QStringList texts;
    for (const QLabel* label : page.findChildren<QLabel*>()) {
        texts << label->text();
    }
    CHECK(texts.contains(QStringLiteral("+10%")));
    CHECK(texts.contains(QStringLiteral("-20%")));
    CHECK(texts.contains(QStringLiteral("60%")));
    CHECK(texts.contains(QStringLiteral("30%")));
    CHECK_FALSE(dialog.grab().isNull());

    reset->click();
    const ImageProperties plain = dialog.data().image;
    CHECK(plain.filter == ImageFilter::None);
    CHECK(plain.brightness == 0);
    CHECK(plain.contrast == 0);
    CHECK(plain.threshold == DEFAULT_IMAGE_THRESHOLD);
    CHECK(plain.transparency == 0);
    CHECK(original->isChecked());
    CHECK(brightness->value() == 0);
}

// =============================================================================
// Description
// =============================================================================

TEST_CASE("Image Properties window: the name, the alternative text and the description",
          "[gui][images][dialog]") {
    const Pictures pictures;
    ImagePropertiesData given = dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400}));
    given.image.name = QStringLiteral("Bridge");
    ImagePropertiesDialog dialog(given, QColor());
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::DescriptionPage);
    auto* name = fieldOf<QLineEdit>(page, QStringLiteral("&Name"));
    auto* alt = fieldOf<QLineEdit>(page, QStringLiteral("&Alternative text"));
    auto* description = fieldOf<QPlainTextEdit>(page, QStringLiteral("&Long description"));
    auto* decorative = buttonOf<QCheckBox>(page, QStringLiteral("Only decorati&ve – screen readers skip it"));
    REQUIRE(name != nullptr);
    REQUIRE(alt != nullptr);
    REQUIRE(description != nullptr);
    REQUIRE(decorative != nullptr);
    CHECK(name->text() == QStringLiteral("Bridge"));
    CHECK(alt->text() == QStringLiteral("The bridge"));
    CHECK(description->tabChangesFocus());

    name->setText(QStringLiteral("Bridge 1931 "));
    emit name->textEdited(name->text());
    alt->setText(QStringLiteral("A steel bridge"));
    emit alt->textEdited(alt->text());
    description->setPlainText(QStringLiteral("Three arches over the river."));
    ImageProperties image = dialog.data().image;
    CHECK(image.name == QStringLiteral("Bridge 1931"));
    CHECK(image.alt == QStringLiteral("A steel bridge"));
    CHECK(image.description == QStringLiteral("Three arches over the river."));

    // Decorative: a screen reader skips it, its texts are not read
    decorative->setChecked(true);
    CHECK(dialog.data().image.decorative);
    CHECK_FALSE(alt->isEnabled());
    CHECK_FALSE(description->isEnabled());
    CHECK(name->isEnabled());
    decorative->setChecked(false);
    CHECK(alt->isEnabled());
}

// =============================================================================
// Caption
// =============================================================================

TEST_CASE("Image Properties window: the caption, its label and number, below or above",
          "[gui][images][dialog][captions]") {
    const Pictures pictures;
    ImagePropertiesData given = dataOf(pictures.add(QStringLiteral("bridge.png"), {800, 400}));
    given.caption.label = QStringLiteral("Figure");
    given.captionNumber = QStringLiteral("3");
    ImagePropertiesDialog dialog(given, QColor());
    dialog.setCaptionLabels({QStringLiteral("Figure"), QStringLiteral("Map")},
                            [](const QString& label) {
                                return label == QLatin1String("Map") ? QStringLiteral("1")
                                                                     : QStringLiteral("3");
                            });
    QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::CaptionPage);
    auto* shown = buttonOf<QCheckBox>(page, QStringLiteral("S&how a caption"));
    auto* label = fieldOf<QComboBox>(page, QStringLiteral("&Label"));
    auto* text = fieldOf<QLineEdit>(page, QStringLiteral("&Text"));
    auto* above = buttonOf<QRadioButton>(page, QStringLiteral("&Above the image"));
    auto* below = buttonOf<QRadioButton>(page, QStringLiteral("B&elow the image"));
    auto* unnumbered =
        buttonOf<QCheckBox>(page, QStringLiteral("&Without a label and number – the text alone"));
    REQUIRE(shown != nullptr);
    REQUIRE(label != nullptr);
    REQUIRE(text != nullptr);
    REQUIRE(above != nullptr);
    REQUIRE(below != nullptr);
    REQUIRE(unnumbered != nullptr);
    const auto shows = [&page](const QString& wanted) {
        for (const QLabel* each : page.findChildren<QLabel*>()) {
            if (each->text() == wanted) {
                return true;
            }
        }
        return false;
    };

    // No caption yet: the fields wait for it
    CHECK_FALSE(shown->isChecked());
    CHECK_FALSE(label->isEnabled());
    CHECK_FALSE(text->isEnabled());
    CHECK_FALSE(dialog.data().caption.shown);

    // Shown: the label of the image first, with the number the program gives
    shown->setChecked(true);
    CHECK(label->isEnabled());
    CHECK(label->currentText() == QStringLiteral("Figure"));
    CHECK(shows(QStringLiteral("the program gives the number: 3")));
    CHECK(below->isChecked());
    text->setText(QStringLiteral("The bridge "));
    emit text->textEdited(text->text());
    CHECK(dialog.data().caption ==
          ImageCaption{true, true, QStringLiteral("Figure"), QStringLiteral("The bridge"), false});

    // Another label: its number; above the image
    label->setCurrentIndex(1);
    CHECK(shows(QStringLiteral("the program gives the number: 1")));
    above->setChecked(true);
    CHECK(dialog.data().caption ==
          ImageCaption{true, true, QStringLiteral("Map"), QStringLiteral("The bridge"), true});

    // The text alone: without it, no caption
    unnumbered->setChecked(true);
    CHECK_FALSE(label->isEnabled());
    CHECK_FALSE(shows(QStringLiteral("the program gives the number: 1")));
    CHECK(dialog.data().caption.shown);
    CHECK_FALSE(dialog.data().caption.numbered);
    text->setText(QString());
    emit text->textEdited(text->text());
    CHECK_FALSE(dialog.data().caption.shown);

    // New Label...: a label of its own, chosen
    unnumbered->setChecked(false);
    answerNext<gui::dialogs::TextInputDialog>([](gui::dialogs::TextInputDialog& input) {
        input.field()->setText(QStringLiteral("Chart"));
        input.acceptButton()->click();
    });
    buttonOf<QPushButton>(page, QStringLiteral("&New Label..."))->click();
    CHECK(dialog.captionLabels() ==
          QStringList{QStringLiteral("Figure"), QStringLiteral("Map"), QStringLiteral("Chart")});
    CHECK(label->currentText() == QStringLiteral("Chart"));
    CHECK(dialog.data().caption.label == QStringLiteral("Chart"));
}

TEST_CASE("Editor panel: Image Properties... gives the image its look and caption as one step",
          "[gui][images][dialog][captions]") {
    const Pictures pictures;
    pictures.add(QStringLiteral("bridge.png"), {800, 400});
    gui::EditorPanel panel;
    panel.setImagesFolder(pictures.dir.path());
    BookEditor& editor = *panel.getBookEditor();
    const QString kml = QStringLiteral(
        "<kml><p align=\"center\"><img src=\"images/bridge.png\"/></p><p>Text</p></kml>");
    editor.fromKml(kml);

    answerNext<ImagePropertiesDialog>([](ImagePropertiesDialog& dialog) {
        QWidget& border = *dialog.pages()->widget(ImagePropertiesDialog::BorderPage);
        fieldOf<QComboBox>(border, QStringLiteral("&Line"))->setCurrentIndex(1);
        QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::CaptionPage);
        buttonOf<QCheckBox>(page, QStringLiteral("S&how a caption"))->setChecked(true);
        auto* text = fieldOf<QLineEdit>(page, QStringLiteral("&Text"));
        text->setText(QStringLiteral("The bridge"));
        emit text->textEdited(text->text());
        dialog.acceptButton()->click();
    });
    panel.showImagePropertiesDialog(0, ImagePropertiesDialog::CaptionPage);
    const std::optional<ImageCaption> caption = editor.imageCaptionOf(0);
    REQUIRE(caption.has_value());
    CHECK(caption->shown);
    CHECK(caption->numbered);
    CHECK(caption->text == QStringLiteral("The bridge"));
    CHECK(editor.toKml().contains(QStringLiteral("border=\"solid\"")));
    CHECK(editor.captionEntries().size() == 1);

    // One step of Undo: the image as it was, without its caption
    editor.undo();
    CHECK(editor.toKml() == kml);
    CHECK_FALSE(editor.canUndo());

    // Only the caption taken away
    editor.redo();
    answerNext<ImagePropertiesDialog>([](ImagePropertiesDialog& dialog) {
        QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::CaptionPage);
        buttonOf<QCheckBox>(page, QStringLiteral("S&how a caption"))->setChecked(false);
        dialog.acceptButton()->click();
    });
    panel.showImagePropertiesDialog(0, ImagePropertiesDialog::CaptionPage);
    CHECK_FALSE(editor.imageCaptionOf(0)->shown);
    CHECK(editor.captionEntries().empty());
    editor.undo();
    CHECK(editor.imageCaptionOf(0)->shown);
}

// =============================================================================
// In Polish
// =============================================================================

TEST_CASE("Image Properties window in Polish: keys of its own, Latin letters, a small screen",
          "[gui][images][dialog]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    const Pictures pictures;
    ImagePropertiesDialog dialog(dataOf(pictures.add(QStringLiteral("most.png"), {800, 400})), QColor());
    dialog.setFileChooser([]() { return QString(); });
    dialog.setImageSaver([]() {});
    dialog.show();
    settle();
    CHECK(dialog.windowTitle() == QStringLiteral("Właściwości obrazu"));
    const QStringList pages = pageNames(dialog);
    CHECK(pages == QStringList{QStringLiteral("&Obraz"), QStringLiteral("&Rozmiar i obrót"),
                               QStringLiteral("&Przycięcie"), QStringLiteral("Obramo&wanie"),
                               QStringLiteral("&Kolory"), QStringLiteral("Op&is"),
                               QStringLiteral("Po&dpis")});
    for (int page = 0; page < dialog.pages()->count(); ++page) {
        INFO("page " << page);
        CHECK(sameKeys(*dialog.pages()->widget(page), nullptr, pages).isEmpty());
    }
    CHECK(sameKeys(dialog, dialog.pages(), pages).isEmpty());
    CHECK(foreignKeys(dialog, pages).isEmpty());
    const QSize size = dialog.size();
    for (int page = 0; page < ImagePropertiesDialog::PageCount; ++page) {
        INFO("page " << page);
        dialog.showPage(static_cast<ImagePropertiesDialog::Page>(page));
        settle();
        CHECK(dialog.size() == size);
    }
    press(dialog, QKeySequence(QStringLiteral("Alt+W")));
    CHECK(dialog.currentPage() == ImagePropertiesDialog::BorderPage);
    press(dialog, QKeySequence(QStringLiteral("Alt+D")));
    CHECK(dialog.currentPage() == ImagePropertiesDialog::CaptionPage);
    checkFitsSmallScreen(dialog);
}

// =============================================================================
// The editor panel shows it
// =============================================================================

TEST_CASE("Editor panel: Image Properties... gives the image its look as one step of Undo",
          "[gui][images][dialog]") {
    const Pictures pictures;
    pictures.add(QStringLiteral("bridge.png"), {800, 400});
    gui::EditorPanel panel;
    panel.setImagesFolder(pictures.dir.path());
    BookEditor& editor = *panel.getBookEditor();
    editor.fromKml(QStringLiteral("<kml><p><img src=\"images/bridge.png\" alt=\"The bridge\"/></p></kml>"));
    const std::string before = editor.toKml().toStdString();

    // The page asked for first; its fields change the image
    answerNext<ImagePropertiesDialog>([](ImagePropertiesDialog& dialog) {
        CHECK(dialog.currentPage() == ImagePropertiesDialog::BorderPage);
        QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::BorderPage);
        fieldOf<QComboBox>(page, QStringLiteral("&Line"))->setCurrentIndex(1);
        QWidget& size = *dialog.pages()->widget(ImagePropertiesDialog::SizePage);
        buttonOf<QPushButton>(size, QStringLiteral("Rotate &Right 90°"))->click();
        // A saved book: another picture can be chosen
        QWidget& image = *dialog.pages()->widget(ImagePropertiesDialog::ImagePage);
        CHECK_FALSE(buttonOf<QPushButton>(image, QStringLiteral("C&hange Image..."))->isHidden());
        dialog.acceptButton()->click();
    });
    panel.showImagePropertiesDialog(0, ImagePropertiesDialog::BorderPage);
    CHECK(editor.toKml().toStdString() ==
          "<kml><p><img src=\"images/bridge.png\" alt=\"The bridge\" rotate=\"90\" "
          "border=\"solid\"/></p></kml>");
    editor.undo();
    CHECK(editor.toKml().toStdString() == before);

    // Cancel, and OK with nothing changed: no step of Undo
    answerNext<ImagePropertiesDialog>([](ImagePropertiesDialog& dialog) {
        QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::BorderPage);
        fieldOf<QComboBox>(page, QStringLiteral("&Line"))->setCurrentIndex(1);
        dialog.reject();
    });
    panel.showImagePropertiesDialog(0);
    CHECK(editor.toKml().toStdString() == before);
    answerNext<ImagePropertiesDialog>([](ImagePropertiesDialog& dialog) { dialog.acceptButton()->click(); });
    panel.showImagePropertiesDialog(0);
    CHECK(editor.toKml().toStdString() == before);
    CHECK_FALSE(editor.canUndo());

    // Crop in the Text: the changes made, the marks on the image in the text
    answerNext<ImagePropertiesDialog>([](ImagePropertiesDialog& dialog) {
        QWidget& page = *dialog.pages()->widget(ImagePropertiesDialog::CropPage);
        QPushButton* crop = buttonOf<QPushButton>(page, QStringLiteral("Crop in the Te&xt"));
        REQUIRE(crop != nullptr);
        CHECK_FALSE(crop->isHidden());
        QWidget& border = *dialog.pages()->widget(ImagePropertiesDialog::BorderPage);
        fieldOf<QComboBox>(border, QStringLiteral("&Line"))->setCurrentIndex(1);
        crop->click();
    });
    panel.showImagePropertiesDialog(0, ImagePropertiesDialog::CropPage);
    CHECK(editor.toKml().toStdString() ==
          "<kml><p><img src=\"images/bridge.png\" alt=\"The bridge\" border=\"solid\"/></p></kml>");
    REQUIRE(editor.imageCropInProgress());
    editor.finishImageCrop(false);
    editor.undo();

    // No image there: no window
    editor.fromKml(QStringLiteral("<kml><p>Text</p></kml>"));
    panel.showImagePropertiesDialog(0);
    CHECK(test::shownDialog() == nullptr);
}
