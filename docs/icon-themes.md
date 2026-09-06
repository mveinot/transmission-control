# External icon themes

Planetary discovers external icon themes in the `icon-themes` directory below
Qt's writable `QStandardPaths::AppDataLocation`. On macOS this is normally
inside the user's `~/Library/Application Support` directory. Each immediate
subdirectory is one theme and must contain a file named `theme.json`.

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
  }
}
```

- `formatVersion` must currently be `1`.
- `id` must begin with a lowercase letter or number and may contain lowercase
  letters, numbers, `.`, `_`, and `-`. IDs are normalized to lowercase.
- `classic` and `glass` are reserved for Planetary's built-in themes.
- `name` is the user-visible theme name.
- `fallback` is optional and defaults to `glass`. It may name another
  registered theme.
- `icons` maps semantic icon IDs to image paths relative to the directory
  containing the manifest. Absolute paths and paths outside that directory are
  rejected. Qt-supported raster and SVG formats can be used.

A theme may provide only the icons it changes. Missing, unreadable, or invalid
icons are resolved through the declared fallback, with built-in Glass always
used as the final fallback.

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
