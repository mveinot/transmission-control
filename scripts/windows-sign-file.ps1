param(
    [Parameter(Mandatory=$true)][string]$FilePath,
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$signTool = $env:PLANETARY_SIGNTOOL
if (-not $signTool) {
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $signTool = Get-ChildItem -LiteralPath $sdkRoot -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
        Sort-Object { [version]$_.Directory.Parent.Name } -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $signTool -or -not (Test-Path -LiteralPath $signTool -PathType Leaf)) {
    throw 'SignTool not found. Set PLANETARY_SIGNTOOL to the SDK x64 signtool.exe.'
}
$thumbprint = ($env:PLANETARY_SIGN_THUMBPRINT -replace '\s', '').ToUpperInvariant()
$password = $env:PLANETARY_SIGN_PASSWORD
if ($env:PLANETARY_SIGN_PASSWORD_FILE) {
    $password = (Get-Content -LiteralPath $env:PLANETARY_SIGN_PASSWORD_FILE -Raw).TrimEnd("`r", "`n")
}
$arguments = @('sign', '/debug', '/fd', 'SHA256')
if ($env:PLANETARY_SIGN_PFX) {
    if (-not (Test-Path -LiteralPath $env:PLANETARY_SIGN_PFX -PathType Leaf)) { throw 'Signing PFX not found.' }
    # Validate the password and certificate without importing a persistent key.
    $certificate = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2(
        $env:PLANETARY_SIGN_PFX, $password,
        [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet)
    $arguments += @('/f', $env:PLANETARY_SIGN_PFX)
    if ($password) { $arguments += @('/p', $password) }
} else {
    if ($thumbprint -notmatch '^[0-9A-F]{40}$') {
        throw 'Configure PLANETARY_SIGN_PFX or PLANETARY_SIGN_THUMBPRINT before building.'
    }
    $certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$thumbprint" -ErrorAction Stop
    $arguments += @('/sha1', $thumbprint, '/s', 'My')
}
if (-not $certificate.HasPrivateKey) { throw 'Signing certificate has no private key.' }
if ($certificate.NotBefore -gt (Get-Date) -or $certificate.NotAfter -le (Get-Date)) { throw 'Signing certificate is outside its validity period.' }
if ($certificate.EnhancedKeyUsageList -and
    $certificate.EnhancedKeyUsageList.ObjectId -notcontains '1.3.6.1.5.5.7.3.3') { throw 'Certificate is not intended for code signing.' }
if ($env:PLANETARY_TIMESTAMP_URL) { $arguments += @('/tr', $env:PLANETARY_TIMESTAMP_URL, '/td', 'SHA256') }
$probeDirectory = $null
try {
    if ($CheckOnly) {
        # An SSH login may read a PFX but still be unable to use its private key.
        # Exercise the actual SignTool path before spending time on compilation.
        $probeDirectory = Join-Path ([IO.Path]::GetTempPath()) ('Planetary-Signing-Probe-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $probeDirectory -ErrorAction Stop | Out-Null
        $FilePath = Join-Path $probeDirectory 'signing-probe.exe'
        Copy-Item -LiteralPath $signTool -Destination $FilePath
    }
    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) { throw "File not found: $FilePath" }
    & $signTool @arguments $FilePath
    if ($LASTEXITCODE -ne 0) {
        throw "SignTool failed with exit code $LASTEXITCODE. Inspect the EKU, expiry and private-key filter counts above. Preflight must pass under the same SSH account/session used for the build."
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $FilePath
    if (-not $signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $certificate.Thumbprint -or
        $signature.Status -in @('HashMismatch','NotSigned')) { throw "Signature verification failed: $FilePath" }
    if ($CheckOnly) { Write-Host "Real signing preflight passed: $($certificate.Subject)" }
    # Self-signed certificates may report an untrusted chain; do not modify system trust.
} finally {
    if ($probeDirectory) {
        # Delete only the one scratch executable and empty directory created above.
        Remove-Item -LiteralPath (Join-Path $probeDirectory 'signing-probe.exe') -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $probeDirectory -ErrorAction SilentlyContinue
    }
}
