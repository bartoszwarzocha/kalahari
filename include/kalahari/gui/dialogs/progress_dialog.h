/// @file progress_dialog.h
/// @brief The program's own window for the progress of a long task

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

class QLabel;
class QProgressBar;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief The progress of a long task, in place of QProgressDialog
///
/// The heading names the task, the content says what is being done and shows a progress
/// bar; Cancel (or Esc) asks the task to stop: the task reads wasCanceled(). The dialog
/// is window-modal, so the window under it waits while the task runs.
/// @code
/// ProgressDialog progress(tr("Export"), tr("Exporting the project archive..."), this);
/// progress.show();
/// for (int i = 0; i < count && !progress.wasCanceled(); ++i) { ...; progress.setValue(i * 100 / count); }
/// @endcode
class ProgressDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Create the dialog, with a range of 0-100
    /// @param title Title of the dialog, also its window title
    /// @param text What is being done
    /// @param parent Parent widget
    ProgressDialog(const QString& title, const QString& text, QWidget* parent = nullptr);

    /// @brief Set what is being done
    void setText(const QString& text);

    /// @brief Set the range of setValue() (by default 0-100)
    void setRange(int minimum, int maximum);

    /// @brief Show the progress and let the window draw it
    /// @param value Between the minimum and the maximum of the range
    void setValue(int value);

    /// @brief The current progress
    int value() const;

    /// @brief Show or hide Cancel, for a task that cannot stop (by default it is shown)
    void setCancelVisible(bool visible);

    /// @brief Cancel or Esc was pressed
    bool wasCanceled() const { return m_canceled; }

    /// @brief Cancel or Esc: remember it for the task and close the dialog
    void reject() override;

private:
    QLabel* m_textLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    bool m_canceled = false;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
