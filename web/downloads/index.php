<?php
declare(strict_types=1);

/*
 * Planetary downloads and release-notes page
 *
 * Drop this file onto planetary.mvgrafx.net, for example as:
 *     /downloads/index.php
 *
 * The page reads public releases from:
 *     https://github.com/mveinot/transmission-control
 *
 * Requirements:
 *   - PHP 8.0+
 *   - cURL extension recommended, or allow_url_fopen enabled
 *   - Write access to CACHE_DIR is recommended
 */

/* ----------------------------- Configuration ----------------------------- */

const GITHUB_OWNER = 'mveinot';
const GITHUB_REPOSITORY = 'transmission-control';
const RELEASE_LIMIT = 10;
const CACHE_TTL = 900; // 15 minutes

// Keep this outside the public document root if convenient.
const CACHE_DIR = __DIR__ . '/cache';
const CACHE_FILE = CACHE_DIR . '/planetary-releases.json';

// Optional GitHub token. Leave blank for public, unauthenticated access.
// A token raises the API rate limit. It should preferably be supplied through
// the server environment rather than committed into this file.
$githubToken = getenv('GITHUB_TOKEN') ?: '';

/* ------------------------------ HTTP helpers ------------------------------ */

function githubApiRequest(string $url, string $token = ''): array
{
    $headers = [
        'Accept: application/vnd.github+json',
        'User-Agent: Planetary-Downloads-Page/1.0',
        'X-GitHub-Api-Version: 2022-11-28',
    ];

    if ($token !== '') {
        $headers[] = 'Authorization: Bearer ' . $token;
    }

    if (function_exists('curl_init')) {
        $curl = curl_init($url);
        if ($curl === false) {
            throw new RuntimeException('Unable to initialize cURL.');
        }

        curl_setopt_array($curl, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_FOLLOWLOCATION => true,
            CURLOPT_CONNECTTIMEOUT => 5,
            CURLOPT_TIMEOUT => 15,
            CURLOPT_HTTPHEADER => $headers,
        ]);

        $body = curl_exec($curl);
        $status = (int) curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
        $error = curl_error($curl);
        curl_close($curl);

        if ($body === false) {
            throw new RuntimeException('GitHub request failed: ' . $error);
        }

        if ($status < 200 || $status >= 300) {
            throw new RuntimeException("GitHub returned HTTP {$status}.");
        }
    } else {
        $context = stream_context_create([
            'http' => [
                'method' => 'GET',
                'timeout' => 15,
                'ignore_errors' => true,
                'header' => implode("\r\n", $headers),
            ],
        ]);

        $body = @file_get_contents($url, false, $context);
        if ($body === false) {
            throw new RuntimeException(
                'GitHub request failed. Enable cURL or allow_url_fopen.'
            );
        }

        $status = 0;
        foreach ($http_response_header ?? [] as $header) {
            if (preg_match('/^HTTP\/\S+\s+(\d{3})/', $header, $matches)) {
                $status = (int) $matches[1];
            }
        }

        if ($status < 200 || $status >= 300) {
            throw new RuntimeException("GitHub returned HTTP {$status}.");
        }
    }

    $decoded = json_decode($body, true, 512, JSON_THROW_ON_ERROR);
    if (!is_array($decoded)) {
        throw new RuntimeException('GitHub returned an unexpected response.');
    }

    return $decoded;
}

function loadReleases(string $token): array
{
    $cacheIsFresh = is_file(CACHE_FILE)
        && (time() - (int) filemtime(CACHE_FILE)) < CACHE_TTL;

    if ($cacheIsFresh) {
        $cached = json_decode(
            (string) file_get_contents(CACHE_FILE),
            true,
            512,
            JSON_THROW_ON_ERROR
        );

        return is_array($cached) ? $cached : [];
    }

    $apiUrl = sprintf(
        'https://api.github.com/repos/%s/%s/releases?per_page=%d',
        rawurlencode(GITHUB_OWNER),
        rawurlencode(GITHUB_REPOSITORY),
        RELEASE_LIMIT
    );

    try {
        $releases = githubApiRequest($apiUrl, $token);

        if (!is_dir(CACHE_DIR) && !@mkdir(CACHE_DIR, 0755, true) && !is_dir(CACHE_DIR)) {
            // Caching is useful, but inability to cache should not break the page.
            return $releases;
        }

        $temporaryFile = CACHE_FILE . '.tmp';
        $json = json_encode(
            $releases,
            JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_THROW_ON_ERROR
        );

        if (@file_put_contents($temporaryFile, $json, LOCK_EX) !== false) {
            @rename($temporaryFile, CACHE_FILE);
        }

        return $releases;
    } catch (Throwable $exception) {
        // A stale page is substantially better than no page during a GitHub
        // outage or rate-limit event.
        if (is_file(CACHE_FILE)) {
            $cached = json_decode(
                (string) file_get_contents(CACHE_FILE),
                true,
                512,
                JSON_THROW_ON_ERROR
            );

            return is_array($cached) ? $cached : [];
        }

        throw $exception;
    }
}

/* ---------------------------- Output utilities ---------------------------- */

function e(?string $value): string
{
    return htmlspecialchars($value ?? '', ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

function formatDate(?string $date): string
{
    if (!$date) {
        return '';
    }

    try {
        return (new DateTimeImmutable($date))
            ->setTimezone(new DateTimeZone('America/Halifax'))
            ->format('F j, Y');
    } catch (Throwable) {
        return $date;
    }
}

function formatBytes(int|float|null $bytes): string
{
    $size = max(0, (float) ($bytes ?? 0));
    $units = ['B', 'KB', 'MB', 'GB'];
    $unit = 0;

    while ($size >= 1024 && $unit < count($units) - 1) {
        $size /= 1024;
        $unit++;
    }

    $precision = $unit === 0 ? 0 : 1;
    return number_format($size, $precision) . ' ' . $units[$unit];
}

function assetDetails(string $filename): array
{
    $lower = strtolower($filename);
    $platform = 'Download';
    $architecture = '';
    $icon = '↓';

    if (str_contains($lower, 'macos') || str_ends_with($lower, '.dmg')) {
        $platform = 'macOS';
        $icon = '⌘';
    } elseif (
        str_contains($lower, 'windows')
        || str_ends_with($lower, '.exe')
        || str_ends_with($lower, '.msi')
    ) {
        $platform = 'Windows';
        $icon = '⊞';
    } elseif (
        str_contains($lower, 'linux')
        || str_ends_with($lower, '.appimage')
        || str_ends_with($lower, '.deb')
        || str_ends_with($lower, '.rpm')
        || str_ends_with($lower, '.flatpak')
    ) {
        $platform = 'Linux';
        $icon = '◆';
    }

    if (str_contains($lower, 'universal')) {
        $architecture = 'Apple Silicon + Intel';
    } elseif (
        str_contains($lower, 'arm64')
        || str_contains($lower, 'aarch64')
        || str_contains($lower, 'apple-silicon')
    ) {
        $architecture = 'Apple Silicon / ARM64';
    } elseif (
        str_contains($lower, 'x86_64')
        || str_contains($lower, 'amd64')
        || str_contains($lower, 'x64')
    ) {
        $architecture = 'Intel / x86-64';
    } elseif (preg_match('/(?:^|[-_.])x86(?:[-_.]|$)/', $lower)) {
        $architecture = '32-bit x86';
    }

    if (str_ends_with($lower, '.zip')) {
        $type = 'ZIP archive';
    } elseif (str_ends_with($lower, '.dmg')) {
        $type = 'Disk image';
    } elseif (str_ends_with($lower, '.exe')) {
        $type = 'Installer';
    } elseif (str_ends_with($lower, '.msi')) {
        $type = 'MSI installer';
    } elseif (str_ends_with($lower, '.appimage')) {
        $type = 'AppImage';
    } elseif (str_ends_with($lower, '.deb')) {
        $type = 'Debian package';
    } elseif (str_ends_with($lower, '.rpm')) {
        $type = 'RPM package';
    } else {
        $type = 'Download';
    }

    return compact('platform', 'architecture', 'type', 'icon');
}

function renderInlineMarkdown(string $text): string
{
    $escaped = e($text);

    // Inline code first so its contents are not subsequently formatted.
    $placeholders = [];
    $escaped = preg_replace_callback(
        '/`([^`]+)`/',
        static function (array $matches) use (&$placeholders): string {
            $key = '@@CODE' . count($placeholders) . '@@';
            $placeholders[$key] = '<code>' . $matches[1] . '</code>';
            return $key;
        },
        $escaped
    ) ?? $escaped;

    $escaped = preg_replace(
        '/\[([^\]]+)\]\((https?:\/\/[^)\s]+)\)/',
        '<a href="$2" rel="noopener noreferrer">$1</a>',
        $escaped
    ) ?? $escaped;

    $escaped = preg_replace('/\*\*([^*]+)\*\*/', '<strong>$1</strong>', $escaped)
        ?? $escaped;
    $escaped = preg_replace('/__([^_]+)__/', '<strong>$1</strong>', $escaped)
        ?? $escaped;
    $escaped = preg_replace('/(?<!\*)\*([^*\n]+)\*(?!\*)/', '<em>$1</em>', $escaped)
        ?? $escaped;

    return strtr($escaped, $placeholders);
}

function renderReleaseMarkdown(?string $markdown): string
{
    $markdown = trim((string) $markdown);
    if ($markdown === '') {
        return '<p>No release notes were provided for this release.</p>';
    }

    $lines = preg_split('/\R/', $markdown) ?: [];
    $html = [];
    $paragraph = [];
    $inList = false;
    $inCode = false;
    $codeLines = [];

    $flushParagraph = static function () use (&$paragraph, &$html): void {
        if ($paragraph !== []) {
            $html[] = '<p>' . renderInlineMarkdown(
                implode(' ', array_map('trim', $paragraph))
            ) . '</p>';
            $paragraph = [];
        }
    };

    $closeList = static function () use (&$inList, &$html): void {
        if ($inList) {
            $html[] = '</ul>';
            $inList = false;
        }
    };

    foreach ($lines as $line) {
        if (preg_match('/^\s*```/', $line)) {
            $flushParagraph();
            $closeList();

            if ($inCode) {
                $html[] = '<pre><code>' . e(implode("\n", $codeLines)) . '</code></pre>';
                $codeLines = [];
                $inCode = false;
            } else {
                $inCode = true;
            }
            continue;
        }

        if ($inCode) {
            $codeLines[] = $line;
            continue;
        }

        if (trim($line) === '') {
            $flushParagraph();
            $closeList();
            continue;
        }

        if (preg_match('/^(#{1,4})\s+(.+)$/', $line, $matches)) {
            $flushParagraph();
            $closeList();
            $level = min(4, strlen($matches[1]) + 2);
            $html[] = sprintf(
                '<h%d>%s</h%d>',
                $level,
                renderInlineMarkdown($matches[2]),
                $level
            );
            continue;
        }

        if (preg_match('/^\s*[-*+]\s+(.+)$/', $line, $matches)) {
            $flushParagraph();
            if (!$inList) {
                $html[] = '<ul>';
                $inList = true;
            }
            $html[] = '<li>' . renderInlineMarkdown($matches[1]) . '</li>';
            continue;
        }

        if (preg_match('/^\s*\d+\.\s+(.+)$/', $line, $matches)) {
            // GitHub release notes rarely need ordered-list semantics; using
            // the same clean list treatment keeps the renderer dependency-free.
            $flushParagraph();
            if (!$inList) {
                $html[] = '<ul>';
                $inList = true;
            }
            $html[] = '<li>' . renderInlineMarkdown($matches[1]) . '</li>';
            continue;
        }

        $paragraph[] = $line;
    }

    $flushParagraph();
    $closeList();

    if ($inCode) {
        $html[] = '<pre><code>' . e(implode("\n", $codeLines)) . '</code></pre>';
    }

    return implode("\n", $html);
}

/* ------------------------------ Load releases ----------------------------- */

$errorMessage = '';
$releases = [];

try {
    $releases = array_values(array_filter(
        loadReleases($githubToken),
        static fn (array $release): bool =>
            empty($release['draft']) && empty($release['prerelease'])
    ));
} catch (Throwable $exception) {
    $errorMessage = $exception->getMessage();
}

$latestRelease = $releases[0] ?? null;
$repositoryUrl = sprintf(
    'https://github.com/%s/%s',
    GITHUB_OWNER,
    GITHUB_REPOSITORY
);
?>
<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="color-scheme" content="dark">
    <meta name="theme-color" content="#10131a">
    <meta name="description" content="Download the latest Planetary release for macOS and read release notes.">
    <title>Downloads — Planetary</title>
    <link rel="icon" href="../favicon.png">
    <link rel="stylesheet" href="../styles.css">
    <link rel="stylesheet" href="styles.css">
</head>
<body class="downloads-page">
<header class="site-header">
    <a class="brand" href="/" aria-label="Planetary home">
        <img src="../assets/planetary.png" alt="" width="42" height="42">
        <span>Planetary</span>
    </a>
    <nav aria-label="Main navigation">
        <a href="/#features">Features</a>
        <a href="/#screenshots">Screenshots</a>
        <a href="/#about">About</a>
        <a class="nav-button" href="/downloads/" aria-current="page">Download</a>
    </nav>
</header>

<main class="download-main">
    <section class="download-hero">
        <p class="eyebrow">Downloads and release notes</p>
        <h1>Get Planetary.</h1>
        <p class="lead">
            Download the universal app for Apple Silicon and Intel Macs running
            macOS 13 or later, or install it with Homebrew. Windows and Linux
            builds are available from source while release packaging matures.
        </p>
    </section>

    <section class="theme-download" aria-labelledby="theme-download-title">
        <div class="theme-download-icon" aria-hidden="true">✦</div>
        <div class="theme-download-copy">
            <span class="badge">New</span>
            <h2 id="theme-download-title">Planetary themes</h2>
            <p>Explore the complete collection of built-in icon/colour themes. Download the pack and select the themes from Planetary’s preferences.</p>
        </div>
        <a class="button primary" href="../icon-themes.zip" download>
            Download theme pack <span aria-hidden="true">↓</span>
        </a>
    </section>

    <?php if ($latestRelease): ?>
        <?php
        $latestName = (string) ($latestRelease['name'] ?: $latestRelease['tag_name']);
        $latestTag = (string) ($latestRelease['tag_name'] ?? '');
        $latestAssets = is_array($latestRelease['assets'] ?? null)
            ? $latestRelease['assets']
            : [];
        ?>
        <section class="latest-card" aria-labelledby="latest-release">
            <div class="release-heading">
                <div>
                    <span class="badge">Latest release</span>
                    <h2 id="latest-release"><?= e($latestName) ?></h2>
                    <div class="release-meta">
                        Released <?= e(formatDate($latestRelease['published_at'] ?? null)) ?>
                    </div>
                </div>
                <a class="button secondary" href="<?= e($latestRelease['html_url'] ?? $repositoryUrl . '/releases') ?>">
                    View on GitHub ↗
                </a>
            </div>

            <div class="homebrew">
                <div>
                    <div class="homebrew-label">Install on macOS with Homebrew</div>
                    <code id="brew-command">brew install --cask mveinot/tap/planetary</code>
                </div>
                <button class="copy-button" type="button" data-copy="#brew-command">
                    Copy command
                </button>
            </div>

            <?php if ($latestAssets !== []): ?>
                <div class="asset-grid">
                    <?php foreach ($latestAssets as $asset): ?>
                        <?php
                        $filename = (string) ($asset['name'] ?? 'Download');
                        $details = assetDetails($filename);
                        $detailText = implode(' · ', array_filter([
                            $details['architecture'],
                            $details['type'],
                        ]));
                        ?>
                        <a class="asset"
                           href="<?= e($asset['browser_download_url'] ?? '#') ?>"
                           download>
                            <span class="asset-icon" aria-hidden="true">
                                <?= e($details['icon']) ?>
                            </span>
                            <span>
                                <span class="asset-title">
                                    <?= e($details['platform']) ?>
                                    <?= $details['architecture'] !== ''
                                        ? ' — ' . e($details['architecture'])
                                        : '' ?>
                                </span>
                                <span class="asset-detail"><?= e($detailText) ?></span>
                                <span class="asset-filename"><?= e($filename) ?></span>
                            </span>
                            <span class="asset-size">
                                <?= e(formatBytes($asset['size'] ?? 0)) ?>
                            </span>
                        </a>
                    <?php endforeach; ?>
                </div>
            <?php else: ?>
                <p>No downloadable assets are attached to this release.</p>
            <?php endif; ?>
        </section>
    <?php endif; ?>

    <?php if ($errorMessage !== ''): ?>
        <div class="error">
            <strong>Release information is temporarily unavailable.</strong><br>
            <?= e($errorMessage) ?>
        </div>
    <?php elseif ($releases === []): ?>
        <div class="error">
            No published releases were returned by GitHub.
        </div>
    <?php else: ?>
        <section class="history" aria-labelledby="release-history">
            <div class="section-intro">
                <p class="eyebrow">What changed</p>
                <h2 id="release-history">Release history</h2>
                <p>
                    Notes and download links are populated from published GitHub
                    releases. Drafts and prereleases are intentionally omitted.
                </p>
            </div>

            <?php foreach ($releases as $index => $release): ?>
                <?php
                $releaseName = (string) ($release['name'] ?: $release['tag_name']);
                $releaseAssets = is_array($release['assets'] ?? null)
                    ? $release['assets']
                    : [];
                ?>
                <article class="release">
                    <div class="release-title-row">
                        <div>
                            <h3>
                                <a href="<?= e($release['html_url'] ?? '#') ?>">
                                    <?= e($releaseName) ?>
                                </a>
                            </h3>
                            <div class="release-meta">
                                <?= e(formatDate($release['published_at'] ?? null)) ?>
                                <?php if ($index === 0): ?>
                                    · Current release
                                <?php endif; ?>
                            </div>
                        </div>
                        <?php $assetCount = count($releaseAssets); ?>
                        <span class="badge">
                            <?= $assetCount ?> <?= $assetCount === 1 ? 'file' : 'files' ?>
                        </span>
                    </div>

                    <div class="release-notes">
                        <?= renderReleaseMarkdown($release['body'] ?? '') ?>
                    </div>

                    <div class="release-actions">
                        <?php foreach ($releaseAssets as $asset): ?>
                            <?php
                            $filename = (string) ($asset['name'] ?? 'Download');
                            $details = assetDetails($filename);
                            ?>
                            <a class="button <?= $index === 0 ? 'button-primary' : '' ?>"
                               href="<?= e($asset['browser_download_url'] ?? '#') ?>">
                                <?= e($details['platform']) ?>
                                <?= $details['architecture'] !== ''
                                    ? ' · ' . e($details['architecture'])
                                    : '' ?>
                                <span aria-hidden="true">↓</span>
                            </a>
                        <?php endforeach; ?>
                        <a class="button secondary" href="<?= e($release['html_url'] ?? '#') ?>">
                            GitHub release ↗
                        </a>
                    </div>
                </article>
            <?php endforeach; ?>
        </section>
    <?php endif; ?>
</main>

<footer>
    <div class="footer-brand">
        <img src="../assets/planetary.png" alt="" width="34" height="34">
        <span>Planetary</span>
    </div>
    <p>
        Planetary is independent and is not affiliated with the Transmission,
        qBittorrent, or Deluge projects.
    </p>
    <div class="footer-links">
        <a href="<?= e($repositoryUrl) ?>">Source</a>
        <a href="<?= e($repositoryUrl . '/issues') ?>">Issues</a>
        <a href="<?= e($repositoryUrl . '/blob/main/LICENSE') ?>">License</a>
    </div>
</footer>

<script>
document.querySelectorAll('[data-copy]').forEach((button) => {
    button.addEventListener('click', async () => {
        const target = document.querySelector(button.dataset.copy);
        if (!target) return;

        const originalText = button.textContent;
        try {
            await navigator.clipboard.writeText(target.textContent.trim());
            button.textContent = 'Copied';
        } catch (_) {
            button.textContent = 'Select and copy';
        }

        window.setTimeout(() => {
            button.textContent = originalText;
        }, 1800);
    });
});
</script>
</body>
</html>
