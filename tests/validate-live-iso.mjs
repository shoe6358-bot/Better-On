import assert from 'node:assert/strict';
import { mkdtemp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { prepareLiveContext } from '../installer/prepare-live-context.mjs';
import { patchPostrootfs } from '../installer/patch-bazzite-live-installer.mjs';

const postrootfs = [
  `imageref="$(podman images --format '{{ index .Names 0 }}\\n' 'bazzite*' | head -1)"`,
  'imageref="${imageref##*://}"',
  'imageref="${imageref%%:*}"',
  `imagetag="$(podman images --format '{{ .Tag }}\\n' "$imageref" | head -1)"`,
  'ostreecontainer --url=$imageref:$imagetag --transport=containers-storage --no-signature-verification',
  'bootc switch --mutate-in-place --enforce-container-sigpolicy --transport registry $imageref:$imagetag',
  'if [[ $imageref == *-nvidia* ]]; then echo nvidia-live-fix; fi',
  'if [[ $imageref == *-nvidia* ]]; then echo nvidia-firmware-fix; fi',
  'dnf -yq remove steam lutris bazaar waydroid || :',
].join('\n');

const patched = patchPostrootfs(postrootfs);
assert.match(patched, /imageref="\$\{INSTALL_IMAGE_PAYLOAD:\?INSTALL_IMAGE_PAYLOAD must name the Better On image\}"/);
assert.equal((patched.match(/\$imageref:\$imagetag/g) ?? []).length, 0);
assert.equal((patched.match(/\$\{INSTALL_IMAGE_PAYLOAD/g) ?? []).length, 1);
assert.match(patched, /bootc switch --mutate-in-place --enforce-container-sigpolicy --transport registry \$imageref/);
assert.equal((patched.match(/\[\[ \$INSTALL_IMAGE_VARIANT == nvidia \]\]/g) ?? []).length, 2);
assert.equal((patched.match(/--enforce-container-sigpolicy/g) ?? []).length, 1);
assert.match(patched, /remove bazaar waydroid/);
assert.doesNotMatch(patched, /remove steam lutris/);

const tempRoot = await mkdtemp(path.join(os.tmpdir(), 'better-on-live-iso-'));
try {
  const upstream = path.join(tempRoot, 'upstream');
  const output = path.join(tempRoot, 'output');
  await mkdir(path.join(upstream, 'kde_flatpaks'), { recursive: true });
  await writeFile(path.join(upstream, 'titanoboa_hook_postrootfs.sh'), postrootfs);
  await writeFile(path.join(upstream, 'Containerfile'), 'upstream containerfile');
  await writeFile(path.join(upstream, 'iso.yaml'), 'upstream config');
  await writeFile(path.join(upstream, 'build.sh'), '#!/usr/bin/bash\n');
  await writeFile(path.join(upstream, 'kde_flatpaks', 'flatpaks'), 'upstream app list');

  const projectRoot = path.resolve('.');
  await prepareLiveContext(upstream, output, projectRoot);

  assert.match(await readFile(path.join(output, 'Containerfile'), 'utf8'), /INSTALL_IMAGE_PAYLOAD/);
  assert.match(await readFile(path.join(output, 'iso.yaml'), 'utf8'), /Launch Better On Live Desktop/);
  assert.equal(await readFile(path.join(output, 'kde_flatpaks', 'flatpaks'), 'utf8'), '');
  assert.match(await readFile(path.join(output, 'titanoboa_hook_postrootfs.sh'), 'utf8'), /INSTALL_IMAGE_PAYLOAD/);
  assert.match(await readFile(path.join(output, 'build_files', 'seed-kde-defaults.sh'), 'utf8'), /seed_kde_default/);
  assert.match(await readFile(path.join(output, 'better-on-system-files/usr/share/color-schemes/BetterOn.colors'), 'utf8'), /BetterOn/);
} finally {
  await rm(tempRoot, { recursive: true, force: true });
}

const workflow = await readFile('.github/workflows/build-live-iso.yml', 'utf8');
assert.match(workflow, /ublue-os\/titanoboa@[0-9a-f]{40}/);
assert.ok(
  workflow.indexOf('verified_base_ref=$(verify_image_digest "$BASE_IMAGE_REF")') <
    workflow.indexOf('verified_payload_ref=$(verify_image_digest "$PAYLOAD_REF")'),
  'The live ISO must resolve and verify its shared base before its selected install payload.'
);
assert.match(workflow, /sudo podman image inspect --format '\{\{\.Digest\}\}'/);
assert.match(workflow, /"\$immutable_ref" >\/dev\/null \|\| return \$\?/);
assert.match(workflow, /LIVE_BASE_REF=\$\{verified_base_ref\}/);
assert.match(workflow, /PAYLOAD_REF=\$\{verified_payload_ref\}/);
assert.match(workflow, /--registry-username "\$GH_USER" --registry-password "\$GH_TOKEN"/);
assert.ok(
  workflow.indexOf('- name: Verify signed Better On live installer payload') < workflow.indexOf('- name: Build the KDE live installer environment'),
  'The live ISO must only be built from a verified Better On payload.'
);
assert.match(workflow, /repository: ublue-os\/bazzite/);
assert.match(workflow, /ref: [0-9a-f]{40}/);
assert.match(workflow, /variant: desktop[\s\S]*variant: nvidia/);
assert.match(workflow, /PAYLOAD_REF=ghcr\.io\/\$\{image_owner\}\/\$\{image_name\}:\$\{payload_tag\}/);
assert.match(workflow, /--build-arg "INSTALL_IMAGE_VARIANT=\$\{IMAGE_VARIANT\}"/);
assert.match(workflow, /sha256sum "\$ISO_PATH"/);
assert.match(workflow, /actions\/upload-artifact@[0-9a-f]{40}/);

const imageWorkflow = await readFile('.github/workflows/build.yml', 'utf8');
const imageContainerfile = await readFile('Containerfile', 'utf8');
const imageBuildScript = await readFile('build_files/build.sh', 'utf8');
const signaturePolicyScript = await readFile('build_files/configure-signature-policy.sh', 'utf8');
assert.ok(
  imageWorkflow.indexOf('- name: Build bootc image') < imageWorkflow.indexOf('- name: Verify Better On signing key pair'),
  'The image build must run before the publishing-only signing-key gate.'
);
assert.ok(
  imageWorkflow.indexOf('cosign sign -y') < imageWorkflow.indexOf('cosign verify --key'),
  'The pushed image digest must be signed before its signature is verified.'
);
assert.match(imageWorkflow, /cosign-installer@[0-9a-f]{40}/);
assert.match(imageWorkflow, /cosign sign[\s\S]*COSIGN_PRIVATE_KEY/);
assert.match(imageWorkflow, /--registry-referrers-mode=legacy/);
assert.match(imageWorkflow, /cosign verify --key build_files\/cosign\.pub/);
assert.match(imageWorkflow, /build_files\/cosign\.pub/);
assert.match(imageWorkflow, /cosign public-key --key env:\/\/COSIGN_PRIVATE_KEY/);
assert.match(imageWorkflow, /podman push --digestfile/);
assert.match(imageWorkflow, /IMAGE_REPOSITORY=\$image_repository/);
assert.match(imageContainerfile, /IMAGE_REPOSITORY/);
assert.match(imageBuildScript, /configure-signature-policy\.sh/);
assert.match(signaturePolicyScript, /sigstoreSigned/);
assert.match(signaturePolicyScript, /default \| type == "array"[\s\S]*insecureAcceptAnything/);
assert.match(signaturePolicyScript, /use-sigstore-attachments: true/);
assert.match(signaturePolicyScript, /matchRepository/);
assert.match(signaturePolicyScript, /transports\.docker\[\$repository\]/);

console.log('Live ISO source patch, context assembly, variant references, and artifact workflow passed static checks.');
