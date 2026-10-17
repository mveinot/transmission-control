# Security Policy

## Supported versions

Security fixes are made against the current development branch and are included
in a subsequent release. Users should update to the latest stable release when
one is available. Older releases are not guaranteed to receive security fixes
or backports.

## Reporting a vulnerability

Please report suspected security vulnerabilities privately to
[planetary@mvgrafx.net](mailto:planetary@mvgrafx.net). Please do not publish
details in a public issue until the maintainer has had a reasonable opportunity
to investigate and coordinate a fix.

Include the affected Planetary version, operating system, steps to reproduce,
and potential impact. Do not include passwords, private server addresses,
unredacted configuration exports, or other sensitive data in an initial
report. There is no guaranteed response or remediation timeline, but reports
will be reviewed as promptly as possible.

## Data, credentials, and network behavior

Planetary is a local desktop application. It has no Planetary-hosted user
accounts, cloud storage, or settings synchronization. Application preferences
and server profiles are stored on the user's device using the platform's
settings facilities. Planetary does not send app settings, saved credentials,
torrent lists, peer addresses, or usage analytics to a Planetary service.

Saved server passwords are stored in the local settings store. Planetary does
not encrypt these values itself or store them in the operating system's secure
credential/keychain facility. Protect the operating-system account and device
accordingly.

There are two configuration export paths with different password behavior:

- **Export Server** asks whether to include the saved password. **Omit
  Password** is the default choice; the user must explicitly choose **Include
  Password** to put it in the exported JSON file.
- **Export Settings** exports all application settings. This can include saved
  server passwords, so treat the resulting file as sensitive. Importing a
  settings export restores the values it contains.

Planetary sends requests to the server endpoints the user configures in order
to operate those Transmission, qBittorrent, or Deluge servers. Those endpoints
may be on the user's own network or hosted remotely. The application supports
both HTTP and HTTPS; use HTTPS when connecting over an untrusted network.
Planetary does not control the security, storage, or logging practices of a
configured server.

Automatic update checks are enabled by default and can be disabled in
preferences. An update check makes an HTTPS request to Planetary's public
release-manifest endpoint and reveals the request's normal network metadata,
such as the source IP address and User-Agent, to the hosting service. The check
does not include the user's settings or current torrent/server data. Downloading
a release is a separate user action; before saving it, Planetary verifies the
download against the SHA-256 digest in the manifest. Planetary has no telemetry
or analytics service.

Peer-country lookups use the bundled GeoIP database locally; peer addresses are
not sent to a third-party lookup service.
