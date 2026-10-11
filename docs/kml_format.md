# KML – Kalahari Markup Language

KML is the XML format of chapter text: paragraphs, inline formatting and annotations. This
document describes the format as the application reads and writes it. The product
specification of the format, including the elements planned for later versions, is
section 4 of `project_docs/19_text_editor_functional_spec_pl.md`; the elements it lists that
the editor does not support yet are under [Not supported yet](#not-supported-yet).

The code that defines the format:

| What | Where |
|------|-------|
| Tags, synonyms, metadata elements and their attributes | `KmlFormatRegistry` (`include/kalahari/editor/kml_format_registry.h`) |
| Reading | `KmlDocumentModel::loadKml()`, called by `BookEditor::fromKml()` |
| Writing | `KmlSerializer::toKml()`, called by `BookEditor::toKml()` |
| Annotations in the editor's document | `include/kalahari/editor/annotation.h` |
| Tests | `tests/editor/test_kml_load.cpp`, `test_kml_document_model.cpp`, `test_kml_serializer.cpp`, `test_editor_stage0_kml_roundtrip.cpp`, `test_annotations.cpp` |

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
  <p align="center"><b>Chapter One</b></p>
  <p>Plain text, <i>italic</i>, <b><i>bold italic</i></b> and <span color="#aa0000">red</span>.</p>
  <p>She met <charref id="r1" target="anna">Anna</charref> in <locref id="r2" target="krakow">Kraków</locref> <anchor ref="a1">on a Tuesday</anchor>.<anchor ref="a2"/></p>
</kml>
```

The editor writes KML without line breaks or indentation; the example is indented for
reading.

## Document structure

- **Root element:** `<kml>`. `<doc>` and `<document>` are accepted as well, and content that
  starts with none of them – for example a bare sequence of paragraphs – is read as if it were
  wrapped in `<kml>`. Attributes of the root element are ignored. The editor writes `<kml>`
  without attributes.
- **Paragraphs** are the children of the root element: `<p>`, or `<paragraph>` on reading.
- **Annotations** – the writer's comments, TODOs and notes – are in an `<annotations>` element,
  a child of the root element before the paragraphs (see [Annotations](#annotations)).
- Any other element at this level is skipped together with everything inside it, and text
  between paragraphs is ignored.

## Paragraph

| Attribute | Values | Meaning |
|-----------|--------|---------|
| `align` | `left`, `center`, `right`, `justify` (any letter case on reading) | The paragraph's own alignment |

A paragraph without `align` has no alignment of its own and is shown with the editor's
default: justified, with the last line aligned to the left. The editor writes `align` only
for paragraphs that have their own alignment. Other attributes are ignored and not written.

## Text

- Spaces and tabs are kept exactly as written, also at the start and end of a paragraph.
- XML entities (`&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;`) and numeric character references
  are decoded. On writing, `&`, `<`, `>`, `"` and `'` are written as entities.
- The format has no line break element. A line break in the text of a paragraph (a newline
  character or `&#10;`) starts a new paragraph, and the indentation of the next line becomes
  the start of its text.
- `<t>` and `<text>` hold plain text, which belongs to the paragraph; the editor writes the
  text without them.
- Elements inside a paragraph other than those described in this document are skipped
  together with everything inside them.

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

On writing, the style attributes go on the first formatting element of the run, for example
`<b font="Georgia"><i>text</i></b>`; a run without formatting elements gets a `<span>`.

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

- headings `<h1>`, `<h2>`, `<h3>` and the scene break `<scene-break>`,
- lists `<ul>`, `<ol>`, `<li>`,
- images `<img>`,
- tables `<table>`, `<tr>`, `<td>`, `<th>`,
- the annotations `<endnote>` and the references `<itemref>`, `<cite>`,
- the paragraph attribute `style` (paragraph styles) and the root attributes `version` and
  `lang`.

The specification's `<comment>`, `<todo>` and `<note>` elements are annotations of the
`<annotations>` section, of the kinds `comment`, `todo` and `note`.

The editor skips these elements, together with everything inside them, when it loads a
chapter, and ignores the attributes. The next save writes the chapter without them, so a file
written by hand or by another program loses them when it is saved in Kalahari.
