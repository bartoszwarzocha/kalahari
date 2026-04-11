## ADDED Requirements

### Requirement: KML Inline Style Attributes
The KML serializer SHALL emit font, size, color, and bg attributes on formatting tags and `<span>` elements when inline style overrides differ from defaults.

#### Scenario: Bold text with custom font
- **WHEN** text has bold formatting and font "Courier New"
- **THEN** KML output is `<b font="Courier New">text</b>`

#### Scenario: Text with color but no formatting tags
- **WHEN** text has red color but no bold/italic/underline/strikethrough
- **THEN** KML output is `<span color="#FF0000">text</span>`

### Requirement: KML Paragraph Alignment Attribute
The KML serializer SHALL emit `align` attribute on `<p>` elements when alignment differs from left.

#### Scenario: Centered paragraph
- **WHEN** paragraph has center alignment
- **THEN** KML output is `<p align="center">`

### Requirement: KML Backward Compatibility
The KML parser SHALL load files without inline style attributes without errors.

#### Scenario: Old KML file
- **WHEN** KML file has `<b>text</b>` without font/size/color attributes
- **THEN** text loads with default font, size, and color
