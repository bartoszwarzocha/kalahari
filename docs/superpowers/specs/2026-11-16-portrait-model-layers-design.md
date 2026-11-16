# Portraits of parts: a set of rendered layers (the style "Model")

Status: format agreed for the work on the set; the program does not read it yet.
Stage 6 of the cards ships the portraits of parts in two drawn styles, Flat and Line, from
`resources/portraits/drawn.json` (`gui::AvatarArt`). This document fixes the format of a third
set: layers rendered in Blender, as PNG pictures, that the program puts together and tints in
the colors of the portrait. The set is made apart from the program (on the writer's computer,
with the Blender add-on); once it is in the repository, the program gets the style "Model" next
to Flat and Line (Settings > Cards > Portraits of Parts and the window Portrait).

## 1. What stays the same

- The parts, their values and the colors are those of `gui::Avatar` (section 7). A portrait
  saved in a book does not change: the style is a property of the portrait (`style`), the
  choices are the same JSON object for every style.
- The rules of a layer are those of `drawn.json`: the order of drawing, `when` (the choices it
  is drawn for, `{"not": [...]}`, a value of `extras` matches when the list has it), `frames`
  (`bust`, `full`, `face`; none: all), `fit` and `fixed`.
- The program draws the background, the shadow on the ground and the sign of the gender itself,
  for every style (the background layers of `drawn.json` are shared). The set draws only the
  figure.
- A value no layer is drawn for is a mistake of the set: the test "the drawings of the program
  have every value of the parts" checks `drawn.json` and will check this set too.

## 2. Files

```
resources/portraits/model/
  set.json            the layers, in the order they are drawn
  LICENSES.md         where each model and texture comes from, and its license
  head/oval.png       the pictures, one folder per part, lowercase, "-" between words
  hair/long-round-back.png
  ...
```

The pictures are PNG, 8 bits, cropped to what they show:

- a layer that takes a color of the portrait: grey with alpha (PNG "LA"), the shading of a
  white material (section 4);
- a layer of its own colors (the whites of the eyes, a steel armor, gold trims): RGBA.

Together the set should stay under 30 MB in the installer; the pictures go through a lossless
optimizer (e.g. `oxipng -o 4`).

## 3. `set.json`

```json
{
  "format": 1,
  "kind": "raster",
  "comment": "Rendered in Blender 4.x, orthographic camera from the front",
  "frames": {"bust": [0, 0, 200, 200], "full": [-110, -26, 420, 600], "face": [60, 64, 80, 80]},
  "axis": 100,
  "ground": 556,
  "density": 4,
  "fit": {"height": {"part": "height", "values": {"short": 0.9, "medium": 1, "tall": 1.08}}},
  "overrides": [{"part": "clothes", "is": "armor", "color": "clothes", "value": "#9aa3ad"}],
  "layers": [
    {"when": {"face": "oval"}, "frames": ["bust", "face"],
     "image": "head/oval.png", "rect": [58, 52, 84, 96], "tint": "{skin}"},
    {"when": {"face": "oval"}, "frames": ["bust", "face"],
     "image": "head/oval-shine.png", "rect": [58, 52, 84, 96], "blend": "screen",
     "opacity": 0.6},
    {"when": {"eyes": "round"}, "frames": ["bust", "face"],
     "image": "eyes/round-white.png", "rect": [76, 92, 48, 16]},
    {"when": {"eyes": "round"}, "frames": ["bust", "face"],
     "image": "eyes/round-iris.png", "rect": [79, 94, 42, 12], "tint": "{eyes}"}
  ]
}
```

- `frames`, `axis`, `ground`: the units of the set and what each frame shows, as in
  `drawn.json`. The set may keep the units of `drawn.json` (recommended: the tiles and the
  preview then frame it the same way) or have its own.
- `density`: pixels of the pictures per unit. 4 gives the bust 800 pixels, as large as the
  picture saved in the materials (`PortraitDialog::pictureOf()`); the whole figure is drawn
  smaller, so its pictures need no more.
- `fit`: only the factors the set wants. A rendered model looks wrong stretched sideways, so the
  set renders the face shapes and the builds (section 5) and uses `fit` for the height alone
  (the whole figure, around `ground`, all but the `fixed` layers).
- A layer: `image` (a path in the set), `rect` (where the picture goes, in units: x, y, width,
  height), and:
  - `tint`: a color of the portrait as in `drawn.json` (`{skin}`, `{hair-10}`,
    `{clothes>a03a35@60}`); none: the picture keeps its colors;
  - `blend`: `multiply` (the default with `tint`), `normal` (the default without it) or
    `screen` (highlights);
  - `opacity`: 0 to 1, by default 1.

## 4. Tinting

A tinted layer is rendered with a white, matte material, so its picture holds the light alone:
white where the light falls, grey in the shade. The program multiplies the color of the
portrait by it, pixel by pixel, and keeps the alpha of the picture: a pale skin, a green skin of
an elf and a navy coat all get the same shading. The shine (the light on the hair, on the lips,
on a steel armor) goes in a layer of its own with `"blend": "screen"`, without a tint, so a dark
color still shines.

In Blender: the Diffuse light passes (Direct + Indirect) of a white material give the grey
picture; the Glossy passes give the shine. Colors that never change (the whites of the eyes,
gold, metal of the glasses) are rendered in their colors, RGBA, without `tint`.

## 5. What is rendered

One camera for the whole set: orthographic, from the front, at the height of the eyes, the
model standing on the ground. The parts that touch each other are rendered against each other
(a hold-out of the head under the hair), so the layers fit without seams.

| Part | Values | Rendered |
|------|--------|----------|
| face (head and neck) | oval, round, square, long, heart | each shape |
| age | young, adult, middle, old | the wrinkles as layers over each shape (`"blend": "multiply"`, grey) |
| eyes, lashes, brows, nose, mouth | section 7 | each value, on the oval head; the other shapes reuse them with a `rect` of their own |
| hair | short, fringe, shoulder, long, bun, curly, bald | each value on each face shape (`when` with `face`), the hair behind the head and in front of it as two layers |
| beard | none, stubble, beard, moustache, full | each value on each face shape |
| glasses, hat, extras | section 7 | each value; the hat and the hood on each face shape |
| clothes, bottom | section 7 | each value on each build (`when` with `build`), the bust and the whole figure apart |
| build | slim, average, strong | the body under the clothes, each build |
| height | short, medium, tall | not rendered: `fit` |

That is about 400 pictures: most of them are small (an eye, a brow) and only the hair and the
clothes are large.

## 6. What the add-on hands over

- `set.json` written by the render script from a table of the layers (part, value, conditions,
  the collections of the scene it shows), not by hand;
- the pictures, cropped, with their `rect` computed from the camera;
- `LICENSES.md`: own models, or bases under CC0 only (e.g. MakeHuman), each with its source;
- one picture of each kind of portrait put together by the script (a woman, a man, an elf in a
  robe, an old man in armor), to compare with the program's.

## 7. The parts and their values

| Part (key) | Values | Default |
|------------|--------|---------|
| face | oval, round, square, long, heart | oval |
| age | young, adult, middle, old | adult |
| eyes | round, almond, narrow, big | round |
| lashes | plain, long | plain |
| brows | straight, arched, thick, raised, frowning | straight |
| nose | small, straight, aquiline, wide | straight |
| mouth | neutral, serious, sad, surprised, smile | neutral |
| hair | short, fringe, shoulder, long, bun, curly, bald | short |
| beard | none, stubble, beard, moustache, full | none |
| glasses | none, round, rectangular, dark, monocle | none |
| hat | none, hat, hood, diadem | none |
| extras (several) | freckles, blush, mole, scar, earrings, eyePatch | – |
| clothes | shirt, sweater, coat, robe, armor, dress | shirt |
| bottom (whole figure) | trousers, skirt | trousers |
| build (whole figure) | slim, average, strong | average |
| height (whole figure) | short, medium, tall | medium |

The colors: `{skin}`, `{eyes}`, `{hair}` (also the beard and the brows), `{clothes}`, `{bottom}`;
the armor is steel (`overrides`). A robe and a dress cover the bottom.

## 8. The program's side, once the set is in the repository

- `AvatarLook::MODEL` ("model") and a loader of raster layers in `AvatarArt` (a layer with
  `image` is a picture, tinted with `QPainter::CompositionMode_Multiply` over the color and cut
  to its alpha); the same problems reported as for `drawn.json`.
- The style "Model" in Settings > Cards > Portraits of Parts and in the window Portrait, shown
  only when the set is installed; a portrait in the style Model opened without the set is drawn
  Flat.
- Tests: every value of every part has a layer, every picture exists and lies in its frame,
  a tinted layer is grey, the window still fits 1366x768 at 150%.
