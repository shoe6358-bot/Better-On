import { readFile, writeFile } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';

export function patchPostrootfs(source) {
  const referenceBlock = [
    `imageref="$(podman images --format '{{ index .Names 0 }}\\n' 'bazzite*' | head -1)"`,
    'imageref="${imageref##*://}"',
    'imageref="${imageref%%:*}"',
    `imagetag="$(podman images --format '{{ .Tag }}\\n' "$imageref" | head -1)"`,
  ].join('\n');
  const installReferenceBlock = [
    'imageref="${INSTALL_IMAGE_PAYLOAD:?INSTALL_IMAGE_PAYLOAD must name the Better On image}"',
    'imagetag=""',
  ].join('\n');

  if (!source.includes(referenceBlock)) {
    throw new Error('Pinned Bazzite installer image-reference block changed; refusing to build with an unknown install target.');
  }
  source = source.replace(referenceBlock, installReferenceBlock);

  const installRefs = source.match(/\$imageref:\$imagetag/g) ?? [];
  if (installRefs.length !== 2) {
    throw new Error(`Expected two Bazzite payload-reference uses, found ${installRefs.length}.`);
  }
  source = source.replaceAll('$imageref:$imagetag', '$imageref');

  const nvidiaConditions = source.match(/\[\[ \$imageref == \*-nvidia\* \]\]/g) ?? [];
  if (nvidiaConditions.length !== 2) {
    throw new Error(`Expected two Bazzite NVIDIA live-session checks, found ${nvidiaConditions.length}.`);
  }
  source = source.replaceAll('[[ $imageref == *-nvidia* ]]', '[[ $INSTALL_IMAGE_VARIANT == nvidia ]]');

  const switchCommands = source.match(/bootc switch --mutate-in-place --enforce-container-sigpolicy --transport registry \$imageref/g) ?? [];
  if (switchCommands.length !== 1) {
    throw new Error(`Expected one signature-enforcing bootc switch, found ${switchCommands.length}.`);
  }

  const packageRemoval = 'dnf -yq remove steam lutris bazaar waydroid || :';
  if (!source.includes(packageRemoval)) {
    throw new Error('Expected live-session package-removal command was not found.');
  }
  source = source.replace(packageRemoval, 'dnf -yq remove bazaar waydroid || :');
  return source;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const scriptPath = process.argv[2];
  if (!scriptPath) throw new Error('Usage: node patch-bazzite-live-installer.mjs <postrootfs-script>');
  const source = await readFile(scriptPath, 'utf8');
  await writeFile(scriptPath, patchPostrootfs(source), 'utf8');
}
