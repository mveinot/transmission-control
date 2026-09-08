# Bundled C dependencies

Planetary builds libmaxminddb and miniz directly from pinned source snapshots
under `third_party/`. They are always static targets and therefore inherit the
active compiler, architecture, runtime, SDK, and deployment target. No
Homebrew, vcpkg, or separately prepared library prefix is required.

| Component | Version | CMake target |
| --- | --- | --- |
| libmaxminddb | 1.13.3 | `Planetary::maxminddb` |
| miniz | 3.1.2 | `Planetary::miniz` |

Each component directory contains an `UPSTREAM.md` recording its tag, source
archive checksum, and import date. To update a component:

1. Download an official tagged source archive and verify its provenance.
2. Replace the vendored source snapshot without reformatting it.
3. Retain the upstream license and notice files.
4. Update the version, tag, SHA-256, and import date in `UPSTREAM.md` and the
   version used by `third_party/CMakeLists.txt`.
5. Build and test on every supported platform. For macOS, also verify both
   slices and their deployment targets.

Planetary-specific changes should preferably stay in the parent CMake file or
small separate patches. If an upstream source file must be changed, document
the modification prominently as required by its license.
