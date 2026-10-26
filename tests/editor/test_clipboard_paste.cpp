/// @file test_clipboard_paste.cpp
/// @brief Text of other programs pasted in the three paste modes, and the HTML and plain text
///        the other programs get of Kalahari's text
///
/// The HTML samples follow what Word, LibreOffice Writer and a browser (Chrome) put on the
/// clipboard: written after their formats, not taken from the programs themselves.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/settings_manager.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/clipboard_handler.h>
#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/gui/panels/editor_panel.h>
#include <kalahari/gui/settings_dialog.h>
#include "editor_test_utils.h"

#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMimeData>
#include <QPushButton>
#include <QTextBlock>
#include <QTextDocument>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QXmlStreamReader>
#include <memory>
#include <string>

using namespace kalahari::editor;
using namespace kalahari::test;

namespace {

/// Word's HTML: its styles in an HTML comment in the head, the text between
/// <!--StartFragment--> and <!--EndFragment-->
const char* const WORD_HTML = R"html(<html xmlns:o="urn:schemas-microsoft-com:office:office"
xmlns:w="urn:schemas-microsoft-com:office:word" xmlns="http://www.w3.org/TR/REC-html40">
<head>
<meta http-equiv=Content-Type content="text/html; charset=utf-8">
<meta name=ProgId content=Word.Document>
<link rel=File-List href="file:///C:/Users/X/AppData/Local/Temp/msohtmlclip1/01/clip_filelist.xml">
<style>
<!--
 /* Style Definitions */
 p.MsoNormal, li.MsoNormal, div.MsoNormal
	{margin:0cm;
	margin-bottom:8.0pt;
	font-size:11.0pt;
	font-family:"Calibri",sans-serif;}
h1
	{font-size:16.0pt;
	font-family:"Calibri Light",sans-serif;
	color:#2F5496;
	font-weight:normal;}
p.MsoQuote, li.MsoQuote, div.MsoQuote
	{margin-left:43.2pt;
	text-align:center;
	font-style:italic;}
-->
</style>
</head>
<body lang=PL style='tab-interval:35.4pt;word-wrap:break-word'>
<p>Outside the fragment</p>
<!--StartFragment-->

<h1>Chapter One<o:p></o:p></h1>

<p class=MsoNormal>Plain <b>bold</b> <i>italic</i> <u>under</u> <s>struck</s> H<sub>2</sub>O
x<sup>2</sup> <span style='font-family:"Comic Sans MS"'>font</span> <span
style='font-size:20.0pt'>big</span> <span style='color:red'>red</span> <span
style='background:yellow;mso-highlight:yellow'>marked</span><o:p></o:p></p>

<p class=MsoNormal align=center style='text-align:center'>Centered<o:p></o:p></p>

<p class=MsoNormal align=right style='text-align:right'>Right<o:p></o:p></p>

<p class=MsoQuote>A quote<o:p></o:p></p>

<p class=MsoIntenseQuote>An intense quote<o:p></o:p></p>

<div class=MsoNormal align=center style='text-align:center'>

<hr size=2 width="100%" align=center>

</div>

<p class=MsoNormal>Before the page break<o:p></o:p></p>

<span style='font-size:11.0pt;font-family:"Calibri",sans-serif'><br
clear=all style='mso-special-character:line-break;page-break-before:always'>
</span>

<p class=MsoNormal>After the page break<o:p></o:p></p>

<p class=MsoNormal><o:p>&nbsp;</o:p></p>

<p class=MsoNormal>Line<br>
two<o:p></o:p></p>

<!--EndFragment-->
<p>Outside the fragment</p>
</body>
</html>)html";

/// LibreOffice Writer's HTML: a whole document, its styles in the head
const char* const LIBREOFFICE_HTML = R"html(<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
<head>
	<meta http-equiv="content-type" content="text/html; charset=utf-8"/>
	<meta name="generator" content="LibreOffice 7.6 (Windows)"/>
	<style type="text/css">
		@page { size: 21cm 29.7cm; margin: 2cm }
		p { line-height: 115%; margin-bottom: 0.25cm; background: transparent }
		h1 { margin-bottom: 0.21cm; background: transparent; page-break-after: avoid }
		h1.western { font-family: "Liberation Serif", serif; font-size: 24pt; font-weight: bold }
		blockquote { margin-left: 1cm; margin-right: 1cm; margin-bottom: 0.5cm }
	</style>
</head>
<body lang="pl-PL" link="#000080" vlink="#800000" dir="ltr"><h1 class="western">Title</h1>
<h3 class="western">Third level</h3>
<h4 class="western">Fourth level</h4>
<p style="line-height: 100%; margin-bottom: 0cm">Text <b>bold</b> <i>italic</i> <font face="Comic Sans MS">font</font> <font color="#ff0000">red</font> <font size="5" style="font-size: 18pt">big</font></p>
<p align="center" style="line-height: 100%; margin-bottom: 0cm">Centered</p>
<p align="justify" style="line-height: 100%; margin-bottom: 0cm">Justified</p>
<p align="left" style="line-height: 100%; margin-bottom: 0cm">Left</p>
<blockquote>A quote</blockquote>
<hr/>

<p style="line-height: 100%; margin-bottom: 0cm; page-break-before: always">After the page break</p>
<p style="line-height: 100%; margin-bottom: 0cm"><br/>

</p>
<p style="line-height: 100%; margin-bottom: 0cm">After the empty one</p>
</body>
</html>)html";

/// A browser's HTML (Chrome on Windows): the styles of each element inline
const char* const BROWSER_HTML = R"html(<html><body>
<!--StartFragment--><h1 style="margin: 0px 0px 16px; font-size: 2em; font-weight: 600; color: rgb(31, 35, 40); font-family: -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, Helvetica, Arial, sans-serif; background-color: rgb(255, 255, 255);">A page</h1><p dir="auto" style="color: rgb(31, 35, 40); font-family: -apple-system, BlinkMacSystemFont, &quot;Segoe UI&quot;, Helvetica, Arial, sans-serif; font-size: 16px;">Text <strong style="font-weight: 600;">strong</strong> and <em>emphasized</em>, <a href="https://example.com" style="color: rgb(9, 105, 218);">a link</a> and <code style="font-family: monospace;">code</code>.</p><blockquote style="margin: 0px 0px 16px; padding: 0px 1em; color: rgb(89, 99, 110); border-left: 0.25em solid rgb(209, 217, 224);"><p dir="auto" style="margin-top: 0px;">A quote</p><p dir="auto">Its second paragraph</p></blockquote><hr style="height: 0.25em; margin: 24px 0px; background-color: rgb(209, 217, 224); border: 0px;"><ul><li>one</li><li>two</li></ul><p>Spans: <span style="font-weight: 700;">bold</span> <span style="font-style: italic;">italic</span> <span style="text-decoration: underline line-through;">both</span> <span style="vertical-align: super;">up</span></p><div>A div</div><div>Another<br>with a break</div><table><tr><td>a</td><td>b</td></tr></table><p><img src="file:///C:/x.png" alt="picture">After the picture</p><!--EndFragment-->
</body>
</html>)html";

/// A text as UTF-8, for the report of a failed check
std::string utf8(const QString& text) {
    return text.toStdString();
}

QString html(const char* text) {
    return QString::fromUtf8(text);
}

/// The KML of HTML pasted in a mode, as UTF-8
std::string pasted(const char* text, PasteMode mode) {
    return utf8(ClipboardHandler::htmlToKml(html(text), mode));
}

QString kml(const char* paragraphs) {
    return QStringLiteral("<kml>") + QString::fromUtf8(paragraphs) + QStringLiteral("</kml>");
}

/// An editor with KML loaded
std::unique_ptr<BookEditor> editorWith(const QString& content) {
    auto editor = std::make_unique<BookEditor>();
    resizeWidget(*editor, QSize(600, 400));
    editor->fromKml(content);
    return editor;
}

}  // namespace

// =============================================================================
// HTML of other programs
// =============================================================================

TEST_CASE("Paste: Word's HTML in the three modes", "[editor][paste]") {
    SECTION("Merge Formatting: the structure, the emphasis, center and right") {
        CHECK(pasted(WORD_HTML, PasteMode::Merge) ==
              utf8(kml("<h1>Chapter One</h1>"
                       "<p>Plain <b>bold</b> <i>italic</i> <u>under</u> <s>struck</s> "
                       "H<sub>2</sub>O x<sup>2</sup> font big red marked</p>"
                       "<p align=\"center\">Centered</p>"
                       "<p align=\"right\">Right</p>"
                       "<p style=\"quote\">A quote</p>"
                       "<p style=\"quote\">An intense quote</p>"
                       "<hr/>"
                       "<p>Before the page break</p>"
                       "<page-break/>"
                       "<p>After the page break</p>"
                       "<p></p>"
                       "<p>Line<br/>two</p>")));
    }

    SECTION("Keep Source Formatting: also Word's styles, fonts, sizes and colors") {
        CHECK(pasted(WORD_HTML, PasteMode::Source) ==
              utf8(kml("<h1><span font=\"Calibri Light\" size=\"16\" color=\"#2f5496\">Chapter "
                       "One</span></h1>"
                       "<p><span font=\"Calibri\" size=\"11\">Plain </span>"
                       "<b font=\"Calibri\" size=\"11\">bold</b>"
                       "<span font=\"Calibri\" size=\"11\"> </span>"
                       "<i font=\"Calibri\" size=\"11\">italic</i>"
                       "<span font=\"Calibri\" size=\"11\"> </span>"
                       "<u font=\"Calibri\" size=\"11\">under</u>"
                       "<span font=\"Calibri\" size=\"11\"> </span>"
                       "<s font=\"Calibri\" size=\"11\">struck</s>"
                       "<span font=\"Calibri\" size=\"11\"> H</span>"
                       "<sub font=\"Calibri\" size=\"11\">2</sub>"
                       "<span font=\"Calibri\" size=\"11\">O x</span>"
                       "<sup font=\"Calibri\" size=\"11\">2</sup>"
                       "<span font=\"Calibri\" size=\"11\"> </span>"
                       "<span font=\"Comic Sans MS\" size=\"11\">font</span>"
                       "<span font=\"Calibri\" size=\"11\"> </span>"
                       "<span font=\"Calibri\" size=\"20\">big</span>"
                       "<span font=\"Calibri\" size=\"11\"> </span>"
                       "<span font=\"Calibri\" size=\"11\" color=\"#ff0000\">red</span>"
                       "<span font=\"Calibri\" size=\"11\"> </span>"
                       "<span font=\"Calibri\" size=\"11\" bg=\"#ffff00\">marked</span></p>"
                       "<p align=\"center\"><span font=\"Calibri\" size=\"11\">Centered</span></p>"
                       "<p align=\"right\"><span font=\"Calibri\" size=\"11\">Right</span></p>"
                       "<p style=\"quote\" align=\"center\"><i>A quote</i></p>"
                       "<p style=\"quote\">An intense quote</p>"
                       "<hr/>"
                       "<p><span font=\"Calibri\" size=\"11\">Before the page break</span></p>"
                       "<page-break/>"
                       "<p><span font=\"Calibri\" size=\"11\">After the page break</span></p>"
                       "<p></p>"
                       "<p><span font=\"Calibri\" size=\"11\">Line<br/>two</span></p>")));
    }

    SECTION("Keep Text Only: the paragraphs and line breaks") {
        CHECK(pasted(WORD_HTML, PasteMode::Text) ==
              utf8(kml("<p>Chapter One</p>"
                       "<p>Plain bold italic under struck H2O x2 font big red marked</p>"
                       "<p>Centered</p>"
                       "<p>Right</p>"
                       "<p>A quote</p>"
                       "<p>An intense quote</p>"
                       "<p>Before the page break</p>"
                       "<p>After the page break</p>"
                       "<p></p>"
                       "<p>Line<br/>two</p>")));
    }
}

TEST_CASE("Paste: LibreOffice's HTML in the three modes", "[editor][paste]") {
    SECTION("Merge Formatting") {
        CHECK(pasted(LIBREOFFICE_HTML, PasteMode::Merge) ==
              utf8(kml("<h1>Title</h1>"
                       "<h3>Third level</h3>"
                       "<h3>Fourth level</h3>"
                       "<p>Text <b>bold</b> <i>italic</i> font red big</p>"
                       "<p align=\"center\">Centered</p>"
                       "<p>Justified</p>"
                       "<p>Left</p>"
                       "<p style=\"quote\">A quote</p>"
                       "<hr/>"
                       "<page-break/>"
                       "<p>After the page break</p>"
                       "<p></p>"
                       "<p>After the empty one</p>")));
    }

    SECTION("Keep Source Formatting: every alignment, the fonts, sizes and colors") {
        CHECK(pasted(LIBREOFFICE_HTML, PasteMode::Source) ==
              utf8(kml("<h1><span font=\"Liberation Serif\" size=\"24\">Title</span></h1>"
                       "<h3>Third level</h3>"
                       "<h3>Fourth level</h3>"
                       "<p>Text <b>bold</b> <i>italic</i> <span font=\"Comic Sans MS\">font</span> "
                       "<span color=\"#ff0000\">red</span> <span size=\"18\">big</span></p>"
                       "<p align=\"center\">Centered</p>"
                       "<p align=\"justify\">Justified</p>"
                       "<p align=\"left\">Left</p>"
                       "<p style=\"quote\">A quote</p>"
                       "<hr/>"
                       "<page-break/>"
                       "<p>After the page break</p>"
                       "<p></p>"
                       "<p>After the empty one</p>")));
    }

    SECTION("Keep Text Only") {
        CHECK(pasted(LIBREOFFICE_HTML, PasteMode::Text) ==
              utf8(kml("<p>Title</p><p>Third level</p><p>Fourth level</p>"
                       "<p>Text bold italic font red big</p><p>Centered</p><p>Justified</p>"
                       "<p>Left</p><p>A quote</p><p>After the page break</p><p></p>"
                       "<p>After the empty one</p>")));
    }
}

TEST_CASE("Paste: a browser's HTML in the three modes", "[editor][paste]") {
    SECTION("Merge Formatting: a heading's own weight, a link's look and pictures go") {
        CHECK(pasted(BROWSER_HTML, PasteMode::Merge) ==
              utf8(kml("<h1>A page</h1>"
                       "<p>Text <b>strong</b> and <i>emphasized</i>, a link and code.</p>"
                       "<p style=\"quote\">A quote</p>"
                       "<p style=\"quote\">Its second paragraph</p>"
                       "<hr/>"
                       "<ul><li>one</li><li>two</li></ul>"
                       "<p>Spans: <b>bold</b> <i>italic</i> <u><s>both</s></u> <sup>up</sup></p>"
                       "<p>A div</p>"
                       "<p>Another<br/>with a break</p>"
                       "<p>a\tb</p>"
                       "<p>After the picture</p>")));
    }

    SECTION("Keep Source Formatting: the first font a computer can have, pixels as points") {
        const QString kmlText =
            ClipboardHandler::htmlToKml(html(BROWSER_HTML), PasteMode::Source);
        CHECK(kmlText.startsWith(QStringLiteral(
            "<kml><h1><span font=\"Segoe UI\" color=\"#1f2328\" bg=\"#ffffff\">A page</span></h1>"
            "<p><span font=\"Segoe UI\" size=\"12\" color=\"#1f2328\">Text </span>")));
        // A link keeps its font, not its color; a generic family (monospace) is no font
        CHECK(kmlText.contains(
            QStringLiteral("<span font=\"Segoe UI\" size=\"12\">a link</span>")));
        CHECK(kmlText.contains(QStringLiteral("<span size=\"12\" color=\"#1f2328\">code</span>")));
        CHECK(kmlText.contains(
            QStringLiteral("<p style=\"quote\"><span color=\"#59636e\">A quote</span></p>")));
    }

    SECTION("Keep Text Only") {
        CHECK(pasted(BROWSER_HTML, PasteMode::Text) ==
              utf8(kml("<p>A page</p><p>Text strong and emphasized, a link and code.</p>"
                       "<p>A quote</p><p>Its second paragraph</p>"
                       "<p>\u2022 one</p><p>\u2022 two</p><p>Spans: bold italic both up</p>"
                       "<p>A div</p><p>Another<br/>with a break</p><p>a\tb</p>"
                       "<p>After the picture</p>")));
    }
}

TEST_CASE("Paste: what a browser shows of HTML", "[editor][paste]") {
    const auto merged = [](const char* text) {
        return ClipboardHandler::htmlToKml(QString::fromUtf8(text), PasteMode::Merge);
    };

    SECTION("Nothing to show: no KML") {
        CHECK(merged("").isEmpty());
        CHECK(merged("<html><body> \n </body></html>").isEmpty());
        CHECK(merged("<p><img src=\"picture.png\"></p>").isEmpty());
        CHECK(merged("<p></p><div> </div>").isEmpty());
    }

    SECTION("The spaces at the ends of a line, and a line break that ends a paragraph, go") {
        CHECK(utf8(merged("<p>   a   b   </p>")) == utf8(kml("<p>a b</p>")));
        CHECK(utf8(merged("<p>a <br> b</p>")) == utf8(kml("<p>a<br/>b</p>")));
        CHECK(utf8(merged("<p>a<br></p>")) == utf8(kml("<p>a</p>")));
        CHECK(utf8(merged("<p>a<br><br></p>")) == utf8(kml("<p>a<br/></p>")));
        CHECK(utf8(merged("<p>&nbsp; a</p>")) == utf8(kml("<p>\u00A0 a</p>")));
        CHECK(utf8(merged("<p>a&nbsp;</p>")) == utf8(kml("<p>a</p>")));
    }

    SECTION("Preformatted text keeps its lines, tabs and empty lines") {
        CHECK(utf8(merged("<pre>one\n\ttwo\n\nthree</pre>")) ==
              utf8(kml("<p>one</p><p>\ttwo</p><p></p><p>three</p>")));
    }

    SECTION("Headings: h4 to h6 are h3; a heading's bold is its look, its italic stays") {
        CHECK(utf8(merged("<h2>A</h2><h5>B</h5><h6>C</h6>")) ==
              utf8(kml("<h2>A</h2><h3>B</h3><h3>C</h3>")));
        CHECK(utf8(merged("<h1>A <b>bold</b> <i>italic</i></h1>")) ==
              utf8(kml("<h1>A bold <i>italic</i></h1>")));
    }

    SECTION("Quotes: every paragraph of a blockquote, also nested ones; a list in it is a list") {
        CHECK(utf8(merged("<blockquote><p>a</p><p>b</p><blockquote>c</blockquote></blockquote>"
                          "<p>d</p>")) ==
              utf8(kml("<p style=\"quote\">a</p><p style=\"quote\">b</p><p style=\"quote\">c</p>"
                       "<p>d</p>")));
        CHECK(utf8(merged("<blockquote><div>a</div><ul><li>b</li></ul></blockquote>")) ==
              utf8(kml("<p style=\"quote\">a</p><ul><li>b</li></ul>")));
    }

    SECTION("Page breaks: before and after a paragraph, two in a row are one") {
        CHECK(utf8(merged("<p>a</p><p style=\"page-break-before: always\">b</p>")) ==
              utf8(kml("<p>a</p><page-break/><p>b</p>")));
        CHECK(utf8(merged("<p style=\"page-break-after: always\">a</p><p>b</p>")) ==
              utf8(kml("<p>a</p><page-break/><p>b</p>")));
        CHECK(utf8(merged("<p style=\"page-break-after: always\">a</p>"
                          "<p style=\"page-break-before: always\">b</p>")) ==
              utf8(kml("<p>a</p><page-break/><p>b</p>")));
        CHECK(utf8(merged("<p>a<br style=\"page-break-before:always\">b</p>")) ==
              utf8(kml("<p>a</p><page-break/><p>b</p>")));
    }

    SECTION("Lists: lists of the editor, a list in an item a level of its list") {
        CHECK(utf8(merged("<ul><li>a<ul><li>b<ul><li>c</li></ul></li></ul></li></ul>"
                          "<ol><li>d</li><li>e</li></ol><ol type=\"a\"><li>f</li></ol>"
                          "<ul style=\"list-style-type: square\"><li>g</li></ul>")) ==
              utf8(kml("<ul><li>a<ul type=\"circle\"><li>b<ul type=\"square\"><li>c</li></ul></li>"
                       "</ul></li></ul>"
                       "<ol><li>d</li><li>e</li></ol><ol type=\"a\"><li>f</li></ol>"
                       "<ul type=\"square\"><li>g</li></ul>")));
        // Numbers in a list of bullets, and the bullets again after them
        CHECK(utf8(merged("<ul><li>a<ol><li>b</li></ol></li><li>c</li></ul>")) ==
              utf8(kml("<ul><li>a<ol><li>b</li></ol></li><li>c</li></ul>")));
    }

    SECTION("Lists: only the text keeps their numbers and bullets") {
        CHECK(pasted("<ul><li>a<ul><li>b</li></ul></li></ul><ol><li>d</li><li>e</li></ol>",
                     PasteMode::Text) ==
              utf8(kml("<p>\u2022 a</p><p>\u25E6 b</p><p>1. d</p><p>2. e</p>")));
    }

    SECTION("Lists of Word: their numbers, bullets and levels") {
        // As Word writes them: the list and level in the paragraph's style, the number in a
        // span that only an old browser shows
        const QString word = QStringLiteral(
            "<html><body><!--StartFragment-->"
            "<p class=MsoListParagraphCxSpFirst style='text-indent:-18.0pt;mso-list:l0 level1 lfo1'>"
            "<![if !supportLists]><span style='mso-fareast-font-family:Calibri'><span "
            "style='mso-list:Ignore'>1.<span style='font:7.0pt \"Times New Roman\"'>&nbsp;&nbsp;"
            "&nbsp;&nbsp;&nbsp; </span></span></span><![endif]>One<o:p></o:p></p>"
            "<p class=MsoListParagraphCxSpMiddle style='margin-left:72.0pt;text-indent:-18.0pt;"
            "mso-list:l0 level2 lfo1'><![if !supportLists]><span><span style='mso-list:Ignore'>a)"
            "<span style='font:7.0pt \"Times New Roman\"'>&nbsp;&nbsp;&nbsp; </span></span></span>"
            "<![endif]>One <b>a</b><o:p></o:p></p>"
            "<p class=MsoListParagraphCxSpMiddle style='text-indent:-18.0pt;mso-list:l0 level1 lfo1'>"
            "<![if !supportLists]><span><span style='mso-list:Ignore'>2.<span style='font:7.0pt "
            "\"Times New Roman\"'>&nbsp;&nbsp;&nbsp; </span></span></span><![endif]>Two<o:p></o:p></p>"
            "<p class=MsoNormal>Between</p>"
            "<p class=MsoListParagraphCxSpFirst style='text-indent:-18.0pt;mso-list:l1 level1 lfo2'>"
            "<![if !supportLists]><span style='font-family:Symbol'><span style='mso-list:Ignore'>"
            "\u00B7<span style='font:7.0pt \"Times New Roman\"'>&nbsp;&nbsp;&nbsp; </span></span>"
            "</span><![endif]>Bullet<o:p></o:p></p>"
            "<p class=MsoListParagraphCxSpLast style='margin-left:72.0pt;text-indent:-18.0pt;"
            "mso-list:l1 level2 lfo2'><![if !supportLists]><span style='font-family:\"Courier New\"'>"
            "<span style='mso-list:Ignore'>o<span style='font:7.0pt \"Times New Roman\"'>&nbsp;"
            "&nbsp; </span></span></span><![endif]>Circle<o:p></o:p></p>"
            "<p class=MsoListParagraph style='text-indent:-18.0pt;mso-list:l2 level1 lfo3'>"
            "<![if !supportLists]><span><span style='mso-list:Ignore'>iii.<span>&nbsp; </span></span>"
            "</span><![endif]>Third<o:p></o:p></p>"
            "<!--EndFragment--></body></html>");
        CHECK(utf8(ClipboardHandler::htmlToKml(word, PasteMode::Merge)) ==
              utf8(kml("<ol><li>One<ol type=\"a\" suffix=\")\"><li>One <b>a</b></li></ol></li>"
                       "<li>Two</li></ol>"
                       "<p>Between</p>"
                       "<ul><li>Bullet<ul type=\"circle\"><li>Circle</li></ul></li></ul>"
                       "<ol type=\"i\" start=\"3\"><li>Third</li></ol>")));
        CHECK(utf8(ClipboardHandler::htmlToKml(word, PasteMode::Text)) ==
              utf8(kml("<p>1. One</p><p>a) One a</p><p>2. Two</p><p>Between</p>"
                       "<p>\u2022 Bullet</p><p>\u25E6 Circle</p><p>iii. Third</p>")));
    }

    SECTION("Tables: a row is a paragraph, its cells separated by tabs") {
        CHECK(utf8(merged("<table><tr><td>a</td><td><p>b</p><p>c</p></td></tr>"
                          "<tr><td colspan=\"2\">d</td></tr></table>")) ==
              utf8(kml("<p>a\tb<br/>c</p><p>d</p>")));
    }

    SECTION("Characters XML cannot hold go") {
        const QString result = merged("<p>a&#x1;b&#xFFFF;c</p>");
        CHECK(utf8(result) == utf8(kml("<p>abc</p>")));
        QXmlStreamReader reader(result);
        while (!reader.atEnd()) {
            reader.readNext();
        }
        CHECK_FALSE(reader.hasError());
    }

    SECTION("Kalahari's text in KML is escaped") {
        CHECK(utf8(merged("<p>&lt;b&gt; &amp; \"</p>")) ==
              utf8(kml("<p>&lt;b&gt; &amp; &quot;</p>")));
    }

    SECTION("Merge Formatting reads no style sheet of Word's head: its quote is not italic") {
        CHECK(utf8(merged("<html><head><style>p.MsoQuote {font-style: italic}</style></head>"
                          "<body><!--StartFragment--><p class=MsoQuote>a</p><!--EndFragment-->"
                          "</body></html>")) ==
              utf8(kml("<p style=\"quote\">a</p>")));
    }
}

// =============================================================================
// Paste modes in the editor
// =============================================================================

TEST_CASE("Paste: the mode of the setting", "[editor][paste]") {
    CHECK(ClipboardHandler::pasteModeFromId(QStringLiteral("merge")) == PasteMode::Merge);
    CHECK(ClipboardHandler::pasteModeFromId(QStringLiteral("source")) == PasteMode::Source);
    CHECK(ClipboardHandler::pasteModeFromId(QStringLiteral("text")) == PasteMode::Text);
    CHECK(ClipboardHandler::pasteModeFromId(QString()) == PasteMode::Merge);
    CHECK(ClipboardHandler::pasteModeFromId(QStringLiteral("other")) == PasteMode::Merge);
}

TEST_CASE("Paste: Settings > Editor > General gives the editors the mode",
          "[gui][settings][paste]") {
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const std::string before = settings.get<std::string>(PASTE_MODE_KEY);
    settings.set<std::string>(PASTE_MODE_KEY, PASTE_MODE_MERGE);
    kalahari::gui::EditorPanel panel;
    BookEditor* bookEditor = panel.getBookEditor();
    CHECK(bookEditor->pasteMode() == PasteMode::Merge);

    kalahari::gui::SettingsDialog dialog(nullptr);
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->text(0) == QStringLiteral("General") && (*it)->parent() &&
            (*it)->parent()->text(0) == QStringLiteral("Editor")) {
            tree->setCurrentItem(*it);
        }
    }
    QComboBox* mode = nullptr;
    for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
        if (combo->findData(QString::fromLatin1(PASTE_MODE_SOURCE)) >= 0) {
            mode = combo;
        }
    }
    REQUIRE(mode != nullptr);
    REQUIRE(mode->count() == 3);
    CHECK(utf8(mode->itemText(0)) == "Merge Formatting");
    CHECK(utf8(mode->itemText(1)) == "Keep Source Formatting");
    CHECK(utf8(mode->itemText(2)) == "Keep Text Only");
    CHECK(utf8(mode->currentData().toString()) == PASTE_MODE_MERGE);

    mode->setCurrentIndex(mode->findData(QString::fromLatin1(PASTE_MODE_SOURCE)));
    dialog.applyButton()->click();
    CHECK(settings.get<std::string>(PASTE_MODE_KEY) == PASTE_MODE_SOURCE);
    QCoreApplication::processEvents();
    CHECK(bookEditor->pasteMode() == PasteMode::Source);

    mode->setCurrentIndex(mode->findData(QString::fromLatin1(PASTE_MODE_TEXT)));
    dialog.applyButton()->click();
    QCoreApplication::processEvents();
    CHECK(bookEditor->pasteMode() == PasteMode::Text);

    settings.set<std::string>(PASTE_MODE_KEY, before);
    QCoreApplication::processEvents();
}

TEST_CASE("Paste: text of another program in the editor, in each mode", "[editor][paste]") {
    auto editor = editorWith(kml("<p><i>xy</i></p>"));
    CHECK(editor->pasteMode() == PasteMode::Merge);
    editor->setCursorPosition({0, 1});  // x|y, in italic text
    QMimeData pasted;
    pasted.setText(QStringLiteral("A B"));
    pasted.setHtml(QStringLiteral("<p><span style=\"font-family: Arial\">A</span> <b>B</b></p>"));

    SECTION("Merge Formatting: the look of the place, with its own emphasis") {
        editor->insertFromMimeData(&pasted);
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>xA </i><b><i>B</i></b><i>y</i></p>")));
    }

    SECTION("Keep Source Formatting: its own look") {
        editor->setPasteMode(PasteMode::Source);
        editor->insertFromMimeData(&pasted);
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p><i>x</i><span font=\"Arial\">A</span> <b>B</b><i>y</i></p>")));
    }

    SECTION("Keep Text Only: the look of the place") {
        editor->setPasteMode(PasteMode::Text);
        editor->insertFromMimeData(&pasted);
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>xA By</i></p>")));
    }

    SECTION("Text copied in Kalahari keeps its formatting in every mode") {
        editor->setPasteMode(PasteMode::Text);
        QMimeData own;
        own.setText(QStringLiteral("A"));
        own.setData(QString::fromLatin1(MIME_KML), QByteArrayLiteral("<kml><p><b>A</b></p></kml>"));
        editor->insertFromMimeData(&own);
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>x</i><b>A</b><i>y</i></p>")));
    }

    SECTION("HTML without text to show: its plain text") {
        pasted.setHtml(QStringLiteral("<p><img src=\"picture.png\"></p>"));
        editor->insertFromMimeData(&pasted);
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>xA By</i></p>")));
    }
}

TEST_CASE("Paste: the pastes of the Edit menu in each mode, whatever Paste does",
          "[editor][paste]") {
    auto editor = editorWith(kml("<p><i>xy</i></p>"));
    editor->setCursorPosition({0, 1});  // x|y, in italic text
    const auto copy = [](const QString& text, const QString& html, const QByteArray& own = {}) {
        auto* data = new QMimeData();
        data->setText(text);
        data->setHtml(html);
        if (!own.isEmpty()) {
            data->setData(QString::fromLatin1(MIME_KML), own);
        }
        QGuiApplication::clipboard()->setMimeData(data);
    };
    copy(QStringLiteral("A B"),
         QStringLiteral("<p><span style=\"font-family: Arial\">A</span> <b>B</b></p>"));

    SECTION("Paste with Source Formatting, while Paste merges") {
        editor->pasteAs(PasteMode::Source);
        CHECK(utf8(editor->toKml()) ==
              utf8(kml("<p><i>x</i><span font=\"Arial\">A</span> <b>B</b><i>y</i></p>")));
        CHECK(editor->pasteMode() == PasteMode::Merge);
    }

    SECTION("Paste and Merge Formatting, while Paste keeps only the text") {
        editor->setPasteMode(PasteMode::Text);
        editor->pasteAs(PasteMode::Merge);
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>xA </i><b><i>B</i></b><i>y</i></p>")));
    }

    SECTION("Paste Text Only: one step of Undo takes it back") {
        editor->pasteAs(PasteMode::Text);
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>xA By</i></p>")));
        editor->undo();
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>xy</i></p>")));
    }

    SECTION("Text copied in Kalahari: without formatting only its text, its paragraphs kept") {
        copy(QStringLiteral("A\nB"), QStringLiteral("<h1>A</h1><p><b>B</b></p>"),
             QByteArrayLiteral("<kml><h1>A</h1><p><b>B</b></p></kml>"));
        editor->pasteAs(PasteMode::Text);
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>xA</i></p><p><i>By</i></p>")));
        editor->undo();
        // With its formatting as Paste gives it: its first paragraph joins the one it goes to
        editor->pasteAs(PasteMode::Source);
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>x</i>A</p><p><b>B</b><i>y</i></p>")));
        editor->undo();
        editor->paste();
        CHECK(utf8(editor->toKml()) == utf8(kml("<p><i>x</i>A</p><p><b>B</b><i>y</i></p>")));
    }
}

TEST_CASE("Paste: headings, quotes, lines and breaks of another program in the editor",
          "[editor][paste][objects]") {
    auto editor = editorWith(kml("<p>ab</p><p></p>"));
    editor->setCursorPosition({1, 0});
    QMimeData pasted;
    pasted.setText(QStringLiteral("Title\nQuote\nBefore\nAfter"));
    pasted.setHtml(QStringLiteral(
        "<h1>Title</h1><blockquote><p>Quote</p></blockquote><hr/><p>Before</p>"
        "<p style=\"page-break-before:always\">After</p>"));
    editor->insertFromMimeData(&pasted);

    const QString expected = kml("<p>ab</p><h1>Title</h1><p style=\"quote\">Quote</p><hr/>"
                                 "<p>Before</p><page-break/><p>After</p>");
    CHECK(utf8(editor->toKml()) == utf8(expected));

    // The line and the break are objects of the text, each a paragraph of its own
    QTextDocument* document = editor->textDocument();
    CHECK(blockObject(document->findBlockByNumber(3)) == TextObject::HorizontalRule);
    CHECK(blockObject(document->findBlockByNumber(5)) == TextObject::PageBreak);

    // One step of Undo takes the whole paste back
    editor->undo();
    CHECK(utf8(editor->toKml()) == utf8(kml("<p>ab</p><p></p>")));
    editor->redo();
    CHECK(utf8(editor->toKml()) == utf8(expected));
}

TEST_CASE("Paste: a format given for the text typed next goes on merged text",
          "[editor][paste]") {
    auto editor = editorWith(kml("<p>x </p>"));
    editor->setCursorPosition({0, 2});
    editor->toggleBold();  // no selection, outside a word: for the text typed next
    QMimeData pasted;
    pasted.setText(QStringLiteral("A"));
    pasted.setHtml(QStringLiteral("<p><i>A</i></p>"));
    editor->insertFromMimeData(&pasted);
    CHECK(utf8(editor->toKml()) == utf8(kml("<p>x <b><i>A</i></b></p>")));
}

// =============================================================================
// HTML and plain text for other programs
// =============================================================================

TEST_CASE("Copy: HTML of headings, quotes, lines and breaks", "[editor][paste]") {
    const auto htmlOf = [](const char* paragraphs, const char* sceneBreak = "asterisks") {
        return utf8(ClipboardHandler::kmlToHtml(kml(paragraphs), QString::fromLatin1(sceneBreak)));
    };

    SECTION("Headings keep their level and alignment") {
        CHECK(htmlOf("<h1>T</h1><h2 align=\"center\">S</h2><h3><b>U</b></h3>") ==
              "<h1>T</h1><h2 style=\"text-align:center\">S</h2><h3><b>U</b></h3>");
    }

    SECTION("Paragraphs in a quote style go in one blockquote, the others are plain") {
        CHECK(htmlOf("<p style=\"quote\">a</p><p style=\"block_quote\">b</p>"
                     "<p style=\"dialogue\">c</p><p style=\"quote\">d</p>") ==
              "<blockquote><p>a</p><p>b</p></blockquote><p>c</p><blockquote><p>d</p></blockquote>");
    }

    SECTION("A horizontal line") {
        CHECK(htmlOf("<p>a</p><hr/><p>b</p>") == "<p>a</p><hr/><p>b</p>");
    }

    SECTION("A scene break as it looks in the editor") {
        const char* scene = "<p>a</p><scene-break/><p>b</p>";
        CHECK(htmlOf(scene) == "<p>a</p><p style=\"text-align:center\">* * *</p><p>b</p>");
        CHECK(htmlOf(scene, "asterism") ==
              utf8(QStringLiteral("<p>a</p><p style=\"text-align:center\">\u2042</p><p>b</p>")));
        CHECK(htmlOf(scene, "fleuron") ==
              utf8(QStringLiteral("<p>a</p><p style=\"text-align:center\">\u2766</p><p>b</p>")));
        CHECK(htmlOf(scene, "line") == "<p>a</p><hr width=\"30%\"/><p>b</p>");
        CHECK(htmlOf(scene, "blank") == utf8(QStringLiteral("<p>a</p><p>\u00A0</p><p>b</p>")));
    }

    SECTION("A page break: the page-break-before of what follows it") {
        CHECK(htmlOf("<p>a</p><page-break/><h1>b</h1>") ==
              "<p>a</p><h1 style=\"page-break-before:always\">b</h1>");
        CHECK(htmlOf("<p>a</p><page-break/><p align=\"right\" style=\"quote\">b</p>") ==
              "<p>a</p><blockquote><p style=\"page-break-before:always;text-align:right\">b</p>"
              "</blockquote>");
        CHECK(htmlOf("<p>a</p><page-break/><hr/>") ==
              "<p>a</p><hr style=\"page-break-before:always\"/>");
    }

    SECTION("A page break that nothing follows, and two in a row: an empty page") {
        CHECK(htmlOf("<p>a</p><page-break/>") ==
              utf8(QStringLiteral("<p>a</p><p style=\"page-break-before:always\">\u00A0</p>")));
        CHECK(htmlOf("<p>a</p><page-break/><page-break/><p>b</p>") ==
              utf8(QStringLiteral("<p>a</p><p style=\"page-break-before:always\">\u00A0</p>"
                                  "<p style=\"page-break-before:always\">b</p>")));
    }

    SECTION("Kalahari's own HTML comes back as the same KML") {
        const QString content =
            kml("<h1>Title</h1><p>Text <b>bold</b> <i>italic</i><br/>line</p>"
                "<p style=\"quote\">a</p><p style=\"quote\">b</p><hr/><p align=\"center\">c</p>"
                "<page-break/><h2>Next</h2>");
        CHECK(utf8(ClipboardHandler::htmlToKml(
                  ClipboardHandler::kmlToHtml(content, QStringLiteral("asterisks")))) ==
              utf8(content));
    }
}

TEST_CASE("Copy: plain text of headings, lines and breaks", "[editor][paste]") {
    const auto textOf = [](const char* paragraphs, const char* sceneBreak) {
        return utf8(ClipboardHandler::kmlToText(kml(paragraphs), QString::fromLatin1(sceneBreak)));
    };
    const char* scene = "<p>a</p><scene-break/><p>b</p>";

    CHECK(textOf("<h1>Title</h1><p>a<br/>b</p><hr/><p style=\"quote\">q</p><scene-break/>"
                 "<p>c</p><page-break/><h2>d</h2>",
                 "asterisks") ==
          utf8(QStringLiteral("Title\na\nb\n") + QString(10, QChar(0x2500)) +
               QStringLiteral("\nq\n* * *\nc\n\f\nd")));
    CHECK(textOf(scene, "asterism") == utf8(QStringLiteral("a\n\u2042\nb")));
    CHECK(textOf(scene, "fleuron") == utf8(QStringLiteral("a\n\u2766\nb")));
    CHECK(textOf(scene, "line") ==
          utf8(QStringLiteral("a\n") + QString(3, QChar(0x2500)) + QStringLiteral("\nb")));
    CHECK(textOf(scene, "blank") == "a\n\nb");
}

TEST_CASE("Copy: the editor gives other programs plain text of its KML", "[editor][paste]") {
    auto editor = editorWith(kml("<p>One<br/>two</p><p><b>Three</b></p>"));
    editor->selectAll();
    const std::unique_ptr<QMimeData> copied = editor->createMimeDataFromSelection();
    REQUIRE(copied);
    CHECK(utf8(copied->text()) == "One\ntwo\nThree");
    // A whole document with the fragment marked, as Word and the browsers put it on the
    // clipboard
    CHECK(utf8(copied->html()) ==
          "<html><head><meta charset=\"utf-8\"></head><body><!--StartFragment-->"
          "<p>One<br/>two</p><p><b>Three</b></p><!--EndFragment--></body></html>");
    // Pasted back from the HTML alone (without Kalahari's own format) it is the same text
    CHECK(utf8(ClipboardHandler::htmlToKml(copied->html())) ==
          utf8(kml("<p>One<br/>two</p><p><b>Three</b></p>")));
}

TEST_CASE("Copy: a list for other programs, with the numbers the editor shows",
          "[editor][paste][lists]") {
    auto editor = editorWith(kml("<ol><li>One<ol type=\"a\" suffix=\")\"><li>One a</li></ol></li>"
                                 "<li>Two</li><li numbered=\"false\">More of two</li>"
                                 "<li value=\"7\">Seven</li></ol><p>Text</p>"
                                 "<ul><li>Coat</li><li>Torch</li></ul>"));
    editor->selectAll();
    std::unique_ptr<QMimeData> copied = editor->createMimeDataFromSelection();
    REQUIRE(copied);
    // A tab for each level below the first, the number or bullet and a space
    CHECK(utf8(copied->text()) ==
          utf8(QStringLiteral("1. One\n\ta) One a\n2. Two\nMore of two\n7. Seven\nText\n"
                              "• Coat\n• Torch")));
    CHECK(utf8(copied->html()) ==
          utf8(ClipboardHandler::htmlDocument(QStringLiteral(
              "<ol><li>One<ol type=\"a\"><li>One a</li></ol></li><li>Two</li>"
              "<li style=\"list-style-type:none\">More of two</li><li value=\"7\">Seven</li></ol>"
              "<p>Text</p><ul><li>Coat</li><li>Torch</li></ul>"))));
    // Pasted back from the HTML alone: the same lists, but the suffix HTML has not
    CHECK(utf8(ClipboardHandler::htmlToKml(copied->html())) ==
          utf8(kml("<ol><li>One<ol type=\"a\"><li>One a</li></ol></li><li>Two</li>"
                   "<li>More of two</li><li>Seven</li></ol><p>Text</p>"
                   "<ul><li>Coat</li><li>Torch</li></ul>")));

    // A part of a list goes on from the number of its first item
    editor->setSelection({{2, 0}, {4, 5}});
    copied = editor->createMimeDataFromSelection();
    REQUIRE(copied);
    CHECK(utf8(copied->text()) == utf8(QStringLiteral("2. Two\nMore of two\n7. Seven")));
    CHECK(utf8(copied->html()) ==
          utf8(ClipboardHandler::htmlDocument(QStringLiteral(
              "<ol start=\"2\"><li>Two</li><li style=\"list-style-type:none\">More of two</li>"
              "<li value=\"7\">Seven</li></ol>"))));

    // KML alone counts its own numbers; bullets of no type of HTML get a CSS one
    CHECK(utf8(ClipboardHandler::kmlToText(
              QStringLiteral("<kml><ol type=\"I\" start=\"4\"><li>a</li><li>b</li></ol></kml>"))) ==
          "IV. a\nV. b");
    CHECK(utf8(ClipboardHandler::kmlToHtml(
              QStringLiteral("<kml><ul type=\"check\"><li>a</li></ul></kml>"))) ==
          utf8(QStringLiteral("<ul style=\"list-style-type:'✓ '\"><li>a</li></ul>")));
}

TEST_CASE("Copy and paste: small and all capitals and the letter spacing", "[editor][paste]") {
    SECTION("Copied as CSS of the span, the spacing in points") {
        CHECK(utf8(ClipboardHandler::kmlToHtml(
                  kml("<p><span caps=\"small\">a</span><span caps=\"all\" spacing=\"1.5\">b</span></p>"),
                  QStringLiteral("asterisks"))) ==
              "<p><span style=\"font-variant:small-caps\">a</span>"
              "<span style=\"text-transform:uppercase;letter-spacing:1.5pt\">b</span></p>");
    }

    const char* const caps =
        "<p><span style=\"font-variant: small-caps\">small</span> "
        "<span style=\"text-transform: uppercase; letter-spacing: 2px\">all</span></p>";

    SECTION("Merge Formatting keeps the capitals, as it keeps the emphasis") {
        CHECK(pasted(caps, PasteMode::Merge) ==
              utf8(kml("<p><span caps=\"small\">small</span> <span caps=\"all\">all</span></p>")));
    }

    SECTION("Keep Source Formatting also keeps the spacing, pixels as points") {
        CHECK(pasted(caps, PasteMode::Source) ==
              utf8(kml("<p><span caps=\"small\">small</span> "
                       "<span caps=\"all\" spacing=\"1.5\">all</span></p>")));
    }

    SECTION("Keep Text Only") {
        CHECK(pasted(caps, PasteMode::Text) == utf8(kml("<p>small all</p>")));
    }
}
