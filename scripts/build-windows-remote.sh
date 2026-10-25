#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SSH_TARGET="${PLANETARY_WINDOWS_SSH_TARGET:-Mark@192.168.10.158}"
REMOTE_REPO="${PLANETARY_WINDOWS_REPO:-C:\\Users\\Mark\\Documents\\transmission-control}"

if [[ $# -ne 4 ]]; then
  echo "Usage: $0 <tag> <commit> <run-id> <local-output-dir>" >&2
  exit 2
fi
TAG="$1"
COMMIT="$2"
RUN_ID="$3"
OUTPUT_DIR="$4"

if [[ ! "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+(-beta)?$ ||
      ! "$COMMIT" =~ ^[0-9a-f]{40}$ ||
      ! "$RUN_ID" =~ ^[0-9]{8}T[0-9]{6}Z-[0-9]+$ ]]; then
  echo "Invalid release-build parameters." >&2
  exit 2
fi
if [[ "$REMOTE_REPO" =~ [[:space:]] ]]; then
  echo "PLANETARY_WINDOWS_REPO must not contain whitespace for scp compatibility." >&2
  exit 2
fi

cd "$ROOT_DIR"
if [[ -n "$(git status --porcelain)" ]]; then
  echo "Refusing to build: local checkout has changes." >&2
  exit 2
fi
git fetch --quiet --tags origin main
if [[ "$(git rev-parse "$TAG^{commit}")" != "$COMMIT" ]]; then
  echo "Local tag $TAG does not point to expected commit $COMMIT." >&2
  exit 1
fi
if ! git merge-base --is-ancestor "$COMMIT" origin/main; then
  echo "Release tag $TAG is not present on origin/main." >&2
  exit 1
fi

ps_quote() {
  local value="$1"
  value="${value//\'/\'\'}"
  printf "'%s'" "$value"
}

quoted_repo="$(ps_quote "$REMOTE_REPO")"
quoted_tag="$(ps_quote "$TAG")"
quoted_commit="$(ps_quote "$COMMIT")"
quoted_run_id="$(ps_quote "$RUN_ID")"
remote_script="$(cat <<EOF
\$ErrorActionPreference = 'Stop'
\$ProgressPreference = 'SilentlyContinue'
\$orchestrationLock = \$null
\$failed = \$false
try {
    \$repo = $quoted_repo
    \$tag = $quoted_tag
    \$commit = $quoted_commit
    \$runId = $quoted_run_id
    Set-Location -LiteralPath \$repo

    \$dirty = git status --porcelain
    if (\$LASTEXITCODE -ne 0) { throw 'Could not inspect Windows checkout status.' }
    if (\$dirty) { throw 'Windows checkout is dirty; refusing to overwrite changes.' }
    \$branch = (git branch --show-current).Trim()
    if (\$LASTEXITCODE -ne 0 -or \$branch -ne 'main') { throw 'Windows checkout must be on main.' }
    New-Item -ItemType Directory -Path (Join-Path \$repo 'dist') -Force | Out-Null
    \$orchestrationLock = [IO.File]::Open((Join-Path \$repo 'dist\\release-orchestrator.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)

    git fetch --quiet --prune --tags origin
    if (\$LASTEXITCODE -ne 0) { throw 'Could not fetch Windows origin and tags.' }
    git pull --quiet --ff-only origin main
    if (\$LASTEXITCODE -ne 0) { throw 'Could not fast-forward Windows main from origin/main.' }
    \$remoteCommit = (git rev-parse "\$tag^{commit}").Trim()
    if (\$LASTEXITCODE -ne 0 -or \$remoteCommit -ne \$commit) { throw "Windows tag \$tag does not resolve to expected commit \$commit." }
    git merge-base --is-ancestor \$commit origin/main
    if (\$LASTEXITCODE -ne 0) { throw "Windows tag \$tag is not on origin/main." }

    \$checkoutChanged = \$false
    try {
        git checkout --quiet --detach \$commit
        if (\$LASTEXITCODE -ne 0) { throw 'Could not check out the release commit on Windows.' }
        \$checkoutChanged = \$true

        \$releaseScript = Join-Path \$repo 'scripts\\windows-release.ps1'
        \$signingConfig = Join-Path \$repo 'scripts\\windows-release.local.json'
        \$arguments = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', \$releaseScript)
        if (Test-Path -LiteralPath \$signingConfig -PathType Leaf) { \$arguments += @('-SigningConfig', \$signingConfig) }
        & powershell.exe @arguments
        if (\$LASTEXITCODE -ne 0) { throw 'Windows release build/package failed.' }

        \$dist = Join-Path \$repo 'dist'
        \$version = (Get-Content -LiteralPath (Join-Path \$repo 'build-release\\llvm-mingw\\planetary-version.txt') -Raw).Trim()
        \$expectedVersion = \$tag -replace '^v', '' -replace '-beta$', ''
        if (\$version -ne \$expectedVersion) { throw "Windows version \$version does not match tag version \$expectedVersion." }
        \$artifactDir = Join-Path \$dist "release-results-\$runId"
        if (Test-Path -LiteralPath \$artifactDir) { throw "Release result directory already exists: \$artifactDir" }
        New-Item -ItemType Directory -Path \$artifactDir | Out-Null
        \$files = @("Planetary-\$version-Windows-x64-Setup.exe", 'Planetary-Windows-hashes.json')
        foreach (\$file in \$files) {
            \$source = Join-Path \$dist \$file
            if (-not (Test-Path -LiteralPath \$source -PathType Leaf)) { throw "Expected Windows artifact is missing: \$source" }
            Copy-Item -LiteralPath \$source -Destination \$artifactDir
        }
        @("tag=\$tag", "commit=\$commit", "version=\$version") |
            Set-Content -LiteralPath (Join-Path \$artifactDir 'Windows-build-info.txt') -Encoding ascii
        Write-Output "WINDOWS_RELEASE_ARTIFACT_DIR=\$artifactDir"
        Write-Output "Windows release complete: Planetary \$version (\$tag / \$commit)"
    } finally {
        if (\$checkoutChanged) {
            git checkout --quiet main
            if (\$LASTEXITCODE -ne 0) { throw 'Build finished but Windows checkout could not be restored to main.' }
        }
    }
} catch {
    Write-Error \$_ -ErrorAction Continue
    \$failed = \$true
} finally {
    if (\$orchestrationLock) { \$orchestrationLock.Dispose() }
}
if (\$failed) { exit 1 }
EOF
)"
mkdir -p "$OUTPUT_DIR"
log_path="$OUTPUT_DIR/remote-build.log"
if ! printf '%s\n' "$remote_script" |
    ssh -T -o BatchMode=yes "$SSH_TARGET" \
      powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
      -OutputFormat Text -Command - \
      2>&1 | tee "$log_path"; then
  echo "Remote Windows build failed; see $log_path" >&2
  exit 1
fi

VERSION="${TAG#v}"
VERSION="${VERSION%-beta}"
REMOTE_DIST="${REMOTE_REPO//\\//}/dist/release-results-$RUN_ID"
for artifact in \
  "Planetary-$VERSION-Windows-x64-Setup.exe" \
  "Planetary-Windows-hashes.json" \
  "Windows-build-info.txt"; do
  scp -O "$SSH_TARGET:$REMOTE_DIST/$artifact" "$OUTPUT_DIR/"
done
if ! grep -Fqx "tag=$TAG" "$OUTPUT_DIR/Windows-build-info.txt" ||
   ! grep -Fqx "commit=$COMMIT" "$OUTPUT_DIR/Windows-build-info.txt"; then
  echo "Windows build metadata does not match requested tag and commit." >&2
  exit 1
fi
echo "Windows artifacts copied to $OUTPUT_DIR"
