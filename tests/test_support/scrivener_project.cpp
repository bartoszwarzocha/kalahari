// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file scrivener_project.cpp
/// @brief A project of Scrivener the tests of its import read

#include "test_support/scrivener_project.h"

#include <catch2/catch_test_macros.hpp>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>

namespace kalahari::test {

namespace {

const QByteArray BINDER = R"XML(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<ScrivenerProject Template="No" Version="2.0" Identifier="1A" Creator="SCRMAC-3.3.1-15651">
  <Binder>
    <BinderItem UUID="DRAFT" Type="DraftFolder">
      <Title>Manuscript</Title>
      <Children>
        <BinderItem UUID="P1" Type="Folder">
          <Title>Część pierwsza</Title>
          <MetaData><LabelID>1</LabelID></MetaData>
          <Children>
            <BinderItem UUID="C1" Type="Folder">
              <Title>Rozdział 1</Title>
              <Children>
                <BinderItem UUID="S1" Type="Text">
                  <Title>Scena 1</Title>
                  <MetaData><StatusID>2</StatusID><LabelID>0</LabelID></MetaData>
                  <Keywords><KeywordID>0</KeywordID><KeywordID>1</KeywordID></Keywords>
                </BinderItem>
                <BinderItem UUID="S2" Type="Text">
                  <Title>Scena 2</Title>
                  <MetaData><StatusID>1</StatusID></MetaData>
                </BinderItem>
                <BinderItem UUID="S3" Type="Text">
                  <Title>Wycięta</Title>
                  <MetaData><IncludeInCompile>No</IncludeInCompile></MetaData>
                </BinderItem>
              </Children>
            </BinderItem>
            <BinderItem UUID="C2" Type="Text">
              <Title>Rozdział 2</Title>
              <MetaData><StatusID>3</StatusID></MetaData>
            </BinderItem>
          </Children>
        </BinderItem>
        <BinderItem UUID="E1" Type="Text"><Title>Epilog</Title></BinderItem>
        <BinderItem UUID="F0" Type="Folder"><Title>Pusty</Title></BinderItem>
      </Children>
    </BinderItem>
    <BinderItem UUID="RES" Type="ResearchFolder">
      <Title>Research</Title>
      <Children>
        <BinderItem UUID="IMG" Type="Image"><Title>Mapa</Title></BinderItem>
        <BinderItem UUID="R1" Type="Text"><Title>Notatki o oazie</Title></BinderItem>
      </Children>
    </BinderItem>
    <BinderItem UUID="CH" Type="Folder">
      <Title>Characters</Title>
      <Children><BinderItem UUID="K1" Type="Text"><Title>Anna</Title></BinderItem></Children>
    </BinderItem>
    <BinderItem UUID="TR" Type="TrashFolder">
      <Title>Trash</Title>
      <Children><BinderItem UUID="T1" Type="Text"><Title>Stary</Title></BinderItem></Children>
    </BinderItem>
  </Binder>
  <LabelSettings>
    <Title>Label</Title>
    <Labels><Label ID="-1">No Label</Label><Label ID="0" Color="0.9 0.2 0.2">Red</Label><Label ID="1">Blue</Label></Labels>
  </LabelSettings>
  <StatusSettings>
    <Title>Status</Title>
    <StatusItems>
      <Status ID="-1">No Status</Status><Status ID="0">To Do</Status><Status ID="1">First Draft</Status>
      <Status ID="2">Revised Draft</Status><Status ID="3">Final Draft</Status><Status ID="4">Done</Status>
    </StatusItems>
  </StatusSettings>
  <Keywords>
    <Keyword ID="0"><Title>pustynia</Title><Color>0 0 0</Color></Keyword>
    <Keyword ID="1"><Title>noc</Title></Keyword>
  </Keywords>
  <ProjectProperties><ProjectTitle>Karawana</ProjectTitle><FullName>Jan Kowalski</FullName></ProjectProperties>
</ScrivenerProject>
)XML";

/// A text of Cocoa's RTF of one paragraph
QByteArray cocoaText(const QByteArray& text) {
    return R"RTF({\rtf1\ansi\ansicpg1252\cocoartf2639
{\fonttbl\f0\fnil\fcharset0 Palatino-Roman;}
\pard\f0\fs24 \cf0 \uc0 )RTF" +
           text + "}";
}

QByteArray pngBytes() {
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

void write(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(data);
}

}  // namespace

QString makeScrivenerProject(const QString& folder) {
    const QDir project(QDir(folder).filePath(QStringLiteral("Karawana.scriv")));
    const auto data = [&project](const char* id, const char* name) {
        return project.filePath(QStringLiteral("Files/Data/%1/%2").arg(QLatin1String(id), QLatin1String(name)));
    };
    write(project.filePath(QStringLiteral("Karawana.scrivx")), BINDER);
    write(data("P1", "content.rtf"), cocoaText(R"(Wst\u281 p do cz\u281 \u347 ci.)"));
    write(data("C1", "synopsis.txt"), QStringLiteral("Rozdział o wyjeździe.\n").toUtf8());
    write(data("S1", "content.rtf"),
          cocoaText(R"(Pierwsza {\field{\*\fldinst{HYPERLINK "scrivcmt://CM1"}}{\fldrslt scena}}{\field{\*\fldinst{HYPERLINK "scrivcmt://FN1"}}{\fldrslt .}})"));
    write(data("S1", "content.comments"), R"XML(<?xml version="1.0" encoding="UTF-8"?>
<Comments>
  <Comment ID="CM1" Color="1 1 0"><![CDATA[{\rtf1\ansi\ansicpg1250 Sprawd\'9f dat\'ea.}]]></Comment>
  <Comment ID="FN1" Footnote="Yes"><![CDATA[{\rtf1\ansi Przypis sceny.}]]></Comment>
</Comments>
)XML");
    write(data("S1", "synopsis.txt"), QStringLiteral("Karawana wyrusza.").toUtf8());
    write(data("S1", "notes.rtf"), cocoaText(R"(Notatka do sceny.\
Druga linia.)"));
    write(data("S2", "content.rtf"),
          R"RTF({\rtf1\ansi\ansicpg1250{\stylesheet{\s0 Normal;}{\s1\b heading 1;}}\pard\s1 Podtytu\'b3\par\pard Druga scena.\par})RTF");
    write(data("S3", "content.rtf"), cocoaText("Wyci\\u281 ty tekst."));
    write(data("C2", "content.rtf"), cocoaText("Tekst rozdzia\\u322 u drugiego."));
    const QDir snapshots(project.filePath(QStringLiteral("Snapshots/C2.snapshots")));
    write(snapshots.filePath(QStringLiteral("index.xml")), R"XML(<?xml version="1.0" encoding="UTF-8"?>
<Snapshots Version="1.0">
  <Snapshot><Title>Po redakcji</Title><Date>2025-10-28 10:00:00 -0700</Date></Snapshot>
  <Snapshot><Title>Pierwsza wersja</Title><Date>2025-10-27 02:20:40 -0700</Date></Snapshot>
</Snapshots>
)XML");
    write(snapshots.filePath(QStringLiteral("2025-10-27-02-20-40-0700.rtf")), cocoaText("Stary tekst."));
    write(snapshots.filePath(QStringLiteral("2025-10-28-10-00-00-0700.rtf")), cocoaText("Nowszy tekst."));
    write(data("E1", "content.rtf"), cocoaText("Koniec."));
    write(data("IMG", "content.png"), pngBytes());
    write(data("IMG", "synopsis.txt"), QStringLiteral("Mapa szlaku.").toUtf8());
    write(data("IMG", "notes.rtf"), cocoaText("Z archiwum."));
    write(data("R1", "content.rtf"), cocoaText("Oaza le\\u380 y na po\\u322 udniu."));
    write(data("K1", "content.rtf"), cocoaText("Przewodniczka."));
    write(data("T1", "content.rtf"), cocoaText("Stary."));
    return project.path();
}

}  // namespace kalahari::test
