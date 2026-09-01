# Release update manifest

Planetary checks for stable updates using the JSON manifest at:

`https://planetary.mvgrafx.net/updates/v1/stable.json`

The endpoint is versioned independently of the application. The current client
supports schema version 1 and the `stable` channel.

## Example

```json
{
  "schemaVersion": 1,
  "channel": "stable",
  "version": "2.0.0",
  "build": 376,
  "displayVersion": "2.0.0.376",
  "minimumMacOSVersion": "13.0",
  "downloadUrl": "https://github.com/mveinot/transmission-control/releases/download/v2.0.0.376/Planetary-2.0.0.376-macOS-universal.dmg",
  "releaseNotesUrl": "https://github.com/mveinot/transmission-control/releases/tag/v2.0.0.376",
  "releaseNotesMarkdown": "## What's new\n\n- Universal Apple Silicon and Intel build.",
  "sha256": "fd0dd6fa10d9a3098f6ac554cf6419ee96b01a54e16e3b59b504fb6c5f4e0cf8"
}
```

Newlines in Markdown must be escaped as `\n` in the JSON string.

## Fields

| Field | Required | Format and behavior |
| --- | --- | --- |
| `schemaVersion` | Yes | Integer. Must be `1`. |
| `channel` | Yes | String. Must be `stable`. |
| `version` | Yes | Numeric `major.minor.patch` string, such as `2.0.0`. |
| `build` | Yes | Non-negative integer. |
| `displayVersion` | Yes | Must exactly equal `version` followed by `.` and `build`, such as `2.0.0.376`. |
| `minimumMacOSVersion` | Yes | Numeric `major.minor` string, such as `13.0`. The current client validates and records this value but does not use it to suppress the update notification. |
| `downloadUrl` | Yes | Valid HTTPS URL for the release artifact. The current client validates this URL but does not download or install the artifact. |
| `releaseNotesUrl` | Yes | Valid HTTPS URL opened by the **Open Release Page** button. |
| `releaseNotesMarkdown` | No | Markdown release notes, limited to 256 KiB. Planetary renders GitHub-dialect Markdown with raw HTML disabled. If this field is absent or blank, the dialog displays a link to `releaseNotesUrl`. |
| `sha256` | Yes | The artifact's SHA-256 digest as exactly 64 hexadecimal characters. Planetary normalizes it to lowercase. The current client validates the format but does not verify the artifact because it does not download it. |

Unknown fields are ignored, allowing optional metadata to be added without
breaking schema version 1 clients.

## Version comparison

Planetary compares the four numeric components of `displayVersion` with the
running application's `major.minor.patch.build` version. An update is offered
only when the manifest version is numerically newer.

## Publishing a release

1. Build, sign, notarize, and staple the universal macOS DMG.
2. Upload the DMG and publish its release page before changing the manifest.
3. Calculate the digest with `shasum -a 256 Planetary-*.dmg` and copy the exact
   result into `sha256`.
4. Verify that `downloadUrl` and `releaseNotesUrl` are public HTTPS URLs.
5. Update the manifest last, preferably with an atomic file replacement, so an
   incomplete release is never advertised.
6. Fetch the public endpoint and validate the live JSON, version, URLs, and
   digest after publishing.

The server should return `Content-Type: application/json`. A short cache policy,
such as `Cache-Control: max-age=300` or `no-cache`, keeps release discovery
timely. `ETag` or `Last-Modified` headers are also useful.

Additive optional fields can remain in schema version 1. A breaking format or
semantic change should use a new schema version and endpoint, with compatible
client support released before the new manifest is published.
