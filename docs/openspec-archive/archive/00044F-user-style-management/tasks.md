# Tasks for #00044F: User Style Management

## 1. Database
- [ ] 1.1 Seed built-in styles in paragraph_styles/character_styles tables on project creation
- [ ] 1.2 Save/load user styles via ProjectDatabase API

## 2. StyleResolver
- [ ] 2.1 Implement resolveForTextRun() — merge paragraph + character + inline overrides with priority

## 3. Dialogs
- [ ] 3.1 CreateStyleDialog — capture selection formatting, naming, save to DB
- [ ] 3.2 EditStyleDialog — edit font/size/bold/italic/color/spacing properties

## 4. StylePalette Widget
- [ ] 4.1 Create StylePalette — list built-in + user styles, click to apply
- [ ] 4.2 Right-click context menu: edit/delete user styles
- [ ] 4.3 Integrate StylePalette in sidebar or floating panel

## 5. Integration
- [ ] 5.1 Connect format.style.manage command → open StylePalette

## 6. Testing
- [ ] 6.1 Integration tests: create → apply → save project → reload → verify style persists
