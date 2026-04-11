## ADDED Requirements

### Requirement: Toolbar Format Sync
The toolbar SHALL reflect the actual formatting at the current cursor position.

#### Scenario: Cursor in bold text
- **WHEN** cursor moves into bold text
- **THEN** Bold button shows checked/active state

#### Scenario: Cursor in 16pt text
- **WHEN** cursor moves into text with font size 16
- **THEN** Font size dropdown shows "16"

### Requirement: Font Size Dropdown
The font size control SHALL be a dropdown with preset sizes and editable custom input.

#### Scenario: Select preset size
- **WHEN** user picks "24" from font size dropdown
- **THEN** selected text changes to 24pt
