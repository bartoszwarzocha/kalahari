# Stan pracy - 2025-11-26

## PROBLEM DO NAPRAWIENIA

Wszystkie komponenty muszą używać WYŁĄCZNIE ArtProvider do pobierania ikon.
Bezpośrednie wywołania IconRegistry są ZABRONIONE.

## CO ZOSTAŁO ZROBIONE

1. **main_window.cpp** - NAPRAWIONE (w trakcie poprzedniej sesji)
   - Usunięto 24 wywołania `IconSet::fromRegistry()` 
   - Zamieniono na `IconSet()` (puste ikony - toolbar używa ArtProvider::createAction)

## CO POZOSTAŁO DO ZROBIENIA

### 1. Dodać metodę do ArtProvider (HEADER)

Plik: `include/kalahari/core/art_provider.h`

Po linii 121 (po metodzie `getPreviewPixmap`) dodać:

```cpp
    /// @brief Get HiDPI-aware pixmap with custom colors (Settings Dialog preview)
    QPixmap getPreviewPixmapWithColors(const QString& cmdId,
                                       int logicalSize,
                                       qreal devicePixelRatio,
                                       const QString& iconTheme,
                                       const QColor& primaryColor,
                                       const QColor& secondaryColor);
```

### 2. Dodać implementację metody (CPP)

Plik: `src/core/art_provider.cpp`

Po metodzie `getPreviewPixmap` (około linii 121) dodać:

```cpp
QPixmap ArtProvider::getPreviewPixmapWithColors(const QString& cmdId,
                                                 int logicalSize,
                                                 qreal devicePixelRatio,
                                                 const QString& iconTheme,
                                                 const QColor& primaryColor,
                                                 const QColor& secondaryColor)
{
    int physicalSize = static_cast<int>(logicalSize * devicePixelRatio);
    QIcon icon = IconRegistry::getInstance().getIconWithColors(
        cmdId, iconTheme, physicalSize, primaryColor, secondaryColor);
    QPixmap pixmap = icon.pixmap(physicalSize, physicalSize);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    return pixmap;
}
```

### 3. Naprawić settings_dialog.cpp

Plik: `src/gui/settings_dialog.cpp`

**Linia 878** - zamienić:
```cpp
// STARE:
QIcon icon = core::IconRegistry::getInstance().getIconWithColors(
    cmdId, iconTheme, physicalSize, primaryColor, secondaryColor);

// NOWE:
QPixmap pixmap = core::ArtProvider::getInstance().getPreviewPixmapWithColors(
    cmdId, logicalSize, dpr, iconTheme, primaryColor, secondaryColor);
```

**Linia 936** - zamienić:
```cpp
// STARE:
auto& sizes = core::IconRegistry::getInstance().getSizes();

// NOWE:
auto& artProvider = core::ArtProvider::getInstance();
// I potem użyć artProvider.getIconSize(IconContext::Toolbar) etc.
```

### 4. Build i test

```bash
cd /e/Python/Projekty/Kalahari
./scripts/build_windows.bat Debug
./build-windows/bin/kalahari.exe
```

## DLACZEGO TO ROBIMY

Architektura wymaga, aby WSZYSTKIE komponenty używały ArtProvider jako jedynego źródła ikon.
IconRegistry to wewnętrzna implementacja, której NIE WOLNO wywoływać bezpośrednio.

Flow: Komponent -> ArtProvider -> IconRegistry (wewnętrznie)

## UWAGI TECHNICZNE

- Narzędzia Read/Edit/Write mają problem z cache'em plików
- Użyj Bash z python3 lub sed do edycji plików
- Przed edycją zabij wszystkie procesy w tle
