import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const workflow = await readFile('.github/workflows/build-vm-image.yml', 'utf8');
const imageWorkflow = await readFile('.github/workflows/build.yml', 'utf8');

assert.match(workflow, /workflow_dispatch:/);
assert.match(workflow, /runs-on:\s*ubuntu-24\.04/);
assert.match(workflow, /variant: desktop[\s\S]*variant: nvidia/);
assert.match(workflow, /github\.event\.repository\.default_branch/);
assert.match(workflow, /cosign-installer@[0-9a-f]{40}/);
assert.match(workflow, /sudo podman image inspect --format '\{\{\.Digest\}\}'/);
assert.match(workflow, /cosign verify --key build_files\/cosign\.pub[\s\S]*immutable_ref/);
assert.match(workflow, /echo "PAYLOAD_REF=\$\{immutable_ref\}" >> "\$GITHUB_ENV"/);
assert.ok(
  workflow.indexOf('- name: Resolve and verify signed OS image') < workflow.indexOf('- name: Build bootable QCOW2 disk'),
  'The QCOW2 disk must be built from a verified immutable image digest.'
);
assert.match(workflow, /image_type: qcow2/);
assert.match(workflow, /bootc_default_fs: btrfs/);
assert.match(workflow, /image-builder-cli:latest@sha256:[0-9a-f]{64}/);
assert.match(workflow, /printf 'variant=%s\\nsource_image=%s\\n'.*PAYLOAD_REF/);
assert.match(workflow, /sha256sum "\$\{disk_files\[0\]\}" output\/BUILD-INFO\.txt > output\/SHA256SUMS/);
assert.match(workflow, /name: better-on-vm-\$\{\{ matrix\.variant \}\}/);
assert.match(workflow, /actions\/upload-artifact@[0-9a-f]{40}/);
assert.match(imageWorkflow, /node tests\/validate-vm-image-workflow\.mjs/);

console.log('VM image workflow verifies signed OS digests before building and checksumming QCOW2 artifacts.');
