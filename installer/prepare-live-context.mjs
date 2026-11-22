import { cp, mkdir, readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';

export async function prepareLiveContext(upstreamInstaller, outputDirectory, projectRoot) {
  await mkdir(outputDirectory, { recursive: false });
  await cp(upstreamInstaller, outputDirectory, { recursive: true, force: false });

  const installerScript = path.join(outputDirectory, 'titanoboa_hook_postrootfs.sh');
  const patcherPath = path.join(path.dirname(fileURLToPath(import.meta.url)), 'patch-bazzite-live-installer.mjs');
  const { patchPostrootfs } = await import(pathToFileURL(patcherPath));
  const original = await readFile(installerScript, 'utf8');
  await writeFile(installerScript, patchPostrootfs(original), 'utf8');

  await cp(path.join(projectRoot, 'installer', 'Containerfile.live'),
           path.join(outputDirectory, 'Containerfile'), { force: true });
  await cp(path.join(projectRoot, 'installer', 'iso-live.yaml'),
           path.join(outputDirectory, 'iso.yaml'), { force: true });
  await cp(path.join(projectRoot, 'system_files'),
           path.join(outputDirectory, 'better-on-system-files'), { recursive: true, force: false });
  await mkdir(path.join(outputDirectory, 'build_files'), { recursive: true });
  await cp(path.join(projectRoot, 'build_files', 'seed-kde-defaults.sh'),
           path.join(outputDirectory, 'build_files', 'seed-kde-defaults.sh'), { force: false });

  const flatpakFile = path.join(outputDirectory, 'kde_flatpaks', 'flatpaks');
  await mkdir(path.dirname(flatpakFile), { recursive: true });
  await writeFile(flatpakFile, '', 'utf8');
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const [upstreamInstaller, outputDirectory, projectRoot] = process.argv.slice(2);
  if (!upstreamInstaller || !outputDirectory || !projectRoot) {
    throw new Error('Usage: node prepare-live-context.mjs <upstream-installer-dir> <output-dir> <project-root>');
  }
  await prepareLiveContext(upstreamInstaller, outputDirectory, projectRoot);
}
