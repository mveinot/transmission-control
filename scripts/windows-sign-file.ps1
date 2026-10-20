param([Parameter(Mandatory=$true)][string]$FilePath)
$ErrorActionPreference = 'Stop'
$thumbprint = ($env:PLANETARY_SIGN_THUMBPRINT -replace '\s', '').ToUpperInvariant()
if ($thumbprint -notmatch '^[0-9A-F]{40}$') {
    throw 'Set PLANETARY_SIGN_THUMBPRINT to the 40-character thumbprint of a certificate in CurrentUser\My.'
}

$certificate = Get-ChildItem -LiteralPath "Cert:\CurrentUser\My\$thumbprint" -ErrorAction SilentlyContinue
if (-not $certificate -or -not $certificate.HasPrivateKey) {
    throw "Signing certificate $thumbprint with a private key was not found in CurrentUser\My."
}
if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
    throw "File to sign was not found: $FilePath"
}

$signTool = $env:PLANETARY_SIGNTOOL
if (-not $signTool) {
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $signTool = Get-ChildItem -LiteralPath $sdkRoot -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $signTool -or -not (Test-Path -LiteralPath $signTool -PathType Leaf)) {
    throw 'SignTool was not found. Set PLANETARY_SIGNTOOL to the Windows SDK x64 signtool.exe path.'
}

& $signTool sign /fd SHA256 /sha1 $thumbprint /s My $FilePath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
