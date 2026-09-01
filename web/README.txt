PLANETARY WEBSITE

Upload the contents of this folder to the document root for planetary.mvgrafx.net.
The landing page is static and the downloads page requires PHP 8.0 or later.

Files:
- index.html
- styles.css
- favicon.png
- assets/
- downloads/index.php
- downloads/styles.css
- updates/v1/stable.json

The downloads page reads published releases from GitHub and caches the response
for 15 minutes. Give the web server write access to downloads/cache, or move the
cache location outside the public document root by changing CACHE_DIR.

No release version is hard-coded into the page. Update updates/v1/stable.json
when publishing a new stable release.
