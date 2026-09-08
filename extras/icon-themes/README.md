# Example external themes

This directory contains installable Planetary theme packages. Copy any whole
subdirectory into the external theme directory documented in
[`docs/icon-themes.md`](../../docs/icon-themes.md), then open Settings so
Planetary rescans it.

Themes may also be installed as ZIP-compatible files with a
`.planetarytheme` extension. Planetary indexes these packs in place and
extracts their assets to its cache only when they are selected.

Icon and colour components are independent. All packages below are colour-only
except Polar Night, Celestial Enamel, and Orbital Console, which also supply
icons, so any colour scheme can be paired with Glass, Classic, Polar Night,
Celestial Enamel, or Orbital Console icons.

## Included colour schemes

| Planetary theme | Palette source |
| --- | --- |
| Solarized Dark and Solarized Light | [Solarized by Ethan Schoonover](https://github.com/altercation/solarized) |
| Gruvbox Dark and Gruvbox Light | [Gruvbox by Pavel Pertsev](https://github.com/morhetz/gruvbox) |
| Nord | [Nord colours and palettes](https://www.nordtheme.com/docs/colors-and-palettes/) |
| Dracula | [Dracula specification](https://spec.draculatheme.com/) |
| Catppuccin Mocha | [Catppuccin palette](https://github.com/catppuccin/catppuccin) |
| Rosé Pine | [Rosé Pine palette](https://github.com/rose-pine/rose-pine-palette) |
| Tokyo Night Storm | [Tokyo Night](https://github.com/folke/tokyonight.nvim) |
| One Dark | [One Dark](https://github.com/joshdick/onedark.vim) |
| Celestial Enamel | Original indigo, brass, and jewel-toned observatory palette |
| Orbital Console | Retro-futurist amber, magenta, and copper console palette |

The original palettes provide the colour vocabulary. Their mapping onto Qt's
application palette and Planetary's torrent-specific semantic roles is a
Planetary adaptation.
