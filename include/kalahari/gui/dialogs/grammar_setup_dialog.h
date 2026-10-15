/// @file grammar_setup_dialog.h
/// @brief The window of the preparation of grammar checking: what is downloaded, how it goes,
///        that it is ready, or why it stopped

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

#include <array>

class QGridLayout;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QToolButton;

namespace kalahari::gui {
class GrammarSetup;
}

namespace kalahari::gui::dialogs {

/// @brief The window of the preparation of grammar checking (GrammarSetup), in three steps
///
/// 1. What is downloaded, how much, where to, and whether there is room for it;
///    "Download and Prepare" starts.
/// 2. The steps of the preparation as they go. "Download in Background" closes the window
///    and the preparation goes on (the status bar shows it); so do Esc and the window's
///    close button. Only "Cancel" breaks it off.
/// 3. Ready: what the underlines look like and where the settings are.
///
/// When the preparation stops, the window says why, what to do and the details, with
/// "Try Again". The window can be closed and shown again at any time: it shows what the
/// preparation is doing.
class GrammarSetupDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the window shows
    enum class Page {
        Plan,      ///< What is downloaded
        Progress,  ///< The steps as they go
        Done,      ///< Ready
        Failed     ///< Why the preparation stopped
    };

    /// @brief Create the window for a preparation, at the page of what it is doing
    /// @param setup The preparation (not owned)
    /// @param parent Parent widget
    explicit GrammarSetupDialog(GrammarSetup* setup, QWidget* parent = nullptr);

    /// @brief What the window shows
    Page page() const { return m_page; }

    /// @brief Why the preparation stopped and what to do (Page::Failed)
    QString message() const;

    /// @brief The details of the failure: what the system said, the address, how much was
    ///        downloaded (Page::Failed)
    QString details() const;

    /// @brief The button that breaks the preparation off (Page::Progress)
    QPushButton* stopButton() const { return m_stopButton; }

    /// @brief The button that checks the room on the disk again (Page::Plan)
    QPushButton* checkAgainButton() const { return m_checkAgainButton; }

    /// @brief The button that copies the failure (Page::Failed)
    QPushButton* copyButton() const { return m_copyButton; }

public slots:
    /// @brief The button of the page: start, download in the background, finish or try again
    void accept() override;

private:
    /// @brief One step of the preparation, as the window shows it
    struct StepRow {
        QLabel* icon{nullptr};
        QLabel* title{nullptr};
        QLabel* value{nullptr};
        QProgressBar* bar{nullptr};
    };

    /// @brief The page of what is downloaded
    QWidget* createPlanPage();

    /// @brief The page of the steps (as they go, or the one that failed)
    QWidget* createStepsPage();

    /// @brief The page of the preparation done
    QWidget* createDonePage();

    /// @brief Start the preparation, or go on after a failure
    void start();

    /// @brief Show the page of what the preparation is doing
    void showState();

    /// @brief Show a page, with its heading and buttons
    void showPage(Page page);

    /// @brief What is downloaded and whether there is room for it
    void updatePlan();

    /// @brief The steps and their progress
    void updateSteps();

    /// @brief The preparation ended: the page of how it ended
    void onFinished();

    /// @brief Put the failure on the clipboard
    void copyFailure();

    /// @brief An icon for a line of the window (ArtProvider id; empty: room for one)
    void setIcon(QLabel* label, const QString& iconId) const;

    GrammarSetup* m_setup;
    Page m_page{Page::Plan};
    QStackedWidget* m_pages{nullptr};

    // What is downloaded
    QLabel* m_languageToolSize{nullptr};
    QLabel* m_javaSize{nullptr};
    QLabel* m_totalSize{nullptr};
    QLabel* m_roomIcon{nullptr};
    QLabel* m_room{nullptr};
    QPushButton* m_checkAgainButton{nullptr};
    QLabel* m_folderIcon{nullptr};
    QLabel* m_folder{nullptr};

    // The steps
    QLabel* m_failureIcon{nullptr};
    QLabel* m_failure{nullptr};
    std::array<StepRow, 4> m_steps;
    QLabel* m_backgroundNote{nullptr};
    QLabel* m_cancelNote{nullptr};
    QLabel* m_hint{nullptr};
    QToolButton* m_detailsToggle{nullptr};
    QPlainTextEdit* m_details{nullptr};

    QPushButton* m_stopButton{nullptr};
    QPushButton* m_copyButton{nullptr};
};

}  // namespace kalahari::gui::dialogs
