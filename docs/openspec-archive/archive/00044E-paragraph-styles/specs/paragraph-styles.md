## ADDED Requirements

### Requirement: Built-in Paragraph Styles
The StyleResolver SHALL provide built-in paragraph style presets (Heading 1-3, Body, Quote, Code) that resolve without database.

#### Scenario: Apply Heading 1
- **WHEN** user applies Heading 1 style to paragraph
- **THEN** paragraph is 24pt, bold, with appropriate spacing

### Requirement: Paragraph Style Dropdown
The toolbar SHALL provide a paragraph style dropdown for applying built-in styles.

#### Scenario: Select heading style
- **WHEN** user picks "Heading 1" from style dropdown
- **THEN** current paragraph reformats to Heading 1 preset

### Requirement: Paragraph Style Serialization
The KML format SHALL serialize paragraph style as `style` attribute on `<p>` elements.

#### Scenario: Save styled paragraph
- **WHEN** paragraph has style "heading1"
- **THEN** KML output is `<p style="heading1">`
