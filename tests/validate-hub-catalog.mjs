import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const path = new URL('../system_files/usr/share/better-on/hub/catalog.json', import.meta.url);
const catalog = JSON.parse(await readFile(path, 'utf8'));
const allowedCategories = new Set([
  'Browsers',
  'Gaming',
  'Communication',
  'Media & Creativity',
  'Productivity',
  'Utilities',
  'Development',
]);
const allowedVerification = new Set(['Verified', 'Listed', 'Unverified']);

assert.equal(catalog.schemaVersion, 1, 'unsupported catalog schema');
assert.equal(catalog.remote?.name, 'flathub', 'catalog must use the fixed Flathub remote');
assert.equal(catalog.remote?.homepage, 'https://flathub.org');
assert.match(catalog.verifiedOn ?? '', /^\d{4}-\d{2}-\d{2}$/, 'verifiedOn must be an ISO date');
assert.equal(new Date(`${catalog.verifiedOn}T00:00:00Z`).toISOString().slice(0, 10), catalog.verifiedOn,
  'verifiedOn must be a valid calendar date');
assert.ok(Array.isArray(catalog.applications) && catalog.applications.length > 0,
  'catalog must contain at least one application');

const ids = new Set();
for (const [index, app] of catalog.applications.entries()) {
  const where = `applications[${index}]`;
  assert.match(app.id ?? '', /^[A-Za-z0-9][A-Za-z0-9._-]*\.[A-Za-z0-9._-]+$/,
    `${where}.id must be a reverse-DNS-like Flatpak application ID`);
  assert.ok(!ids.has(app.id), `duplicate application ID: ${app.id}`);
  ids.add(app.id);

  for (const key of ['name', 'summary', 'developer']) {
    assert.equal(typeof app[key], 'string', `${where}.${key} must be text`);
    assert.ok(app[key].trim().length > 0, `${where}.${key} cannot be empty`);
  }
  assert.ok(allowedCategories.has(app.category), `${where}.category is unknown`);
  assert.ok(allowedVerification.has(app.verification), `${where}.verification is unknown`);

  const icon = new URL(app.icon);
  assert.equal(icon.protocol, 'https:', `${where}.icon must use HTTPS`);
  assert.equal(icon.hostname, 'dl.flathub.org', `${where}.icon must use Flathub's CDN`);
  assert.equal(icon.port, '', `${where}.icon cannot use a custom port`);
  assert.equal(icon.username, '', `${where}.icon cannot contain URL credentials`);
  assert.equal(icon.search, '', `${where}.icon cannot contain a query`);
  assert.equal(icon.hash, '', `${where}.icon cannot contain a fragment`);
  assert.match(icon.pathname, /\.png$/, `${where}.icon must be a PNG`);
  assert.ok(icon.pathname.includes(app.id), `${where}.icon path must correspond to its app ID`);

  const details = new URL(app.details);
  assert.equal(details.protocol, 'https:', `${where}.details must use HTTPS`);
  assert.equal(details.hostname, 'flathub.org', `${where}.details must point to Flathub`);
  assert.equal(details.port, '', `${where}.details cannot use a custom port`);
  assert.equal(details.username, '', `${where}.details cannot contain URL credentials`);
  assert.equal(details.search, '', `${where}.details cannot contain a query`);
  assert.equal(details.hash, '', `${where}.details cannot contain a fragment`);
  assert.equal(details.pathname, `/en/apps/${app.id}`, `${where}.details path must correspond to its app ID`);
  if (app.note !== undefined) assert.equal(typeof app.note, 'string', `${where}.note must be text`);
}

console.log(`Validated ${ids.size} Better On Hub catalog entries.`);
