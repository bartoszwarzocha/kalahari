# KML – Kalahari Markup Language

KML is the XML format of chapter text: paragraphs and headings with their styles and own
formatting, lists, tables, horizontal lines and breaks, inline formatting, links and annotations. This document describes the format as the
application reads and writes it. The product specification of the format, including the
elements planned for later versions, is section 4 of
`project_docs/19_text_editor_functional_spec_pl.md`; the elements it lists that the editor does
not support yet are under [Not supported yet](#not-supported-yet).

The code that defines the format:

| What | Where |
|------|-------|
| Tags, synonyms, metadata elements and their attributes | `KmlFormatRegistry` (`include/kalahari/editor/kml_format_registry.h`) |
| Paragraph styles, headings and the objects of the text | `include/kalahari/editor/paragraph_styles.h` |
| A paragraph's own formatting and its attributes | `include/kalahari/editor/paragraph_format.h` |
| Lists, the formats of their levels and the numbers of the items | `include/kalahari/editor/text_lists.h` |
| Tables in the editor's document | `include/kalahari/editor/text_tables.h` |
| Reading | `KmlDocumentModel::loadKml()`, called by `BookEditor::fromKml()` |
| Writing | `KmlSerializer::toKml()`, called by `BookEditor::toKml()` |
| Annotations in the editor's document | `include/kalahari/editor/annotation.h` |
| Links in the editor's document | `include/kalahari/editor/text_link.h` |
| The clipboard's KML, HTML and plain text | `ClipboardHandler` (`include/kalahari/editor/clipboard_handler.h`) |
| Tests | `tests/editor/test_kml_load.cpp`, `test_kml_document_model.cpp`, `test_kml_serializer.cpp`, `test_editor_stage0_kml_roundtrip.cpp`, `test_annotations.cpp`, `test_line_breaks.cpp`, `test_paragraph_styles.cpp`, `test_text_lists.cpp`, `test_character_formatting.cpp`, `test_text_links.cpp`, `test_tables.cpp` |

## Where KML is stored

- **Chapter files** (`.kchapter`) are JSON; the chapter's KML is the string `content.kml`
  (`core::ChapterDocument`).
- **Clipboard:** text copied in the editor is also put on the clipboard as KML, with the
  MIME type `application/x-kalahari-kml`, and pasted back through the same reader.

## Example

```xml
<kml>
  <annotations>
    <annotation id="a1" kind="comment" author="Bartosz" created="2026-10-08T12:00:00Z" done="true">Check the date.</annotation>
    <annotation id="a2" kind="todo">Describe the weather.</annotation>
  </annotations>
  <h1>Chapter One</h1>
  <p>Plain text, <i>italic</i>, <b><i>bold italic</i></b> and <span color="#aa0000">red</span>.</p>
  <p>She met <charref id="r1" target="anna">Anna</charref> in <locref id="r2" target="krakow">Kraków</locref> <anchor ref="a1">on a Tuesday</anchor>.<anchor ref="a2"/></p>
  <scene-break/>
  <p style="quote">Nothing is lost.</p>
</kml>
```

The editor writes KML without line breaks or indentation; the example is indented for
reading.

## Document structure

- **Root element:** `<kml>`. `<doc>` and `<document>` are accepted as well, and content that
  starts with none of them – for example a bare sequence of paragraphs – is read as if it were
  wrapped in `<kml>`. Attributes of the root element are ignored. The editor writes `<kml>`
  without attributes.
- **Paragraphs** are the children of the root element: `<p>`, or `<paragraph>` on reading,
  the headings `<h1>`, `<h2>` and `<h3>` (see [Paragraph](#paragraph)), the lists `<ol>` and
  `<ul>`, whose items are paragraphs too (see [Lists](#lists)), the tables `<table>`, whose
  cells hold paragraphs (see [Tables](#tables)), and the objects `<hr/>`, `<scene-break/>` and
  `<page-break/>` (see [Horizontal line and breaks](#horizontal-line-and-breaks)).
- **Annotations** – the writer's comments, TODOs and notes – are in an `<annotations>` element,
  a child of the root element before the paragraphs (see [Annotations](#annotations)).
- Any other element at this level is skipped together with everything inside it, and text
  between paragraphs is ignored.

## Paragraph

| Attribute | Values | Meaning |
|-----------|--------|---------|
| `align` | `left`, `center`, `right`, `justify` (any letter case on reading) | The paragraph's own alignment |
| `style` | The id of a paragraph style, for example `quote` | The paragraph's style (Format > Text Style) |
| `indent-left`, `indent-right` | Points, -1000 to 1000 | The indent of every line from the left and the right edge of the text |
| `indent-first` | Points, -1000 to 1000 | The indent of the first line from `indent-left`; a negative one is a hanging indent: the first line starts left of the others |
| `space-before`, `space-after` | Points, 0 to 1000 | The space above and below the paragraph |
| `line-height` | A multiple of a single line, 0.5 to 4 | Line spacing: `1` single, `1.5`, `2` double |
| `line-height-min` | Points, 1 to 1000 | Line spacing At least: a larger letter makes its line higher |
| `line-height-exact` | Points, 1 to 1000 | Line spacing Exactly: every line so high |
| `keep-with-next` | Flag | On the page of the paragraph after it |
| `keep-together` | Flag | All its lines on one page |
| `page-break-before` | Flag | Starts on a new page |
| `widow-control` | Flag | No single line of it alone at the top or bottom of a page; on without the attribute |

A paragraph without `align` has no alignment of its own and is shown with the alignment of its
style; a style without one has the editor's default: justified, with the last line aligned to
the left. The editor writes `align` only for paragraphs that have their own alignment. Other
attributes are ignored and not written.

The attributes from `indent-left` on are the paragraph's own formatting (Format > Paragraph):
a part without its attribute is as the paragraph's style gives it, and the editor writes only
the parts the paragraph has of its own, in the order of the table. The layout scales the
lengths as it scales the style's own: in the proportion of the editor's font size to the size
of Normal in the style pack.

- A number is written with at most two decimals and a point (`indent-first="-18"`,
  `line-height="1.15"`). A value outside its range is brought into it; one that is not a
  number is ignored.
- A flag is set by `true` or `1` (any letter case), and is off with any other value, so
  `widow-control="false"` turns the widow and orphan control off. The editor writes `true` or
  `false`.
- Of the three line spacings the first in the order of the table counts.
- Format > Clear Formatting takes the paragraph's own formatting off, as it takes off the
  formatting of its text.

### Headings and paragraph styles

The styles are those of the book's type: its style pack (`resources/booktypes/*/styles.json`)
gives their names, fonts, sizes, spacing and alignment. The types that come with Kalahari use
the base pack, with the styles `normal`, `heading_1`, `heading_2`, `heading_3`, `quote`,
`dialogue`, `code`, `caption` and `footnote`; each adds its own styles or changes these.

| Written as | Style |
|------------|-------|
| `<p>` | Normal (`normal`) |
| `<h1>`, `<h2>`, `<h3>` | Heading 1, 2 and 3 (`heading_1`, `heading_2`, `heading_3`) |
| `<p style="quote">` | The style with that id |

- A heading has the attributes and the content of a paragraph; its `style` attribute is
  ignored.
- `<p style="normal">` is a paragraph in Normal, and `<p style="heading_1">` a heading: the
  editor writes them as `<p>` and `<h1>`.
- A style the book's type does not have is kept: the paragraph is shown in Normal and written
  back with its `style`, so the text loses nothing when it moves to a book of another type.

## Horizontal line and breaks

| Element | Meaning |
|---------|---------|
| `<hr/>` | A horizontal line (Insert > Horizontal Line) |
| `<scene-break/>` | A scene break (Insert > Scene Break, also in the Book menu) |
| `<page-break/>` | A page break (Insert > Page Break, Ctrl+Enter) |

- Each is a block of its own between paragraphs, not inside one. Its attributes and its
  content are ignored, and the editor writes the empty element.
- In the editor each is one character in a paragraph of its own: the cursor passes it with
  one key press, and Backspace or Delete removes it whole.
- A scene break looks as the setting `editor.sceneBreak.style` says (three asterisks, an
  asterism, a fleuron, a short line or a blank line); the file keeps the element whatever the
  look.
- A page break starts a new page in the page view. The continuous views show it as a dashed
  line with the words "Page Break"; the page view shows that line too when the formatting
  marks are shown.
- An object has no text: it adds nothing to the word and character counts. Other programs
  get it from the clipboard as the section "The clipboard of other programs" says.

## Lists

A bulleted list is an `<ul>`, a numbered one an `<ol>`, and each of their `<li>` elements is a
paragraph: an item of the list.

```xml
<ol suffix=")">
  <li>Pack the tent</li>
  <li>Buy food<ul type="dash"><li>bread</li><li>tea</li></ul></li>
  <li value="7">Leave on Monday</li>
</ol>
<p>A paragraph between the parts of the list.</p>
<ol continue="true"><li>Come back</li></ol>
```

- **Levels.** A list has nine levels. A list inside an item is the next level of the same list,
  and its element gives the format of that level: a level may have numbers and the next one
  bullets. The levels a list does not write have the formats of a list of its kind (numbered:
  1., a), i. and again from the fourth level on; bulleted: •, ◦, ▪ and again).
- **The numbers** of a list are counted over the whole chapter: a paragraph outside the list
  between two of its parts leaves the numbering going on (Format > Continue Numbering), which
  KML writes with `continue="true"` on the list after it. A list without it starts its numbers
  anew. An item of a level starts the numbers of its level anew after an item of a lower level
  (1., a), b), 2., a)).
- **An item** has the attributes of a paragraph (`style`, `align` and the paragraph's own
  formatting), but its left and first-line indents are those of its level (`text-indent` and
  `marker-indent`): the layout does not use its `indent-left` and `indent-first`, and
  Format > Paragraph does not give them to an item.
- A paragraph `<p>` inside an item is the item's own text when the item has none before it;
  otherwise it, and the text after a list inside the item, is a paragraph in the indent of the
  item's level without a number or bullet. Other elements inside a list are skipped with
  everything inside them, and text between the items (the indentation of the KML) is ignored.

**Attributes of `<ol>` and `<ul>`:**

| Attribute | Element | Values | Meaning |
|-----------|---------|--------|---------|
| `type` | `<ol>` | `1`, `a`, `A`, `i`, `I` (`1` without it) | The numbers: 1, 2, 3; a, b, c ... z, aa; A, B, C; i, ii, iii; I, II, III |
| `type` | `<ul>` | `disc`, `circle`, `square`, `dash`, `check`, `arrow` or the bullet itself (`disc` without it) | The bullet: •, ◦, ▪, –, ✓, ➢ or any other character |
| `suffix` | `<ol>` | Text (`.` without it) | After the number: `.` or `)` |
| `start` | `<ol>` | Number | The number of the first item (1 without it) |
| `marker-indent` | Both | Points | From the start of the text to the number or bullet; without it 18 points for each level (the first 18, the second 36) |
| `text-indent` | Both | Points | From the start of the text to the text of the item; without it 18 points more than `marker-indent` |
| `marker-align` | Both | `left`, `center`, `right` | Where the number stands at its indent (left without it) |
| `continue` | The `<ol>` or `<ul>` of the first level | Flag | The list goes on from the list before it, with the formats of its levels |

**Attributes of `<li>`:**

| Attribute | Values | Meaning |
|-----------|--------|---------|
| `value` | Number, 0 to 32767 | The item's own number (Set Numbering Value): the next items of its level go on from it |
| `numbered` | `false` or `0` | The item has no number or bullet: a paragraph in the indent of its level (Backspace at the start of an item) |

The editor writes an attribute only when it is not the default, and the elements of a list
in the order of its items: a list of a deeper level inside the item before it. A part of a
list after another list cannot go on with `continue`, which refers to the list just before
it: its first item gets the number it has in the chapter as `value`.

## Tables

```xml
<table>
  <col width="40%"/><col width="60%"/>
  <tr><th><p>Who</p></th><th><p>Role</p></th></tr>
  <tr><td><p>Anna</p></td><td><p>Narrator</p><p>A second paragraph</p></td></tr>
  <tr><td colspan="2"><p>Both</p></td></tr>
</table>
```

- **A table** is a block between paragraphs, as wide as the text. Insert > Table... puts one
  at the cursor: its columns and rows (up to 20 and 100), equal columns, and whether its first
  row is a header row.
- **Rows and cells.** Each `<tr>` is a row, each `<td>` or `<th>` in it a cell, from left to
  right. A cell holds paragraphs: `<p>`, the headings and the lists, with their formatting,
  links and the anchors of annotations; an empty cell has one empty paragraph. Text in a cell
  outside a paragraph is a paragraph of its own. A cell holds only text: a horizontal line or a
  break in it is left out, and the cells of a table inside it give their paragraphs to the
  cell.
- **Merged cells.** `colspan` and `rowspan` (1 to 100) are the columns and rows a cell spans. A
  cell goes in the first column the cells above it do not span into; a row with fewer cells
  than the table has columns gets empty ones, and a cell spanning more rows than the table has
  spans those it has.
- **The header row.** A first row of `<th>` cells only is the header row, which the editor
  shows in bold on a shaded background. A `<th>` in another row is read as a `<td>`, and the
  editor writes it as one.
- **The widths of the columns.** The `<col width="..."/>` elements before the rows, one for
  each column, give the widths in percent of the table's width; the editor writes them in
  percent, rounded to two decimals, for a table that has them (Insert > Table... makes equal
  columns, without widths). Widths in another unit
  (`120px`) are taken in proportion to one another, and a column without a `<col>` gets the
  average width of the others; without `<col>` elements, or when one has no width, the columns
  are equal.
- `<thead>`, `<tbody>`, `<tfoot>` and `<colgroup>` around the rows or the `<col>` elements are
  read as if they were not there. Any other element in a table is skipped with everything
  inside it, and a table without cells is left out.
- **At the start and the end.** The editor's document has a paragraph before a table and after
  it: a table at the start or the end of a chapter gets an empty paragraph there, which the
  editor writes (`<p></p>`).
- **In the editor.** Tab goes to the next cell, in the last one it adds a row; Shift+Tab goes
  to the cell before, Up and Down to the cell above and below. Enter starts a paragraph in the
  cell; Backspace and Delete do not join cells, and next to a table they remove an empty
  paragraph the table does not need. A selection going into or out of a table removes the text
  of the cells it covers, not the cells; a table it covers whole goes with it. In a cell
  Insert > Horizontal Line, Scene Break, Page Break and Table are greyed out.
- **A part of a table** (copied text): the rows from the cell where the part starts to the cell
  where it ends, with its text; the other cells of those rows are empty. A part inside one cell
  is its paragraphs, without the table.
- **Pasted** text with a table puts it at the cursor, splitting the paragraph; an empty
  paragraph next to the table that it does not need goes. Pasted in a cell, the text keeps only
  its text: each row of a table is a paragraph with its cells separated by tabs, and a
  horizontal line or a break an empty paragraph.

## Text

- Spaces and tabs are kept exactly as written, also at the start and end of a paragraph.
- XML entities (`&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;`) and numeric character references
  are decoded. On writing, `&`, `<`, `>`, `"` and `'` are written as entities.
- `<br/>` is a line break: the paragraph goes on in a new line, as after Shift+Enter in the
  editor (a manual line break in Word). The lines keep the paragraph's alignment, spacing and
  formatting, and a paragraph may end with a break, which leaves its last line empty. In the
  editor's text the break is the character U+2028 (line separator); written in the text
  (`&#x2028;`), it is read as a break too, and written back as `<br/>`. For the word and
  character counts the break is whitespace, as a space is.
- A newline character in the text of a paragraph (or `&#10;`) is not a line break: it starts
  a new paragraph, and the indentation of the next line becomes the start of its text.
- `<t>` and `<text>` hold plain text, which belongs to the paragraph; the editor writes the
  text without them.
- Elements inside a paragraph other than those described in this document – also a heading
  or an object put inside a paragraph – are skipped together with everything inside them.

## Line breaks in other formats

Where the text leaves the editor, a line break becomes what the other format has for it, and
comes back as a break where that format tells it apart from the end of a paragraph:

| Where | A line break is written as | Read as a line break |
|-------|----------------------------|----------------------|
| Clipboard, KML | `<br/>` | `<br/>` |
| Clipboard, HTML (Word, LibreOffice, browsers) | `<br/>` inside the paragraph | `<br>` or `<br/>` inside a paragraph |
| Clipboard, plain text | A line end, as the end of a paragraph | A vertical tab (Word writes a manual line break so) or U+2028; a line end starts a new paragraph |
| Plain text files (`.txt`) | A line end | As the plain text of the clipboard |
| Grammar check (LanguageTool) | A line end | – |

Export and import of other formats are not written yet; they are to keep a line break so:

| Format | Export | Import |
|--------|--------|--------|
| DOCX | `<w:br/>` in the run | `<w:br/>` without a type or with `w:type="textWrapping"`, and `<w:cr/>`; a page or column break is not a line break |
| Markdown | A backslash at the end of the line (a hard line break) | A backslash or two spaces at the end of the line |
| EPUB, MOBI | `<br/>` | – |
| PDF | A new line of the same paragraph, without the paragraph's spacing and first-line indent | – |
| InDesign ICML | The character U+2028 (a forced line break) | – |
| LaTeX | `\\` at the end of the line | – |
| Scrivener (RTF) | – | `\line` |

## Inline formatting

| Written | Also accepted on reading | Effect |
|---------|--------------------------|--------|
| `<b>` | `<bold>`, `<strong>` | Bold |
| `<i>` | `<italic>`, `<em>` | Italic |
| `<u>` | `<underline>` | Underline |
| `<s>` | `<strike>`, `<strikethrough>` | Strikethrough |
| `<sub>` | `<subscript>` | Subscript |
| `<sup>` | `<superscript>` | Superscript |
| `<span>` | | No formatting of its own; carries style attributes |

Nested elements add up: `<b><i>text</i></b>` is bold italic. On writing, each run of text
with the same formatting gets its own elements, always in the order `<b><i><u><s>` followed by
`<sub>` or `<sup>`; the synonyms are written as the short form (`<strong>` becomes `<b>`).

**Style attributes** of formatting elements and `<span>`:

| Attribute | Value | Written when |
|-----------|-------|--------------|
| `font` | Font family | The text has its own font family |
| `size` | Size in points (greater than 0) | The text has its own size |
| `color` | Text color (`#rrggbb`; on reading any color name Qt accepts) | The color is set and is not black |
| `bg` | Background color (as `color`) | The background is set and not transparent |
| `caps` | `small` (small capitals) or `all` (all capitals) | The text is shown so; the letters stay as typed |
| `spacing` | Letter spacing in points, -20 to 100 (more than 0 expanded, less condensed) | The text has its own letter spacing |

On writing, the style attributes go on the first formatting element of the run, for example
`<b font="Georgia"><i>text</i></b>`; a run without formatting elements gets a `<span>`.

## Links

```xml
<p>See <a href="https://example.com">the page</a>.</p>
```

| Attribute | Value | Meaning |
|-----------|-------|---------|
| `href` | An address (escaped as XML, `&amp;` for `&`) | The address the link opens |

- The text of `<a>` is the text of the link, with its own formatting inside it. A link goes on
  in more `<a>` elements with the same `href`, one for each run of formatting:
  `<a href="https://example.com">the </a><a href="https://example.com"><b>page</b></a>` is one
  link. Text next to it with another address, or none, is not a part of it.
- On writing, `<a>` goes inside the anchors of annotations and outside the references and the
  formatting elements: `<anchor ref="a1"><a href="..."><charref ...><b>text</b></charref></a></anchor>`.
- Spaces around the address are dropped on reading; `<a>` without an `href`, or with an empty
  one, is plain text.
- The editor keeps any address, but it opens only a web page (`http`, `https`), a file server
  (`ftp`) and a new e-mail (`mailto`); a link to another address (`file:`, `javascript:`) is
  only text. Insert > Link (Ctrl+K) gives an address without a scheme `https://`, and an e-mail
  address `mailto:`.
- The look of a link – its color, underlined – is not in the KML: the editor draws every link
  in the link color of its paper (Settings > Editor > Colors).

## Annotations

Comments, TODOs and notes are kept in the `<annotations>` section; the text says where each of
them is with `<anchor>` elements.

### The `<annotations>` section

Each `<annotation>` element is one annotation; its content is the annotation's text, which may
have several lines (child elements are skipped together with their text).

| Attribute | Value | Meaning |
|-----------|-------|---------|
| `id` | Text, unique in the book | Required |
| `kind` | `comment`, `todo` or `note` | Required |
| `author` | Text | Who made it |
| `created` | Date and time, ISO 8601 | When it was made. Written in UTC, as `2026-10-08T12:00:00Z`; a time without a time zone is read as UTC |
| `done` | Flag | A TODO done, a comment resolved |

- The editor writes the attributes in the order of the table, then the others – those it does
  not know, and a `created` it cannot read – sorted by name, as they were read.
- An annotation without an `id`, with another `kind`, or with an `id` used before in the
  section is left out, and so is one that no anchor in the text refers to. Children of the
  section other than `<annotation>` are skipped.
- The section must come before the anchors that refer to its annotations; the editor writes it
  first, with the annotations in the order of their anchors, and writes no section when the
  text has no annotations.

### Anchors

| Written as | Meaning |
|------------|---------|
| `<anchor ref="a1">text</anchor>` | The annotation `a1` is about this text (a fragment) |
| `<anchor ref="a2"/>` | The annotation `a2` is about this place in the text |

- A fragment may go on in more anchors of the same annotation, in the same or the following
  paragraphs: the editor writes one for each run of formatting, for example
  `<anchor ref="a1">one </anchor><anchor ref="a1"><b>two</b></anchor>`. Anchors of different
  annotations nest.
- On writing, the anchors of a fragment enclose all other elements of its text; an anchor of a
  place goes right after the character before the place, inside the elements of that character.
- An anchor that refers to an annotation the section does not have is dropped; its text stays
  as plain text.

### Annotations on the clipboard

Copied text carries the annotations anchored in it: the KML on the clipboard has its own
`<annotations>` section. Pasted into a chapter that already has an annotation with the same
`id`, the annotation comes in as a copy with a new `id` – unless the text is pasted inside the
fragment of that annotation, which it then simply joins. The HTML and the plain text on the
clipboard, for other programs, have no annotations.

## The clipboard of other programs

The clipboard gives other programs HTML and plain text of the copied KML, and HTML of other
programs is read into KML (`ClipboardHandler::kmlToHtml()`, `kmlToText()` and `htmlToKml()`;
tests in `tests/editor/test_clipboard_paste.cpp`).

| KML | HTML for other programs | Plain text |
|-----|-------------------------|------------|
| `<h1>`, `<h2>`, `<h3>` | `<h1>`, `<h2>`, `<h3>` | A line |
| `<p style="quote">`, `<p style="block_quote">` | A `<p>` in a `<blockquote>`, one for the quote paragraphs in a row | A line |
| `<p>` in another style | `<p>` | A line |
| `align` | CSS `text-align` (`center`, `right`, `justify`) | – |
| `<hr/>` | `<hr/>` | A line of ten `─` (U+2500) |
| `<scene-break/>` | As the setting `editor.sceneBreak.style` shows it: a centered paragraph with `* * *`, `⁂` or `❦`, a short line (`<hr width="30%"/>`) for `line`, an empty paragraph for `blank` | Its symbol (`───` for `line`) on a line, an empty line for `blank` |
| `<page-break/>` | CSS `page-break-before:always` of what follows it; an empty paragraph when nothing does | A form feed (U+000C) on a line of its own |
| `<ol>`, `<ul>`, `<li>` | `<ol>`, `<ul>` and `<li>`, a list of a deeper level inside an item; the type of the numbers, the number of the first item and of an item that does not go on from the one before it | Each item on a line, with its number or bullet as the editor shows it |
| `caps`, `spacing` | CSS `font-variant:small-caps` or `text-transform:uppercase`, `letter-spacing` in points | – |
| `<a href>` | `<a href>` when the address is that of a web page, a file server or an e-mail (`http`, `https`, `ftp`, `mailto`); the text alone for another address | The text alone |
| `<table>` | `<table>` as wide as the text with a thin grid (`border="1"`, `border-collapse:collapse`), the header row in a `<thead>` of `<th>` cells, `colspan` and `rowspan`, the widths of the columns on the cells of the first row | A line for each row, its cells separated by tabs (one also for a cell that a cell above spans into), the paragraphs of a cell by spaces |

The HTML of another program (Word, LibreOffice, a browser) is pasted, and dropped, in the mode
of the setting `editor.paste.mode` (Settings > Editor > General, "Paste from Other
Programs"). Qt reads the HTML (`QTextDocumentFragment::fromHtml()`) as a browser shows it,
without the files it refers to.

| Mode | Value | What the KML keeps |
|------|-------|--------------------|
| Merge Formatting (the default) | `merge` | The headings (`<h4>` to `<h6>` as `<h3>`); the quotes (`<blockquote>`, Word's quote styles) as `<p style="quote">`; `<hr/>`; the page breaks (CSS `page-break-before` and `page-break-after: always`, Word's page break) as `<page-break/>`; the line breaks; bold, italic, underline, strikethrough, subscript and superscript; the links to web pages, file servers and e-mail addresses (a link to a place in the page or to a script comes as its text); centered and right-aligned paragraphs. The text takes the format of the place it is pasted to, with its emphasis on top. |
| Keep Source Formatting | `source` | Also every alignment, the font (the first of a CSS list that is not a generic family), the size (in points) and the colors, and the styles of the head of Word's HTML |
| Keep Text Only | `text` | The paragraphs and line breaks of the plain text, without links |

Text copied in Kalahari is pasted as KML in every mode. The lists of browsers, LibreOffice
and Word come as lists in the modes Merge Formatting and Keep Source Formatting, a list in an
item as the next level of its list; Word's numbers and bullets (also those of the fonts Symbol
and Wingdings) give the levels their formats and first numbers. Keep Text Only keeps the
numbers and bullets as text. Tables come as tables in Merge Formatting and Keep Source
Formatting, with their merged cells, the widths of their columns (in percent, or pixels in
proportion) and their header row: a `<thead>`, or a first row of `<th>` cells, whose bold, and
in Merge Formatting their centering, is the look of the header; in Keep Text Only, and in a
cell, a row is a paragraph with its cells separated by tabs. Images are left out. A heading's
bold is the look of the heading and is not kept. A scene break of another program comes as the paragraph
of its symbol.

## References

These elements mark a range of text. All of their attributes are kept, including ones the
editor does not know, and written back.

| Element | Known attributes | Meaning |
|---------|------------------|---------|
| `<footnote>` | `id`, `number` (integer) | Footnote reference |
| `<charref>` | `id`, `target` | Reference to a character |
| `<locref>` | `id`, `target` | Reference to a location |

- A flag is set by `true` or `1` (any letter case). It is written as `true`, and not at all
  when it is not set. Empty attributes are not written.
- On writing, the known attributes come first, in the order of the table, then the others
  sorted by name.
- References enclose the formatting elements of their range. Several references on the same
  range are nested in the order of the table: `<footnote><charref><b>text</b></charref></footnote>`.

## Unreadable KML

If the KML is not well-formed XML, the editor loads the text read before the error – the
rest of the chapter is not loaded – and logs an error.

## Not supported yet

Elements of the product specification that the editor does not read yet:

- images `<img>`,
- the annotations `<endnote>` and the references `<itemref>`, `<cite>`,
- the root attributes `version` and `lang`, the paragraph attribute `speaker` and the scene
  break attribute `type` (the look of the scene breaks is a setting of the editor).

The specification's `<comment>`, `<todo>` and `<note>` elements are annotations of the
`<annotations>` section, of the kinds `comment`, `todo` and `note`. The style ids of its
example (`heading1`, `dialog`) are not those of the style packs (`heading_1`, `dialogue`): a
paragraph with such a style is shown in Normal and keeps it.

The editor skips these elements, together with everything inside them, when it loads a
chapter, and ignores the attributes. The next save writes the chapter without them, so a file
written by hand or by another program loses them when it is saved in Kalahari.
