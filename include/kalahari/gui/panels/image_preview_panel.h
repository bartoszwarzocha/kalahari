/// @file image_preview_panel.h
/// @brief The preview of the picture of a material, on a tab in the center of the window

#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

class QAction;
class QGraphicsPixmapItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QPushButton;
class QStackedWidget;
class QToolBar;

namespace kalahari {
namespace gui {

/// @brief The preview of the picture of a material, on a tab beside the chapters
///
/// The bar over the picture fits it to the window, zooms it out and in (also with the commands
/// of View > Zoom and Ctrl with the mouse wheel), shows it at 100%, turns it by a right angle
/// left and right (L, R), shows its negative (N) and opens the file in the program the system
/// gives it. Dragging with the mouse and the arrows move the picture. The turn is saved with
/// the material (core::ProjectManager::setRotation()); the zoom and the negative are the
/// tab's own. The file itself never changes.
///
/// A file the program cannot show (a format without its image plugin, a damaged picture, a
/// file missing from the book's folder) gets a sentence saying why, and the button that opens
/// it in the system program.
///
/// The tab has the properties "elementId" and "tabIconId", as an editor's tab has them.
class ImagePreviewPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief The zooms of Zoom In and Zoom Out, from the smallest
    static const QList<double>& zoomSteps();

    /// @brief Create the preview of material @p elementId of the open book
    explicit ImagePreviewPanel(const QString& elementId, QWidget* parent = nullptr);

    /// @brief The material it shows
    QString elementId() const { return m_elementId; }

    /// @brief Whether it shows a picture; false with the sentence of why it does not
    bool hasPicture() const { return !m_picture.isNull(); }

    /// @brief The sentence of why it shows no picture; empty when it shows one
    QString problem() const;

    /// @brief Zoom: 1 is 100%
    double zoom() const { return m_zoom; }

    /// @brief Whether the picture fits the window, also when the window changes its size
    bool fitsWindow() const { return m_fit; }

    /// @brief Turn of the picture, clockwise: 0, 90, 180 or 270 degrees
    int rotation() const { return m_rotation; }

    /// @brief Whether it shows the negative
    bool isNegative() const { return m_negative; }

    /// @brief The size of the picture as it is shown, turned, at 100%
    QSize pictureSize() const;

    /// @brief The view of the picture, which has the keys
    QGraphicsView* view() const { return m_view; }

    /// @brief The label of the zoom on the bar: "75%"
    QLabel* zoomLabel() const { return m_zoomLabel; }

public slots:
    /// @brief Read the file again and show it, keeping the zoom and the negative
    void reload();

    /// @brief The next larger zoom of zoomSteps()
    void zoomIn();

    /// @brief The next smaller zoom of zoomSteps()
    void zoomOut();

    /// @brief 100%
    void zoomReset();

    /// @brief The whole picture in the window, at 100% at most
    void zoomToFit();

    /// @brief Turn by 90 degrees to the left and save the turn with the material
    void rotateLeft();

    /// @brief Turn by 90 degrees to the right and save the turn with the material
    void rotateRight();

    /// @brief Show the negative, or the picture again
    void toggleNegative();

signals:
    /// @brief The writer asked to open the file in the program the system gives it
    void openWithSystemRequested(const QString& elementId);

    /// @brief The zoom changed: the status bar can show it
    void zoomChanged(double zoom);

    /// @brief The turn changed and was saved with material @p elementId: its card and the
    /// lists of materials can show it
    void rotationChanged(const QString& elementId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Turn by @p degrees more, show it and save the turn
    void turn(int degrees);

    /// @brief Show the picture as read, turned and maybe as a negative
    void showPicture();

    /// @brief Set the zoom; the picture no longer fits the window by itself
    void setZoom(double zoom);

    /// @brief Apply the zoom to the view and the label, and the state of the buttons
    void applyZoom();

    /// @brief Refresh the icons of the bar after a change of theme or icons
    void refreshIcons();

    QString m_elementId;
    QString m_path;          ///< The file of the material
    QImage m_picture;        ///< The picture as read, not turned
    QString m_problem;       ///< Why there is no picture
    double m_zoom = 1.0;
    bool m_fit = true;
    int m_rotation = 0;
    bool m_negative = false;

    QToolBar* m_toolBar = nullptr;
    QAction* m_fitAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomResetAction = nullptr;
    QAction* m_rotateLeftAction = nullptr;
    QAction* m_rotateRightAction = nullptr;
    QAction* m_negativeAction = nullptr;
    QAction* m_systemAction = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QStackedWidget* m_pages = nullptr;
    QGraphicsScene* m_scene = nullptr;
    QGraphicsView* m_view = nullptr;
    QGraphicsPixmapItem* m_item = nullptr;
    QLabel* m_problemLabel = nullptr;
    QPushButton* m_problemButton = nullptr;
};

} // namespace gui
} // namespace kalahari
