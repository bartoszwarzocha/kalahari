/// @file image_preview_panel.cpp
/// @brief The preview of the picture of a material, on a tab in the center of the window

#include "kalahari/gui/panels/image_preview_panel.h"

#include "kalahari/gui/material_file.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"

#include <QAction>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTransform>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

/// Two zooms closer than this are the same
constexpr double SAME_ZOOM = 0.001;

/// Room around a picture fitted to the window
constexpr int FIT_MARGIN = 8;

/// @p text with its key in brackets, as the tooltips of the program have it: "Zoom In (Ctrl++)"
QString withKey(const QString& text, const QKeySequence& key) {
    return QStringLiteral("%1 (%2)").arg(text, key.toString(QKeySequence::NativeText));
}

} // namespace

const QList<double>& ImagePreviewPanel::zoomSteps() {
    static const QList<double> steps = {0.1, 0.25, 0.33, 0.5, 0.67, 0.75, 1.0,
                                        1.25, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0};
    return steps;
}

ImagePreviewPanel::ImagePreviewPanel(const QString& elementId, QWidget* parent)
    : QWidget(parent), m_elementId(elementId) {
    setProperty("elementId", elementId);
    setProperty("tabIconId", QStringLiteral("common.image"));

    const auto& pm = core::ProjectManager::getInstance();
    if (const core::ProjectElement* element = pm.findElement(elementId)) {
        m_path = pm.filePathOf(*element);
        m_rotation = element->rotation;
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // The bar: the zoom, the turn and the negative, and the system program
    auto& art = core::ArtProvider::getInstance();
    m_toolBar = new QToolBar(this);
    m_toolBar->setObjectName(QStringLiteral("imagePreviewBar"));
    const auto addAction = [this, &art](const char* iconId, const QString& text,
                                        const QString& tooltip) {
        QAction* action = art.createAction(QString::fromLatin1(iconId), text, m_toolBar);
        action->setToolTip(tooltip);
        m_toolBar->addAction(action);
        return action;
    };
    //: In Polish: Dopasuj do okna
    m_fitAction = addAction("view.zoomWholePage", tr("Fit to Window"), tr("Fit to Window"));
    m_zoomOutAction = addAction("view.zoomOut", tr("Zoom Out"),
                                withKey(tr("Zoom Out"), QKeySequence(Qt::CTRL | Qt::Key_Minus)));
    m_zoomLabel = new QLabel(m_toolBar);
    m_zoomLabel->setObjectName(QStringLiteral("imagePreviewZoom"));
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setMinimumWidth(m_zoomLabel->fontMetrics().horizontalAdvance(
                                     QStringLiteral(" 800% ")));
    m_toolBar->addWidget(m_zoomLabel);
    m_zoomInAction = addAction("view.zoomIn", tr("Zoom In"),
                               withKey(tr("Zoom In"), QKeySequence(Qt::CTRL | Qt::Key_Plus)));
    //: In Polish: Powiększenie 100%
    m_zoomResetAction = addAction("view.resetZoom", tr("Zoom 100%"),
                                  withKey(tr("Zoom 100%"), QKeySequence(Qt::CTRL | Qt::Key_0)));
    m_toolBar->addSeparator();
    //: In Polish: Obróć w lewo
    m_rotateLeftAction = addAction("view.rotateLeft", tr("Rotate Left"),
                                   withKey(tr("Rotate Left"), QKeySequence(Qt::Key_L)));
    //: In Polish: Obróć w prawo
    m_rotateRightAction = addAction("view.rotateRight", tr("Rotate Right"),
                                    withKey(tr("Rotate Right"), QKeySequence(Qt::Key_R)));
    //: The colors of the picture turned to their opposites. In Polish: Negatyw
    m_negativeAction = addAction("view.negative", tr("Negative"),
                                 withKey(tr("Negative"), QKeySequence(Qt::Key_N)));
    m_negativeAction->setCheckable(true);
    m_toolBar->addSeparator();
    //: The button that opens the file in the program the system has for it. In Polish:
    //: Program systemowy
    m_systemAction = addAction("common.openExternal", tr("System Program"),
                               //: In Polish: Otwórz w programie systemowym
                               tr("Open in the System Program"));
    if (auto* button = qobject_cast<QToolButton*>(m_toolBar->widgetForAction(m_systemAction))) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    layout->addWidget(m_toolBar);

    connect(m_fitAction, &QAction::triggered, this, &ImagePreviewPanel::zoomToFit);
    connect(m_zoomOutAction, &QAction::triggered, this, &ImagePreviewPanel::zoomOut);
    connect(m_zoomInAction, &QAction::triggered, this, &ImagePreviewPanel::zoomIn);
    connect(m_zoomResetAction, &QAction::triggered, this, &ImagePreviewPanel::zoomReset);
    connect(m_rotateLeftAction, &QAction::triggered, this, &ImagePreviewPanel::rotateLeft);
    connect(m_rotateRightAction, &QAction::triggered, this, &ImagePreviewPanel::rotateRight);
    connect(m_negativeAction, &QAction::triggered, this, &ImagePreviewPanel::toggleNegative);
    connect(m_systemAction, &QAction::triggered, this,
            [this]() { emit openWithSystemRequested(m_elementId); });

    // The picture, or why there is none
    m_pages = new QStackedWidget(this);
    m_scene = new QGraphicsScene(this);
    m_item = m_scene->addPixmap(QPixmap());
    m_item->setTransformationMode(Qt::SmoothTransformation);
    m_view = new QGraphicsView(m_scene, m_pages);
    m_view->setObjectName(QStringLiteral("imagePreviewView"));
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setAlignment(Qt::AlignCenter);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform);
    //: The picture on the tab of its preview. In Polish: Podgląd obrazu: strzałki przesuwają,
    //: L i R obracają, N to negatyw
    m_view->setAccessibleName(
        tr("Picture preview: the arrows move it, L and R turn it, N shows its negative"));
    m_view->setFocusPolicy(Qt::StrongFocus);
    m_view->installEventFilter(this);
    m_view->viewport()->installEventFilter(this);
    m_pages->addWidget(m_view);

    auto* problemPage = new QWidget(m_pages);
    auto* problemLayout = new QVBoxLayout(problemPage);
    problemLayout->addStretch();
    m_problemLabel = new QLabel(problemPage);
    m_problemLabel->setObjectName(QStringLiteral("imagePreviewProblem"));
    m_problemLabel->setWordWrap(true);
    m_problemLabel->setAlignment(Qt::AlignCenter);
    m_problemLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    problemLayout->addWidget(m_problemLabel);
    //: In Polish: &Otwórz w programie systemowym
    m_problemButton = new QPushButton(tr("&Open in the System Program"), problemPage);
    m_problemButton->setObjectName(QStringLiteral("imagePreviewOpenSystem"));
    connect(m_problemButton, &QPushButton::clicked, this,
            [this]() { emit openWithSystemRequested(m_elementId); });
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    buttonRow->addWidget(m_problemButton);
    buttonRow->addStretch();
    problemLayout->addLayout(buttonRow);
    problemLayout->addStretch();
    m_pages->addWidget(problemPage);
    layout->addWidget(m_pages, 1);

    setFocusProxy(m_view);
    refreshIcons();
    connect(&art, &core::ArtProvider::resourcesChanged, this, &ImagePreviewPanel::refreshIcons);
    reload();
}

QString ImagePreviewPanel::problem() const {
    return m_picture.isNull() ? m_problem : QString();
}

QSize ImagePreviewPanel::pictureSize() const {
    QSize size = m_picture.size();
    if (m_rotation == 90 || m_rotation == 270) {
        size.transpose();
    }
    return size;
}

void ImagePreviewPanel::reload() {
    QImageReader reader(m_path);
    reader.setAutoTransform(true);
    m_picture = reader.read();
    m_problem.clear();
    if (m_picture.isNull()) {
        MaterialFile file = MaterialFile::of(m_path);
        if (file.readable) {
            file.unreadable(reader.errorString());
        }
        m_problem = file.problem();
        if (m_problem.isEmpty()) {
            //: A file of a material that is no picture. In Polish: Program nie pokazuje plików
            //: tego rodzaju. Otwórz plik w programie systemowym.
            m_problem = tr("The program does not show files of this kind. Open the file in "
                           "the system program.");
        }
        core::Logger::getInstance().info("ImagePreviewPanel: No picture of {}: {}",
                                         m_path.toStdString(), reader.errorString().toStdString());
        m_problemLabel->setText(m_problem);
        m_pages->setCurrentIndex(1);
    } else {
        m_pages->setCurrentIndex(0);
    }
    const bool shown = !m_picture.isNull();
    for (QAction* action : {m_fitAction, m_zoomOutAction, m_zoomInAction, m_zoomResetAction,
                            m_rotateLeftAction, m_rotateRightAction, m_negativeAction}) {
        action->setEnabled(shown);
    }
    showPicture();
}

void ImagePreviewPanel::showPicture() {
    if (m_picture.isNull()) {
        m_item->setPixmap(QPixmap());
        m_zoomLabel->clear();
        return;
    }
    QImage shown = m_rotation == 0 ? m_picture
                                   : m_picture.transformed(QTransform().rotate(m_rotation));
    if (m_negative) {
        shown.invertPixels();
    }
    m_item->setPixmap(QPixmap::fromImage(shown));
    m_scene->setSceneRect(m_item->boundingRect());
    applyZoom();
}

void ImagePreviewPanel::zoomIn() {
    const QList<double>& steps = zoomSteps();
    const auto next = std::find_if(steps.cbegin(), steps.cend(),
                                   [this](double step) { return step > m_zoom + SAME_ZOOM; });
    setZoom(next != steps.cend() ? *next : steps.last());
}

void ImagePreviewPanel::zoomOut() {
    const QList<double>& steps = zoomSteps();
    const auto next = std::find_if(steps.crbegin(), steps.crend(),
                                   [this](double step) { return step < m_zoom - SAME_ZOOM; });
    setZoom(next != steps.crend() ? *next : steps.first());
}

void ImagePreviewPanel::zoomReset() {
    setZoom(1.0);
}

void ImagePreviewPanel::zoomToFit() {
    m_fit = true;
    applyZoom();
}

void ImagePreviewPanel::rotateLeft() {
    turn(270);
}

void ImagePreviewPanel::rotateRight() {
    turn(90);
}

void ImagePreviewPanel::toggleNegative() {
    if (m_picture.isNull()) {
        return;
    }
    m_negative = !m_negative;
    m_negativeAction->setChecked(m_negative);
    showPicture();
}

void ImagePreviewPanel::turn(int degrees) {
    if (m_picture.isNull()) {
        return;
    }
    m_rotation = (m_rotation + degrees) % 360;
    showPicture();
    // The material remembers it; a book open read-only only shows it
    if (core::ProjectManager::getInstance().setRotation(m_elementId, m_rotation)) {
        emit rotationChanged(m_elementId);
    }
}

void ImagePreviewPanel::setZoom(double zoom) {
    m_fit = false;
    m_zoom = zoom;
    applyZoom();
}

void ImagePreviewPanel::applyZoom() {
    if (m_picture.isNull()) {
        return;
    }
    if (m_fit) {
        const QSize picture = pictureSize();
        const QSize room = m_view->viewport()->size() - QSize(FIT_MARGIN, FIT_MARGIN) * 2;
        double fitted = 1.0;
        if (picture.width() > 0 && picture.height() > 0 && room.width() > 0 &&
            room.height() > 0) {
            fitted = std::min({1.0, static_cast<double>(room.width()) / picture.width(),
                               static_cast<double>(room.height()) / picture.height()});
        }
        m_zoom = fitted;
    }
    m_view->setTransform(QTransform::fromScale(m_zoom, m_zoom));
    //: The zoom of a picture; %1: the percent. In Polish: %1%
    m_zoomLabel->setText(tr("%1%").arg(qRound(m_zoom * 100)));
    m_zoomInAction->setEnabled(m_zoom < zoomSteps().last() - SAME_ZOOM);
    m_zoomOutAction->setEnabled(m_zoom > zoomSteps().first() + SAME_ZOOM);
    emit zoomChanged(m_zoom);
}

void ImagePreviewPanel::refreshIcons() {
    // The actions of the bar refresh their own icons
    auto& art = core::ArtProvider::getInstance();
    const int iconSize = art.getIconSize(core::IconContext::Toolbar);
    m_toolBar->setIconSize(QSize(iconSize, iconSize));
    m_problemButton->setIcon(
        art.getIcon(QStringLiteral("common.openExternal"), core::IconContext::Button));
}

bool ImagePreviewPanel::eventFilter(QObject* watched, QEvent* event) {
    // A picture that fits the window fits it again at the size of the view: the view gets its
    // size after the tab, when it is first shown
    if (watched == m_view->viewport() && event->type() == QEvent::Resize && m_fit) {
        applyZoom();
    }
    // Ctrl with the wheel zooms; the wheel alone moves the picture
    if (watched == m_view->viewport() && event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            const int delta = wheel->angleDelta().y();
            if (delta > 0) {
                zoomIn();
            } else if (delta < 0) {
                zoomOut();
            }
            return true;
        }
    }
    // L and R turn the picture, N shows its negative; Ctrl+L and Ctrl+R align paragraphs
    if (watched == m_view && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers =
            key->modifiers() & ~(Qt::KeypadModifier | Qt::ShiftModifier);
        if (modifiers == Qt::NoModifier) {
            switch (key->key()) {
                case Qt::Key_L:
                    rotateLeft();
                    return true;
                case Qt::Key_R:
                    rotateRight();
                    return true;
                case Qt::Key_N:
                    toggleNegative();
                    return true;
                default:
                    break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace gui
} // namespace kalahari
