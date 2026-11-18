# Texts for Publishers – the views

The core, the templates and the settings came in the draft PR #148. The mockup
`teksty-dla-wydawnictwa.html` asked three questions; the answers (17.11) are its recommendations:

- **a)** the texts are a kind of the Materials, beside the Cover: one item with its number in the
  Navigator; its list has the texts in groups by what they are, and the set as a second view;
- **b)** a synopsis is written in the editor, as a chapter (spelling, grammar, styles, versions,
  statistics); a sentence about the book is written on its card;
- **c)** the points of the template of a synopsis are beside the text, in the Properties panel, by
  default; the skeleton in the text is the other choice of Settings.

The work comes in three pull requests, each complete on its own.

## Part 1 – the texts

1. **The kind.** `kalahari.base:publishing`: a text kind of the Workshop (`workshopGroup`
   `resources`), after the Cover in the list of the Workshop, icon `material.publishing`
   (`outbox`); the words of its name in Polish and English. Every text has its `.kchapter` file:
   the text of a synopsis or of a sentence is the text of the element.
2. **Adding.** `ProjectManager::addPublishingText(part, templateReference, title)`: a text of the
   Workshop with its `publishing` data, one save. The Add Element window, for the kind: the part
   (a sentence about the book, a synopsis; a cover description and a tagline go to the Cover as
   materials of the cover) and the template of the part, with "No template"; the title of a new
   text is "<part> – version <n>". The part and the template chosen last are kept.
3. **The list** (MaterialListPanel, MaterialList): the columns Text or Title, Template, State,
   Length (with its target, in the unit of the Cover) and Changed; the texts in groups by part
   while Group by Kind is down, the filter Kind, the button From Cover that adds the cover
   descriptions and the taglines (rows marked "from the Cover"); the menu of a row: Open, Choose
   for the Set, Reject or Restore as a Draft, Copy Text, Show in the Cover. The gear opens
   Settings > Navigator > Texts for Publishers.
4. **A sentence about the book** opens on a tab of its own (PitchPanel, an EditorPanel like
   CardPanel): the template and the fields of its pattern, the sentence in the editor under them
   (composed from the fields until the writer changes it by hand; "Compose from the Fields"
   composes it again), its length with the target, the state and its own target.
5. **A synopsis** opens in the editor. The Properties panel shows, beside the chapter page, the
   box of the text (PublishingTextBox): its length with the target and the standard pages, the
   template and its points with check boxes (Space), the state and its own target. Tools >
   Texts for Publishers > Template Points gives the keys to the points; Esc gives them back.
6. Tests, the Polish translation, the narrow-window checks, screenshots.

## Part 2 – the set

The second view of the list (the button Set): the parts of the set one under another, each with
its version (a list of the versions), its length, Copy and Open; Properties: the contents of the
set (check boxes, Space; Alt+Up and Alt+Down move a part), the part chosen, Copy All and Export
Set.... Export Book gets the set as its fourth source (DOCX and Markdown with the headings of the
parts).

## Part 3 – templates and the skeleton

The window Templates of Texts for Publishers (the templates of the book's type, read only, and
the writer's own: new, copy, delete, name, part, types, target, points or pattern and fields).
The skeleton in the text: Insert Skeleton puts the headings of the points (style Template Point)
with grey hints that go when the writer types; Remove Headings and Hints takes them out; the
counter, the set, copying and export leave them out. Settings > Navigator > Texts for Publishers
gets the choice of where the points are.
