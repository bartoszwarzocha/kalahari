## ADDED Requirements

### Requirement: Text Color
The BookEditor SHALL allow setting text foreground color on selection or as pending format.

#### Scenario: Color selected text
- **WHEN** user selects text and picks red from color picker
- **THEN** selected text foreground becomes red

### Requirement: Background Highlight Color
The BookEditor SHALL allow setting text background (highlight) color.

#### Scenario: Highlight text
- **WHEN** user selects text and picks yellow highlight
- **THEN** selected text background becomes yellow

### Requirement: Color Picker Widget
The toolbar SHALL provide a color picker popup with common colors and custom color option.

#### Scenario: Pick custom color
- **WHEN** user clicks "Custom..." in color picker
- **THEN** QColorDialog opens for custom color selection
