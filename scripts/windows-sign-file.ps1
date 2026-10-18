param([Parameter(Mandatory=$true)][string]$FilePath)
$ErrorActionPreference = 'Stop'
if (-not $env:PLANETARY_SIGNTOOL -or -not $env:PLANETARY_SIGN_PFX -or -not $env:PLANETARY_SIGN_PASSWORD) {
    throw 'Set PLANETARY_SIGNTOOL, PLANETARY_SIGN_PFX and PLANETARY_SIGN_PASSWORD before signing.'
}
& $env:PLANETARY_SIGNTOOL sign /fd SHA256 /f $env:PLANETARY_SIGN_PFX /p $env:PLANETARY_SIGN_PASSWORD $FilePath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
