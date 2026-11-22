import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const source = await readFile('apps/better-on-welcome/main.cpp', 'utf8');
const cmake = await readFile('apps/better-on-welcome/CMakeLists.txt', 'utf8');

assert.match(cmake, /find_package\(Qt6 6\.4 REQUIRED COMPONENTS Widgets\)/);
assert.match(source, /QStackedWidget/);
assert.match(source, /Welcome setup progress/);
for (const page of [
  'Welcome to Better On',
  'Get oriented',
  'Set up for gaming',
  'Choose your applications',
  "You're all set",
]) {
  assert.ok(source.includes(page), `Missing first-run setup step: ${page}`);
}

for (const destination of ['systemsettings', 'better-on-gaming', 'better-on-hub', 'plasma-discover']) {
  assert.ok(source.includes(`QStringLiteral("${destination}")`), `Missing real launch target: ${destination}`);
}

assert.match(source, /kcm_lookandfeel/);
assert.match(source, /Skip this step/);
assert.match(source, /Skip setup/);
assert.match(source, /finishSetup\(&window\)/);
assert.match(source, /markCompleted\(\)/);

console.log('Welcome setup contains skippable onboarding steps and real KDE, Gaming, Hub, and Discover launch targets.');
