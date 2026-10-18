PLANETARY WEBSITE

Upload the contents of this folder to the document root for planetary.mvgrafx.net.
The landing page is static and the downloads page requires PHP 8.0 or later.

Files:
- index.html
- styles.css
- favicon.png
- assets/
- assets/responsive/ (generated WebP screenshot sizes with PNG fallbacks)
- .htaccess (optional one-day cache headers for images and CSS; requires mod_expires)
- themes/index.html
- themes/styles.css
- themes/packages/
- themes/previews/
- themes/packages/.htaccess (serves .planetarytheme downloads with the correct filename)
- downloads/index.php
- downloads/styles.css
- downloads/cache/.gitkeep
- updates/v1/stable.json
- updates/v1/beta.json

The downloads page reads published releases from GitHub and caches the response
for 15 minutes. Give the web server write access to downloads/cache, or move the
cache location outside the public document root by changing CACHE_DIR.

No release version is hard-coded into the downloads page. Update the matching
manifest in updates/v1/ when publishing a stable or beta release.

Theme packages and their catalogue are static files under themes/. Keep the
packages there as the authoritative downloadable copies; the application
source tree does not maintain a second theme collection.
