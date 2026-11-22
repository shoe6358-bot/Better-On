import assert from 'node:assert/strict';
import { createHash, createPublicKey } from 'node:crypto';
import { readFile } from 'node:fs/promises';

const workflow = await readFile('.github/workflows/build-iso.yml', 'utf8');
const imageWorkflow = await readFile('.github/workflows/build.yml', 'utf8');
const liveIsoWorkflow = await readFile('.github/workflows/build-live-iso.yml', 'utf8');
const installer = await readFile('installer/Containerfile', 'utf8');
const imageContainerfile = await readFile('Containerfile', 'utf8');
const bazziteKey = await readFile('build_files/bazzite-cosign.pub', 'utf8');

for (const [name, source] of Object.entries({ workflow, imageWorkflow, liveIsoWorkflow })) {
  assert.match(source, /runs-on:\s*ubuntu-24\.04/, `${name} must use the stable Ubuntu 24.04 runner.`);
  assert.doesNotMatch(source, /runs-on:\s*ubuntu-26\.04/, `${name} must not depend on the Ubuntu 26.04 preview runner.`);
}

assert.match(workflow, /osbuild\/image-builder-action@[0-9a-f]{40}/);
assert.match(workflow, /cosign-installer@[0-9a-f]{40}/);
assert.match(workflow, /cosign verify --key build_files\/cosign\.pub[\s\S]*immutable_ref/);
assert.match(workflow, /sudo podman image inspect --format '\{\{\.Digest\}\}'/);
assert.match(workflow, /immutable_ref="\$\{image_repository\}@\$\{image_digest\}"/);
assert.match(workflow, /PAYLOAD_REF=\$\{immutable_ref\}/);
assert.match(workflow, /--registry-username "\$GH_USER" --registry-password "\$GH_TOKEN"/);
assert.ok(
  workflow.indexOf('- name: Verify signed Better On installer payload') < workflow.indexOf('- name: Build and publish the Anaconda installer environment'),
  'The Anaconda ISO must only embed a signed Better On payload.'
);
assert.match(workflow, /image_type:\s*bootc-generic-iso/);
assert.match(workflow, /bootc_installer_payload_ref:/);
assert.match(workflow, /variant: desktop[\s\S]*variant: nvidia/);
assert.match(workflow, /image_tag="nvidia-\$\{GITHUB_SHA\}"/);
assert.match(workflow, /PAYLOAD_REF=ghcr\.io\/\$\{image_owner\}\/\$\{image_name\}:\$\{image_tag\}/);
assert.match(workflow, /bootc_installer_payload_ref: \$\{\{ env\.PAYLOAD_REF \}\}/);
assert.match(workflow, /github\.event\.repository\.default_branch/);
assert.match(workflow, /GITHUB_REF_NAME.*DEFAULT_BRANCH/);
assert.match(imageWorkflow, /base_image: ghcr\.io\/ublue-os\/bazzite-nvidia:stable/);
assert.match(imageWorkflow, /run: bash build_files\/test-configure-signature-policy\.sh/);
assert.match(imageWorkflow, /apt-get install --yes jq podman shellcheck/);
assert.ok(
  imageWorkflow.indexOf('- name: Install Podman') < imageWorkflow.indexOf('- name: Check bootc signature policy configuration'),
  'The signature-policy fixture needs jq installed before it runs.'
);
assert.match(imageWorkflow, /Resolve immutable Bazzite base image/);
assert.match(imageWorkflow, /podman pull "\$BASE_IMAGE"/);
assert.match(imageWorkflow, /image_repository="\$\{BASE_IMAGE%%:\*\}"/);
assert.match(imageWorkflow, /cosign verify --new-bundle-format=false --key build_files\/bazzite-cosign\.pub "\$immutable_ref"/);
assert.match(imageWorkflow, /echo "BASE_IMAGE=\$\{immutable_ref\}" >> "\$GITHUB_ENV"/);
assert.match(bazziteKey, /^-----BEGIN PUBLIC KEY-----[\s\S]+-----END PUBLIC KEY-----\s*$/);
const parsedBazziteKey = createPublicKey(bazziteKey);
assert.equal(parsedBazziteKey.asymmetricKeyType, 'ec');
assert.equal(parsedBazziteKey.asymmetricKeyDetails.namedCurve, 'prime256v1');
assert.equal(
  createHash('sha256').update(parsedBazziteKey.export({ format: 'der', type: 'spki' })).digest('hex'),
  'efaae036a5afefcfe2c64686db0311ab64e57ae794be634d80c25286695336a6',
  'Pinned Bazzite signing key fingerprint changed.'
);
assert.ok(
  imageWorkflow.indexOf('cosign verify --new-bundle-format=false --key build_files/bazzite-cosign.pub') < imageWorkflow.indexOf('- name: Build bootc image'),
  'The Bazzite base must be signature-verified before it is used to build the OS.'
);
assert.match(imageWorkflow, /- name: Build bootc image\n        env:\n          IMAGE_SOURCE:/);
assert.match(imageWorkflow, /commit_tag="nvidia-\$\{GITHUB_SHA\}"/);
assert.match(imageWorkflow, /commit_tag="\$GITHUB_SHA"/);
assert.match(workflow, /image-builder-cli:latest@sha256:[0-9a-f]{64}/);
assert.match(workflow, /sha256sum\s+"\$\{iso_files\[0\]\}"\s+>\s+output\/SHA256SUMS/);
assert.match(workflow, /path:\s+output\//);

assert.match(installer, /^FROM quay\.io\/fedora\/fedora-bootc:45$/m);
assert.match(installer, /anaconda-webui/);
assert.match(installer, /inst\.webui/);
assert.match(installer, /interactive-defaults\.ks/);
assert.match(installer, /containers-storage:/);
assert.match(imageContainerfile, /dnf5 install -y[^\n]*\bjq\b/);
assert.ok(
  imageContainerfile.indexOf('dnf5 install -y') < imageContainerfile.indexOf('env IMAGE_REPOSITORY="$IMAGE_REPOSITORY" /usr/bin/bash /ctx/build.sh'),
  'jq must be installed in the final image before the signature policy is configured.'
);

for (const destructiveDirective of ['autopart', 'clearpart', 'ignoredisk', 'reqpart']) {
  assert.doesNotMatch(installer, new RegExp(`^\\s*${destructiveDirective}\\b`, 'm'));
}

console.log('Installer workflow uses the pinned generic ISO builder and an interactive Anaconda payload.');
