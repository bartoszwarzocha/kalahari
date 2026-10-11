/// @file progress_dialog.cpp
/// @brief The program's own window for the progress of a long task

#include "kalahari/gui/dialogs/progress_dialog.h"

#include <QCoreApplication>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

ProgressDialog::ProgressDialog(const QString& title, const QString& text, QWidget* parent)
    : KalahariDialog(parent)
{
    setHeading(title);
    setCompactHeading(true);
    setWindowModality(Qt::WindowModal);

    m_textLabel = new QLabel(text, this);
    m_textLabel->setTextFormat(Qt::PlainText);
    m_textLabel->setWordWrap(true);
    contentLayout()->addWidget(m_textLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setAccessibleName(title);
    m_textLabel->setBuddy(m_progressBar);
    contentLayout()->addWidget(m_progressBar);

    // Only Cancel: the dialog closes by itself when the task is done
    acceptButton()->hide();
    cancelButton()->setDefault(true);
    cancelButton()->setFocus();
}

void ProgressDialog::setText(const QString& text)
{
    m_textLabel->setText(text);
}

void ProgressDialog::setRange(int minimum, int maximum)
{
    m_progressBar->setRange(minimum, maximum);
}

void ProgressDialog::setValue(int value)
{
    m_progressBar->setValue(value);
    // The task runs in the window's thread: let the dialog draw itself and see Cancel
    if (isVisible()) {
        QCoreApplication::processEvents();
    }
}

int ProgressDialog::value() const
{
    return m_progressBar->value();
}

void ProgressDialog::setCancelVisible(bool visible)
{
    cancelButton()->setVisible(visible);
}

void ProgressDialog::reject()
{
    // A task that cannot stop has no Cancel, and Esc does not close its dialog either
    if (cancelButton()->isHidden()) {
        return;
    }
    m_canceled = true;
    KalahariDialog::reject();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
