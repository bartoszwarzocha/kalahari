// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file file_associations_mac.mm
/// @brief The program of the files of Kalahari on macOS
///
/// The types are exported by Info.plist of Kalahari.app (packaging/macos/Info.plist.in), with
/// the program as their owner; Launch Services knows them once the system has seen the bundle.
/// NSWorkspace (macOS 12 and later) tells which application opens a type and sets Kalahari as
/// its default; the system may ask the writer first, and calls back when it is done.

#include "file_associations_mac.h"
#include "kalahari/core/logger.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QString>

#include <cstddef>
#include <memory>
#include <utility>

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace kalahari::gui {

namespace {

/// The text of a system older than macOS 12
QString needsMacOS12()
{
    //: In Polish: Program potrzebuje systemu macOS 12 lub nowszego.
    return QCoreApplication::translate("kalahari::gui::FileAssociations",
                                       "The program needs macOS 12 or later.");
}

class MacFileAssociations final : public FileAssociationSystem {
public:
    FileAssociation current(const KalahariFileType& type) const override
    {
        if (@available(macOS 12.0, *)) {
            UTType* content = [UTType exportedTypeWithIdentifier:type.contentType.toNSString()];
            NSURL* application =
                [[NSWorkspace sharedWorkspace] URLForApplicationToOpenContentType:content];
            if (application == nil) {
                return {FileAssociation::State::NoProgram, {}};
            }
            NSString* identifier = [[NSBundle bundleWithURL:application] bundleIdentifier];
            if (identifier != nil &&
                [identifier isEqualToString:[[NSBundle mainBundle] bundleIdentifier]]) {
                return {FileAssociation::State::Kalahari, {}};
            }
            NSString* name =
                [[NSFileManager defaultManager] displayNameAtPath:[application path]];
            return {FileAssociation::State::OtherProgram, QString::fromNSString(name)};
        }
        return {FileAssociation::State::Unknown, needsMacOS12()};
    }

    void associate(std::function<void(const QString&)> done) override
    {
        if (@available(macOS 12.0, *)) {
            // The answers of the system come on a queue of its own: the last one, on the thread
            // of the window, calls done with the first error
            struct Pending {
                std::size_t left = 0;
                QString problem;
                std::function<void(const QString&)> done;
            };
            auto pending = std::make_shared<Pending>();
            pending->left = kalahariFileTypes().size();
            pending->done = std::move(done);
            NSURL* bundle = [[NSBundle mainBundle] bundleURL];
            for (const KalahariFileType& type : kalahariFileTypes()) {
                UTType* content =
                    [UTType exportedTypeWithIdentifier:type.contentType.toNSString()];
                const QString name = type.contentType;
                [[NSWorkspace sharedWorkspace]
                    setDefaultApplicationAtURL:bundle
                             toOpenContentType:content
                             completionHandler:^(NSError* error) {
                               const QString problem =
                                   error == nil ? QString()
                                                : QString::fromNSString(
                                                      [error localizedDescription]);
                               QMetaObject::invokeMethod(
                                   QCoreApplication::instance(),
                                   [pending, problem, name]() {
                                       if (!problem.isEmpty()) {
                                           core::Logger::getInstance().warn(
                                               "File associations: {} not set: {}",
                                               name.toStdString(), problem.toStdString());
                                           if (pending->problem.isEmpty()) {
                                               pending->problem = problem;
                                           }
                                       }
                                       if (--pending->left == 0) {
                                           pending->done(pending->problem);
                                       }
                                   },
                                   Qt::QueuedConnection);
                             }];
            }
            return;
        }
        done(needsMacOS12());
    }

    bool hasSettingsWindow() const override { return false; }

    bool openSettingsWindow() override { return false; }
};

}  // namespace

std::unique_ptr<FileAssociationSystem> macFileAssociationSystem()
{
    return std::make_unique<MacFileAssociations>();
}

} // namespace kalahari::gui
