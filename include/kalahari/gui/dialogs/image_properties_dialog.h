/// @file image_properties_dialog.h
/// @brief Image > Image Properties...: the file, size, crop, border, colors and description
///        of the selected image

#pragma once

#include "kalahari/editor/text_images.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QFrame>
#include <QImage>

#include <array>
#include <functional>

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSlider;
class QStackedWidget;
class QToolButton;

namespace kalahari {
namespace gui {

class ColorMenu;
class LengthSpinBox;

namespace dialogs {

/// @brief The picture of an image as the window shows it: with its look and frame, or
///        whole with its cut off parts shaded (the Crop page)
class ImagePreview : public QFrame {
    Q_OBJECT

public:
    /// @param size The room the preview takes
    /// @param parent Parent widget
    explicit ImagePreview(const QSize& size, QWidget* parent = nullptr);

    /// @brief The file's whole picture (made small) and the color of an automatic border
    void setPicture(const QImage& picture, const QColor& automaticColor);

    /// @brief The look the picture is shown with
    /// @param image The image's properties
    /// @param shape The size of its picture in points, for its shape (an own height
    ///        stretches it); empty: the shape of the file
    void setImage(const editor::ImageProperties& image, const QSizeF& shape = QSizeF());

    /// @brief Show the picture turned and mirrored but whole, its crop shaded
    void setShowsCrop(bool shows);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QSize m_size;
    QImage m_picture;
    QColor m_automaticColor;
    editor::ImageProperties m_image;
    QSizeF m_shape;
    bool m_showsCrop = false;
};

/// @brief Image > Image Properties...: the look of the selected image in pages, chosen in a
///        list on the left: Image (its file), Size and Rotation, Crop, Border, Colors and
///        Description
///
/// The window has the size of its largest page on every page. Each page has an access key
/// (Alt with its letter), and Ctrl+Tab and Ctrl+Shift+Tab go to the next and the previous
/// page. The changes are made with OK, as one step of Undo; the file stays as it is.
class ImagePropertiesDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The pages, in the order of the list
    enum Page { ImagePage, SizePage, CropPage, BorderPage, ColorsPage, DescriptionPage, PageCount };

    /// @param data The image now (BookEditor::imagePropertiesData())
    /// @param automaticColor The color of the text, the color of an automatic border
    /// @param parent Parent widget
    ImagePropertiesDialog(const editor::ImagePropertiesData& data, const QColor& automaticColor,
                          QWidget* parent = nullptr);

    /// @brief The properties chosen
    [[nodiscard]] editor::ImagePropertiesData data() const;

    /// @brief Choose another file for the image (Change Image...): the window of the system,
    ///        giving its path; empty: none chosen. Without it the button is hidden.
    void setFileChooser(std::function<QString()> chooser);

    /// @brief Save the image's file elsewhere (Save as Image...). Without it the button is
    ///        hidden.
    void setImageSaver(std::function<void()> saver);

    /// @brief Offer Crop in the Text on the page Crop: the window closes as with OK and the
    ///        marks of the crop show on the image in the text (cropInTextAsked()). Without
    ///        it the button is hidden.
    void setCropInTextAvailable(bool available);

    /// @brief Whether the window closed with Crop in the Text
    [[nodiscard]] bool cropInTextAsked() const { return m_cropInTextAsked; }

    /// @brief Show a page, as its item in the list does
    void showPage(Page page);

    /// @brief The page shown
    [[nodiscard]] Page currentPage() const;

    /// @brief The list of the pages and the pages (for the tests)
    [[nodiscard]] QListWidget* pageList() const { return m_pageList; }
    [[nodiscard]] QStackedWidget* pages() const { return m_pages; }

private:
    QWidget* createImagePage();
    QWidget* createSizePage();
    QWidget* createCropPage();
    QWidget* createBorderPage();
    QWidget* createColorsPage();
    QWidget* createDescriptionPage();

    /// @brief The fields, the previews and the texts follow the image's properties
    void refresh();
    void refreshImagePage();
    void refreshSizePage();
    void refreshCropPage();
    void refreshBorderPage();
    void refreshColorsPage();

    /// @brief The image's size in points as it is set (before the layout fits it in the
    ///        line): its picture's width and height
    [[nodiscard]] QSizeF pointSize() const;

    /// @brief The size of the picture of its file in points: its pixels at 96 an inch,
    ///        cropped and turned; empty without a file
    [[nodiscard]] QSizeF originalSize() const;

    /// @brief Set the width of the image's picture in points (in the unit of its width),
    ///        its height with it when the proportions are kept
    void setPointWidth(double points);

    /// @brief Set the height of the image's picture in points: in proportion, its width
    ///        follows; otherwise an own height
    void setPointHeight(double points);

    /// @brief Set the crop of the sides as the image is shown; with the scale kept, the
    ///        image's size follows
    void setShownCrop(const editor::ImageCrop& crop);

    /// @brief A turn by quarters; an own height turns with the picture
    void turn(int quarters);

    /// @brief The page's access keys: Alt with the letter of each page
    void addPageKeys();

    editor::ImagePropertiesData m_data;
    editor::ImageProperties m_image;  ///< The properties as the fields set them
    QImage m_picture;                 ///< The file's picture, made small for the previews
    QColor m_automaticColor;
    std::function<QString()> m_fileChooser;
    std::function<void()> m_imageSaver;
    bool m_updating = false;  ///< The program sets the fields: they change nothing
    bool m_cropInTextAsked = false;

    QListWidget* m_pageList = nullptr;
    QStackedWidget* m_pages = nullptr;
    std::array<ImagePreview*, 4> m_previews{};  ///< Of the pages Image, Crop, Border, Colors

    // Image
    QLabel* m_fileInfo = nullptr;
    QLabel* m_fileName = nullptr;
    QPushButton* m_changeButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QCheckBox* m_keepSizeAndCrop = nullptr;

    // Size and Rotation
    QDoubleSpinBox* m_widthPercent = nullptr;
    LengthSpinBox* m_widthLength = nullptr;
    QComboBox* m_widthUnit = nullptr;
    QDoubleSpinBox* m_heightPercent = nullptr;
    LengthSpinBox* m_heightLength = nullptr;
    QComboBox* m_heightUnit = nullptr;
    QCheckBox* m_keepProportions = nullptr;
    QLabel* m_scale = nullptr;
    QPushButton* m_originalButton = nullptr;
    QLabel* m_originalInfo = nullptr;
    QComboBox* m_angle = nullptr;
    QCheckBox* m_flipHorizontal = nullptr;
    QCheckBox* m_flipVertical = nullptr;

    // Crop
    std::array<QDoubleSpinBox*, 4> m_cropSides{};  ///< Left, right, top, bottom as shown
    QComboBox* m_aspect = nullptr;
    QRadioButton* m_keepSize = nullptr;
    QRadioButton* m_keepScale = nullptr;
    QPushButton* m_cropInText = nullptr;
    QPushButton* m_removeCrop = nullptr;

    // Border
    QComboBox* m_line = nullptr;
    QComboBox* m_lineWidth = nullptr;
    QPushButton* m_colorButton = nullptr;
    ColorMenu* m_colorMenu = nullptr;
    LengthSpinBox* m_padding = nullptr;
    QCheckBox* m_shadow = nullptr;

    // Colors
    QButtonGroup* m_filters = nullptr;
    QSlider* m_brightness = nullptr;
    QSlider* m_contrast = nullptr;
    QSlider* m_threshold = nullptr;
    QSlider* m_transparency = nullptr;
    std::array<QLabel*, 4> m_sliderValues{};

    // Description
    QLineEdit* m_name = nullptr;
    QLineEdit* m_alt = nullptr;
    QPlainTextEdit* m_description = nullptr;
    QCheckBox* m_decorative = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
