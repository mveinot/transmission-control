# Linux release build container

This image provides an Ubuntu 22.04 x86_64 build environment with Qt 6.12.0,
CMake, Ninja, and the system development packages required by Planetary. Qt is
installed from the official Qt archive using the pinned `aqtinstall` version.

The image is intended to be built and run on the configured Ubuntu Docker host. From
a checkout of Planetary on that host, run:

```sh
./scripts/build-release-linux-docker.sh
```

The script builds the image, compiles Planetary into `build-linux-docker/`, and
packages the result as an AppImage in `dist/`. It uses the checkout's Git
metadata so the normal four-part application version is retained. The build
directory is kept between invocations for incremental builds. The first run
downloads the official linuxdeploy and Qt plugin AppImages into the ignored
`tools/` directory; subsequent runs reuse them.

The image itself is pinned to Ubuntu 22.04, Qt 6.12.0, aqtinstall 3.3.0, and
CMake 3.31.6. Ubuntu's package repository supplies the compiler/system packages. The
resulting x86_64 AppImage uses Ubuntu 22.04's glibc baseline and is intended for
Ubuntu 22.04 and newer. Packaging uses
the project's `scripts/package-linux-appimage.sh` workflow inside the container.

This is a build environment, not a Windows cross-compiler. Windows packaging
continues to use the native Windows release setup documented under
`packaging/windows/`.
