/// @file file_trash.h
/// @brief Files to the Trash of the system (the Recycle Bin on Windows)
///
/// A file in the Trash can be brought back in the system's file manager. Not every folder has
/// a Trash: a network drive or a USB stick on Windows has none, so a file deleted there is
/// gone for good. The program asks canMoveToTrash() before it tells the writer where the
/// deleted files go.

#pragma once

#include <QString>

#include <functional>

namespace kalahari::core {

/// @brief Whether files in @p path (a file or a folder) can go to the Trash of the system
///
/// - Windows: a fixed drive has a Recycle Bin; a network drive, a removable drive (a USB stick)
///   or a disc has none.
/// - macOS: a local volume has a Trash; a network volume has none.
/// - Linux and other systems (the freedesktop.org Trash): the drive of the home folder has
///   the user's Trash ($XDG_DATA_HOME/Trash; the program makes a missing folder of the user's
///   data); another drive has one when the program can use or make its Trash folder
///   (.Trash/<user id> or .Trash-<user id> at the top of the drive).
///
/// The Trash can still refuse a file, e.g. when the writer switched it off for the drive;
/// moveToTrash() then fails and the file stays.
/// @return false when @p path does not exist
bool canMoveToTrash(const QString& path);

/// @brief Move file @p path to the Trash of the system
/// @return true when the file is gone from its folder to the Trash; false when it stays
///         where it was
bool moveToTrash(const QString& path);

/// @brief A Trash in place of the system's, for tests: no file of a test lands in the
/// writer's Trash, and a test can have a drive without a Trash or a Trash that refuses files
///
/// Empty functions give the system's Trash back.
/// @param canMove Answers canMoveToTrash() for a path that exists
/// @param move Does moveToTrash() for a file that exists
void setTrashForTests(std::function<bool(const QString&)> canMove,
                      std::function<bool(const QString&)> move);

}  // namespace kalahari::core
