# Building a tagged release on all platforms

From a clean, up-to-date `main` checkout on the Mac, run the orchestrator with
the exact tag created by `scripts/tag-release.sh`:

```sh
scripts/build-release-all.sh v2.5.3.485
```

To retry selected platforms without rebuilding completed ones, pass one or more
skip flags before the tag. For example, to retry only Windows:

```sh
scripts/build-release-all.sh --skip-macos --skip-linux v2.6.0.490
```

The available flags are `--skip-macos`, `--skip-windows`, and `--skip-linux`.
At least one platform must remain selected. Each invocation gets a new output
directory; skipped platforms are omitted from that run's artifact folders and
combined checksum list.

Use the `-beta` suffix for beta tags. The script verifies that the tag is pushed
to `origin`, resolves to the same commit locally and remotely, and belongs to
`origin/main`. It refuses a dirty or out-of-sync Mac checkout. It builds the
macOS package from a temporary worktree at that exact tag, and fetches/pulls on
the Windows and Linux hosts before verifying and building the same commit.
Remote checkouts must be clean and on `main`; after each build they are returned
to `main`.

The build runs in order: macOS universal DMG (including signing and
notarization), Windows installer (signed using the Windows machine-local
signing configuration), then Linux x86_64 AppImage using Docker
on the Ubuntu 24.04 host. The script does not create a GitHub release or upload
anything to GitHub or the website.

Successful artifacts, the Windows hash manifest, build metadata, logs, and a
combined SHA-256 list are collected under:

```text
dist/release-builds/<tag>/<UTC-run-id>/
```

If one platform fails, the output directory and logs are retained for diagnosis;
the checksums file is written only after all selected builds succeed. Reruns
use a new run-id directory and do not overwrite previous attempts.

The current machine defaults are:

- Windows SSH host: `Mark@192.168.10.158`
- Windows checkout: `C:\Users\Mark\Documents\transmission-control`
- Linux SSH host: `docker`
- Linux checkout: `/home/vmark/planetary-release-checkout` (cloned from the
  public GitHub repository on first use)

Override these with `PLANETARY_WINDOWS_SSH_TARGET`, `PLANETARY_WINDOWS_REPO`,
`PLANETARY_LINUX_SSH_TARGET`, and `PLANETARY_LINUX_REPO`. The Linux clone URL
can be overridden with `PLANETARY_RELEASE_GIT_URL`. `PLANETARY_RELEASE_OUTPUT_ROOT`
changes the local artifact root, and `PLANETARY_MACOS_BUILD_DIR` changes the
relative macOS build directory used inside the temporary worktree.

macOS signing and notarization still require the local keychain identity and
notary profile documented in `scripts/package-macos.sh`. Windows signing uses
the ignored, machine-local `scripts/windows-release.local.json`; private keys
and passwords are neither copied to the Mac nor committed. The current Windows
certificate is self-signed, so the installer is signed but not publicly trusted
by Windows.

The Linux host needs passwordless SSH access, Git, Docker access for the SSH
user, and network access to fetch the public repository and Docker build
dependencies. Both remote hosts need Git and access to `origin`.

Release builds fetch the public repository over HTTPS by default to avoid
depending on GitHub SSH availability; set `PLANETARY_RELEASE_GIT_URL` to
override that URL. The normal Git remote remains unchanged, so this does not
change how release tags are pushed.
