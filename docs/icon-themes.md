# External themes

Planetary discovers external themes in the `icon-themes` directory below
Qt's writable `QStandardPaths::AppDataLocation`. On macOS this is normally
inside the user's `~/Library/Application Support` directory. Each immediate
subdirectory is one theme and must contain a file named `theme.json`.
Standalone `*.json` manifests placed directly in `icon-themes` are also
discovered; their relative icon paths resolve from that directory.

The directory name is retained for compatibility with the first external icon
theme format. A package may now contain icons, colours, or both. Planetary
lists each component independently: an icon component appears in **Icon
theme**, while a colour component appears in **Colour scheme**. Selecting one
does not change the other.

For example:

```text
icon-themes/
  midnight/
    theme.json
    icons/
      start.svg
      stop.png
```

## Manifest format

```json
{
  "formatVersion": 1,
  "id": "midnight",
  "name": "Midnight",
  "fallback": "glass",
  "icons": {
    "action-start": "icons/start.svg",
    "action-stop": "icons/stop.png"
  },
  "colors": {
    "mode": "dark",
    "palette": {
      "window": "#101827",
      "window-text": "#eaf7ff",
      "base": "#0b1220",
      "text": "#e6f3ff",
      "button": "#172945",
      "button-text": "#eaf7ff",
      "highlight": "#137f9f",
      "highlighted-text": "#ffffff"
    },
    "semantic": {
      "download": "#20d6f2",
      "upload": "#a779ff",
      "error": "#ff6f70",
      "piece-complete": "#785de8"
    }
  }
}
```

- `formatVersion` must currently be `1`.
- `id` must begin with a lowercase letter or number and may contain lowercase
  letters, numbers, `.`, `_`, and `-`. IDs are normalized to lowercase.
- `classic`, `glass`, `system`, `light`, and `dark` are reserved for
  Planetary's built-in themes.
- `name` is the user-visible theme name.
- A manifest must contain at least one non-empty `icons` component or a
  `colors` component. Both components are optional independently.
- `fallback` is optional and defaults to `glass`. It may name another
  registered icon theme. It is ignored when the package has no icon component.
- `icons` maps semantic icon IDs to image paths relative to the directory
  containing the manifest. Absolute paths and paths outside that directory are
  rejected. Qt-supported raster and SVG formats can be used.

A theme may provide only the icons it changes. Missing, unreadable, or invalid
icons are resolved through the declared fallback, with built-in Glass always
used as the final fallback.

Planetary automatically supplies a subtle mouse-over variant for every loaded
icon. It is generated from the normal artwork at runtime using the active Qt
highlight colour and a restrained lightness lift, so theme packages do not need
duplicate hover image files. The generated variant is exposed through
`QIcon::Active` and is used by toolbar buttons, menus, and other icon-aware
widgets when they enter their active state.

## Colour themes

The optional `colors` object has three fields:

- `mode` is optional and may be `system`, `light`, or `dark`; it defaults to
  `system`. It tells Qt which native colour mode to use before palette
  overrides are applied. A system-mode theme continues to follow live macOS
  appearance changes.
- `palette` is optional and overrides standard application-wide `QPalette`
  roles. Supported names are `window`, `window-text`, `base`,
  `alternate-base`, `tool-tip-base`, `tool-tip-text`, `text`, `button`,
  `button-text`, `bright-text`, `highlight`, `highlighted-text`, `link`,
  `link-visited`, `placeholder-text`, and `accent`.
- `semantic` is optional and supplies colours for Planetary-specific visuals
  that are not adequately represented by standard palette roles. Supported
  names are `download`, `upload`, `success`, `warning`, `error`, `inactive`,
  `verification`, `queued`, `piece-complete`, `piece-remaining`, and
  `piece-border`.

Colours use any string format accepted by `QColor`, including `#RRGGBB` and
`#AARRGGBB`. Unknown role names or invalid colour values invalidate the
manifest, making spelling mistakes visible rather than silently ignoring them.
Palette and semantic entries may be omitted; Planetary then derives suitable
defaults from the active native palette.

## Semantic icon IDs

```text
action-add-torrent
action-add-magnet
action-start
action-stop
action-start-all
action-stop-all
action-force-start
action-verify
action-reannounce
action-delete
queue-top
queue-up
queue-down
queue-bottom
filter-all
filter-tracker
filter-folder
status-downloading
status-seeding
status-complete
status-active
status-inactive
status-stopped
status-error
status-verifying
status-queued
status-unknown
```

Unknown semantic IDs invalidate the manifest so spelling errors cannot silently
produce an incomplete theme.
