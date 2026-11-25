// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file image_properties_dialog.h
/// @brief Image > Image Properties...: the file, size, crop, the text flowing around it, the
///        room around it, border, colors, description and caption of the selected image

#ifndef KALAHARI_GUI_DIALOGS_IMAGE_PROPERTIES_DIALOG_H
#define KALAHARI_GUI_DIALOGS_IMAGE_PROPERTIES_DIALOG_H

#include "kalahari/editor/text_images.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QFrame>
#include <QImage>

#include <array>
#include <functional>
#include <QStringList>

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

namespace kalahari::gui {

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

/// @brief A picture of how the text flows around an image: lines of text and the image as a
///        box, with the room around it (the page Text Flow, its choices and the page Space
///        Around); the colors of the window's palette
class WrapPicture : public QFrame {
    Q_OBJECT

public:
    /// @param size The room the picture takes
    /// @param parent Parent widget
    explicit WrapPicture(const QSize& size, QWidget* parent = nullptr);

    /// @brief Draw the text flowing so around the image, with this room around it (points)
    void setWrap(editor::ImageWrap wrap, double above, double below, double beside);

    /// @brief The picture as a pixmap of @p size (the icons of the choices)
    static QPixmap pixmap(editor::ImageWrap wrap, const QSize& size, const QPalette& palette,
                          qreal devicePixelRatio);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    /// @brief Draw the picture in @p rect: the room in points shown in proportion to its width
    static void draw(QPainter& painter, const QRectF& rect, editor::ImageWrap wrap, double above,
                     double below, double beside, const QPalette& palette);

    QSize m_size;
    editor::ImageWrap m_wrap = editor::ImageWrap::Inline;
    double m_above = editor::DEFAULT_IMAGE_SPACE_ABOVE;
    double m_below = editor::DEFAULT_IMAGE_SPACE_BELOW;
    double m_beside = editor::DEFAULT_IMAGE_SPACE_BESIDE;
};

/// @brief Image > Image Properties...: the look of the selected image in pages, chosen in a
///        list on the left: Image (its file), Size and Rotation, Crop, Text Flow (how the text
///        flows around it, each way with a picture of it), Space Around (the room it keeps
///        from the text), Border, Colors, Description and Caption
///
/// The window has the size of its largest page on every page. Each page has an access key
/// (Alt with its letter), and Ctrl+Tab and Ctrl+Shift+Tab go to the next and the previous
/// page. The changes are made with OK, as one step of Undo; the file stays as it is. The gear
/// opens Settings > Editor > Images.
class ImagePropertiesDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The pages, in the order of the list
    enum Page {
        ImagePage,
        SizePage,
        CropPage,
        WrapPage,
        SpacePage,
        BorderPage,
        ColorsPage,
        DescriptionPage,
        CaptionPage,
        PageCount
    };

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

    /// @brief The labels the page Caption offers, and the number the image's caption would have
    ///        with each ("3", "2.3"); without them the label of its caption alone
    void setCaptionLabels(const QStringList& labels,
                          std::function<QString(const QString& label)> numberOf);

    /// @brief The labels of the page Caption, with those New Label... added
    [[nodiscard]] QStringList captionLabels() const { return m_captionLabels; }

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
    QWidget* createWrapPage();
    QWidget* createSpacePage();
    QWidget* createBorderPage();
    QWidget* createColorsPage();
    QWidget* createDescriptionPage();
    QWidget* createCaptionPage();

    /// @brief The fields, the previews and the texts follow the image's properties
    void refresh();
    void refreshImagePage();
    void refreshSizePage();
    void refreshCropPage();
    void refreshWrapPage();
    void refreshSpacePage();
    void refreshBorderPage();
    void refreshColorsPage();
    void refreshCaptionPage();

    /// @brief New Label... of the page Caption
    void addCaptionLabel();

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
    std::array<ImagePreview*, 5> m_previews{};  ///< Of the pages Image, Crop, Border, Colors,
                                                ///< Caption

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

    // Text Flow: a choice of each way (ImageWrap), with its picture
    QButtonGroup* m_wraps = nullptr;
    QLabel* m_wrapNote = nullptr;  ///< What the way chosen does, or why only one is offered

    // Space Around: above, below and beside, with the picture of the room
    std::array<LengthSpinBox*, 3> m_spaces{};
    QPushButton* m_spacesOfSettings = nullptr;
    QLabel* m_spaceNote = nullptr;
    WrapPicture* m_spacePicture = nullptr;

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
    // Caption
    editor::ImageCaption m_caption;  ///< As the fields set it
    QStringList m_captionLabels;
    std::function<QString(const QString& label)> m_captionNumberOf;
    QCheckBox* m_captionShown = nullptr;
    QComboBox* m_captionLabel = nullptr;
    QPushButton* m_newCaptionLabel = nullptr;
    QLabel* m_captionNumber = nullptr;
    QLineEdit* m_captionText = nullptr;
    QRadioButton* m_captionBelow = nullptr;
    QRadioButton* m_captionAbove = nullptr;
    QCheckBox* m_captionUnnumbered = nullptr;
    QLabel* m_captionPreviewAbove = nullptr;
    QLabel* m_captionPreviewBelow = nullptr;
};

} // namespace dialogs
} // namespace kalahari::gui

#endif  // KALAHARI_GUI_DIALOGS_IMAGE_PROPERTIES_DIALOG_H
