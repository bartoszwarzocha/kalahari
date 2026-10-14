/// @file program_instance.h
/// @brief One program for the writer: a later start hands its request to the program that
/// runs, and ends

#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QLocalServer;
class QLocalSocket;
class QLockFile;

namespace kalahari {
namespace gui {

/// @brief One program for the writer
///
/// The first start of the program holds a lock and listens on a channel named after the
/// writer and the program's file. A later start finds the lock, hands its request to the
/// program that runs (to show its window and open a book or a file) and ends. So a book is
/// never opened by two copies of the program, and every request reaches the window that runs.
/// A request names what is to be done, so the program can later open a book in a window of
/// its own.
///
/// A program file of another place (another build) has a channel of its own.
///
/// @code
/// ProgramInstance instance;
/// if (instance.handOver(path)) {
///     return 0;  // the program that runs took the request
/// }
/// instance.listen();
/// QObject::connect(&instance, &ProgramInstance::requested, &window, &MainWindow::onLaterStart);
/// @endcode
class ProgramInstance : public QObject {
    Q_OBJECT

public:
    /// @brief The channel @p key; the default one is the same for every start of this program
    /// file by this writer
    explicit ProgramInstance(const QString& key = defaultKey(), QObject* parent = nullptr);
    ~ProgramInstance() override;

    ProgramInstance(const ProgramInstance&) = delete;
    ProgramInstance& operator=(const ProgramInstance&) = delete;

    /// @brief Hand the request to show the window and open @p path (empty: nothing to open)
    /// to the program that runs
    /// @param timeoutMs How long to wait for the program that runs to take it: it may still be
    ///        starting. When it ends meanwhile, this start is the first one at once.
    /// @return true when the program that runs took the request: this start ends; false when
    ///         no program runs, or it did not answer: this start runs on its own
    bool handOver(const QString& path, int timeoutMs = 20000);

    /// @brief Listen for the requests of later starts, after handOver() found no program that
    /// runs
    /// @return false when this start does not hold the lock or the channel cannot be opened:
    ///         the program then runs on its own
    bool listen();

    /// @brief Whether this start listens for the requests of later starts
    bool isListening() const;

    /// @brief The channel of this program file and this writer
    static QString defaultKey();

signals:
    /// @brief A later start asks to show the window and to open @p path (empty: nothing)
    void requested(const QString& path);

private:
    /// @brief Read the request of a later start, answer it and pass it on
    void readRequest(QLocalSocket* socket);

    QString m_key;
    std::unique_ptr<QLockFile> m_lock;  ///< Held by the start that listens
    QLocalServer* m_server = nullptr;
};

} // namespace gui
} // namespace kalahari
