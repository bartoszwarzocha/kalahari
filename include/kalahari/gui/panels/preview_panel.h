// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file preview_panel.h
/// @brief The preview of the file of a material on a tab in the center of the window: what the
/// window asks of a preview of a picture and of a PDF file alike

#ifndef KALAHARI_GUI_PANELS_PREVIEW_PANEL_H
#define KALAHARI_GUI_PANELS_PREVIEW_PANEL_H

#include <QString>
#include <QWidget>

namespace kalahari::gui {

/// @brief The preview of the file of a material, on a tab beside the chapters
///
/// The window gives it the commands of View > Zoom and Edit > Find and shows its zoom and its
/// page in the status bar; the Navigator opens it and answers what it asks for. The tab has
/// the properties "elementId" and "tabIconId", as an editor's tab has them.
class PreviewPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief The preview of material @p elementId
    explicit PreviewPanel(const QString& elementId, QWidget* parent = nullptr);

    /// @brief The material it shows
    [[nodiscard]] QString elementId() const { return m_elementId; }

    /// @brief Zoom: 1 is 100%
    [[nodiscard]] virtual double zoom() const = 0;

    /// @brief The page shown, for the status bar: "Page 3 of 212"; empty: it has no pages
    [[nodiscard]] virtual QString pageStatus() const;

    /// @brief Whether it has a search of its own for Edit > Find (find())
    [[nodiscard]] virtual bool canFind() const;

    /// @brief Show page @p page of its file, as the file or the writer numbers it ("45",
    /// "xii")
    /// @return false when it has no such page
    virtual bool showPage(const QString& page);

    /// @brief Show the material again after it changed elsewhere (its bookmarks on its card)
    virtual void refresh();

public slots:
    /// @brief The next larger zoom
    virtual void zoomIn() = 0;

    /// @brief The next smaller zoom
    virtual void zoomOut() = 0;

    /// @brief 100%
    virtual void zoomReset() = 0;

    /// @brief The width of the page in the window
    virtual void zoomToPageWidth() = 0;

    /// @brief The whole page in the window
    virtual void zoomToWholePage() = 0;

    /// @brief Edit > Find: its search gets the keys (canFind())
    virtual void find();

signals:
    /// @brief The zoom changed: the status bar can show it
    void zoomChanged(double zoom);

    /// @brief Another page is shown: the status bar can show it
    void pageChanged();

    /// @brief The writer asked to open the file in the program the system gives it
    void openWithSystemRequested(const QString& elementId);

    /// @brief The writer asked for the card of material @p elementId
    void openCardRequested(const QString& elementId);

    /// @brief The writer asked for the settings of the preview (Settings > Materials)
    void settingsRequested();

    /// @brief The writer asked to make a quote of @p text from page @p page of source
    /// @p sourceId
    void newQuoteRequested(const QString& sourceId, const QString& text, const QString& page);

    /// @brief Material @p elementId changed here (its bookmarks): its cards can show it
    void materialChanged(const QString& elementId);

private:
    QString m_elementId;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_PREVIEW_PANEL_H
