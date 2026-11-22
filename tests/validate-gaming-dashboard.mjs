import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const containerfile = await readFile('Containerfile', 'utf8');
const cmake = await readFile('apps/better-on-gaming/CMakeLists.txt', 'utf8');
const source = await readFile('apps/better-on-gaming/main.cpp', 'utf8');
const desktop = await readFile('system_files/usr/share/applications/better-on-gaming.desktop', 'utf8');

assert.match(containerfile, /COPY apps\/better-on-gaming \/src\/better-on-gaming/);
assert.match(containerfile, /COPY --from=welcome-builder \/usr\/bin\/better-on-gaming/);
assert.match(cmake, /find_package\(Qt6 6\.4 REQUIRED COMPONENTS Widgets\)/);
assert.match(desktop, /Exec=\/usr\/bin\/better-on-gaming/);

for (const realDataSource of [
  '/proc/cpuinfo',
  '/proc/meminfo',
  '/proc/stat',
  '/sys/class/drm',
  'QStorageInfo::root()',
  'QSysInfo::kernelVersion()',
]) {
  assert.ok(source.includes(realDataSource), `Missing real system data source: ${realDataSource}`);
}

assert.match(source, /No display adapter detected through DRM sysfs/);
assert.match(source, /GPU temperatures, FPS, and game compatibility are not estimated/);
assert.match(source, /QStandardPaths::locate\([\s\S]*better-on\/hub\/catalog\.json/);
assert.match(source, /category[\s\S]*Gaming/);
assert.match(source, /QStringLiteral\("list"\)[\s\S]*QStringLiteral\("--app"\)[\s\S]*QStringLiteral\("--columns=application"\)/);
assert.match(source, /Installed gaming apps could not be read/);
assert.match(source, /installedNames\.isEmpty\(\)/);

assert.match(containerfile, /dnf5 install -y pciutils/);
assert.match(source, /findExecutable\(QStringLiteral\("lspci"\)\)/);
assert.match(source, /QStringLiteral\("-nn"\)/);
assert.match(source, /pciTimeout->setInterval\(3000\)/);
assert.match(source, /\.local\/share\/Steam\/steamapps/);
assert.match(source, /\.var\/app\/com\.valvesoftware\.Steam/);
assert.match(source, /libraryfolders\.vdf/);
assert.match(source, /appmanifest_\*\.acf/);
assert.match(source, /canonicalFilePath\(\)/);
assert.match(source, /installedPath\.startsWith\(commonDirectory/);
assert.match(source, /INSTALLED STEAM GAMES/);
assert.match(source, /#include <QScrollArea>/);
assert.match(source, /scrollArea->setWidgetResizable\(true\)/);
assert.match(source, /primaryScreen\(\)/);
assert.match(source, /refreshRate\(\)/);
assert.match(source, /Primary display refresh \(approx\.\)/);
assert.ok(!source.includes('\u00e2\u20ac\u00a6'), 'Source must not contain mojibake');

console.log('Gaming dashboard wiring, live system data sources, and installed Steam game discovery are present.');




