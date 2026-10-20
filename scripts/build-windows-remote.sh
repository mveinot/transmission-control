#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
remote_target="${PLANETARY_WINDOWS_SSH_TARGET:-}"
remote_repo="${PLANETARY_WINDOWS_REPO:-C:\\Users\\Mark\\Documents\\transmission-control}"
sign_thumbprint="${PLANETARY_WINDOWS_SIGN_THUMBPRINT:-}"

if [[ -z "$remote_target" ]]; then
    echo "Set PLANETARY_WINDOWS_SSH_TARGET (for example Mark@192.168.10.158)." >&2
    exit 2
fi
if [[ -n "$sign_thumbprint" && ! "$sign_thumbprint" =~ ^[[:xdigit:]]{40}$ ]]; then
    echo "PLANETARY_WINDOWS_SIGN_THUMBPRINT must be a 40-character certificate thumbprint." >&2
    exit 2
fi

cd "$repo_root"
if [[ -n "$(git status --porcelain)" ]]; then
    echo "Refusing to build: commit or stash local changes first." >&2
    exit 2
fi

commit="$(git rev-parse HEAD)"
git fetch --quiet origin main
if ! git merge-base --is-ancestor "$commit" origin/main; then
    echo "Refusing to build $commit: it is not present on origin/main yet." >&2
    exit 2
fi

ps_quote() {
    local value="$1"
    value="${value//\'/\'\'}"
    printf "'%s'" "$value"
}

quoted_repo="$(ps_quote "$remote_repo")"
quoted_commit="$(ps_quote "$commit")"
quoted_thumbprint="$(ps_quote "${sign_thumbprint^^}")"
remote_script="$(cat <<EOF
\$ErrorActionPreference = 'Stop'
\$ProgressPreference = 'SilentlyContinue'
try {
    \$repo = $quoted_repo
    \$commit = $quoted_commit
    \$thumbprint = $quoted_thumbprint
    Set-Location -LiteralPath \$repo

    \$dirty = git status --porcelain
    if (\$LASTEXITCODE -ne 0) { throw 'Could not inspect remote checkout status.' }
    if (\$dirty) { throw 'Remote checkout has changes; refusing to overwrite them.' }
    git fetch --quiet origin main
    if (\$LASTEXITCODE -ne 0) { throw 'Could not fetch origin/main on Windows.' }
    git cat-file -e "\$commit^{commit}"
    if (\$LASTEXITCODE -ne 0) { throw 'Requested source commit is not available on Windows.' }
    git checkout --quiet --detach \$commit
    if (\$LASTEXITCODE -ne 0) { throw 'Could not check out requested source commit.' }

    \$env:PLANETARY_SIGN_THUMBPRINT = \$thumbprint
    & (Join-Path \$repo 'scripts\\windows-release.ps1')
    if (\$LASTEXITCODE -ne 0) { throw 'Windows release build/package failed.' }

    \$dist = Join-Path \$repo 'dist'
    \$version = (Get-Content -LiteralPath (Join-Path \$repo 'build-release\\llvm-mingw\\planetary-version.txt') -Raw).Trim()
    \$installer = Join-Path \$dist "Planetary-\$version-Windows-x64-Setup.exe"
    if (-not (Test-Path -LiteralPath \$installer -PathType Leaf)) { throw "Expected installer is missing: \$installer" }

    \$record = Join-Path \$dist 'Planetary-Windows-build.txt'
    @("commit=\$(git rev-parse HEAD)", "version=\$version", "thumbprint=\$thumbprint", "signed=\$([bool]\$thumbprint)", "qt=6.12.0") |
        Set-Content -LiteralPath \$record -Encoding ascii
    Get-FileHash -LiteralPath \$installer -Algorithm SHA256 |
        ForEach-Object { Add-Content -LiteralPath \$record -Value ("sha256=\$([IO.Path]::GetFileName(\$_.Path)) \$($_.Hash)") -Encoding ascii }
    Write-Output "Windows release complete: Planetary \$version (\$commit)"
} catch {
    Write-Error \$_
    exit 1
}
EOF
)"
encoded_script="$(printf '%s' "$remote_script" | iconv -f UTF-8 -t UTF-16LE | base64 | tr -d '\r\n')"

output_dir="$repo_root/dist/windows"
mkdir -p "$output_dir"
log_path="$output_dir/remote-build.log"
if ! ssh -o BatchMode=yes "$remote_target" powershell.exe -NoLogo -NoProfile -NonInteractive -OutputFormat Text -EncodedCommand "$encoded_script" 2>&1 | tee "$log_path"; then
    echo "Remote Windows build failed; see $log_path" >&2
    exit 1
fi

remote_path="${remote_repo//\\//}/dist"
scp -O "$remote_target:$remote_path/Planetary-Windows-build.txt" "$output_dir/"
version="$(sed -n 's/^version=//p' "$output_dir/Planetary-Windows-build.txt" | head -n 1)"
if [[ -z "$version" ]]; then
    echo "Build record did not contain a version." >&2
    exit 1
fi
scp -O "$remote_target:$remote_path/Planetary-$version-Windows-x64-Setup.exe" "$output_dir/"

echo "Artifacts copied to $output_dir"
