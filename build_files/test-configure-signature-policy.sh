#!/usr/bin/bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
temporary_root=$(mktemp -d)
trap 'rm -rf "$temporary_root"' EXIT

public_key="$temporary_root/cosign.pub"
target="$temporary_root/image-root"
repository=ghcr.io/maddo/better-on
mkdir -p "$target/etc/containers"
printf '%s\n' '-----BEGIN PUBLIC KEY-----' 'test-key-material' '-----END PUBLIC KEY-----' > "$public_key"
cat > "$target/etc/containers/policy.json" <<'EOF'
{
  "default": [{ "type": "reject" }],
  "transports": {
    "docker": {
      "quay.io/other/image": [{ "type": "reject" }]
    }
  }
}
EOF

IMAGE_REPOSITORY="$repository" bash "$script_dir/configure-signature-policy.sh" "$public_key" "$target"
jq -e --arg repository "$repository" '
  .default == [{"type":"reject"}] and
  .transports.docker["quay.io/other/image"] == [{"type":"reject"}] and
  .transports.docker[$repository][0].type == "sigstoreSigned" and
  .transports.docker[$repository][0].signedIdentity.type == "matchRepository" and
  (.transports.docker[$repository][0].keyPath | endswith("/etc/pki/containers/better-on-cosign.pub"))
' "$target/etc/containers/policy.json" >/dev/null
cmp "$public_key" "$target/etc/pki/containers/better-on-cosign.pub"
grep -Fxq '    use-sigstore-attachments: true' "$target/etc/containers/registries.d/better-on-ghcr.yaml"
grep -Fq "  \"$repository\":" "$target/etc/containers/registries.d/better-on-ghcr.yaml"

# A permissive base policy must fail closed and must not be rewritten.
rm -f "$target/etc/pki/containers/better-on-cosign.pub"
cat > "$target/etc/containers/policy.json" <<'EOF'
{
  "default": [{ "type": "insecureAcceptAnything" }]
}
EOF
cp "$target/etc/containers/policy.json" "$temporary_root/permissive-policy.json"
if IMAGE_REPOSITORY="$repository" bash "$script_dir/configure-signature-policy.sh" "$public_key" "$target"; then
    echo "A permissive base signature policy was accepted." >&2
    exit 1
fi
cmp "$temporary_root/permissive-policy.json" "$target/etc/containers/policy.json"
test ! -e "$target/etc/pki/containers/better-on-cosign.pub"

# Local development without a project key leaves the base image untouched.
rm -f "$target/etc/pki/containers/better-on-cosign.pub" "$target/etc/containers/registries.d/better-on-ghcr.yaml"
cp "$target/etc/containers/policy.json" "$temporary_root/local-policy.json"
IMAGE_REPOSITORY= bash "$script_dir/configure-signature-policy.sh" "$temporary_root/missing.pub" "$target"
cmp "$temporary_root/local-policy.json" "$target/etc/containers/policy.json"

# A malformed repository must fail before adding trust to the target image.
invalid_target="$temporary_root/invalid-repository-root"
mkdir -p "$invalid_target/etc/containers"
printf '%s\n' '{ "default": [{ "type": "reject" }] }' > "$invalid_target/etc/containers/policy.json"
if IMAGE_REPOSITORY=ghcr.io/Upper/BetterOn bash "$script_dir/configure-signature-policy.sh" "$public_key" "$invalid_target"; then
    echo "An invalid GHCR repository was accepted." >&2
    exit 1
fi
test ! -e "$invalid_target/etc/pki/containers/better-on-cosign.pub"
test ! -e "$invalid_target/etc/containers/registries.d/better-on-ghcr.yaml"

# A missing base policy must fail rather than create an incomplete project policy.
missing_policy_target="$temporary_root/missing-policy-root"
mkdir -p "$missing_policy_target/etc/containers"
if IMAGE_REPOSITORY="$repository" bash "$script_dir/configure-signature-policy.sh" "$public_key" "$missing_policy_target"; then
    echo "A missing base signature policy was accepted." >&2
    exit 1
fi
test ! -e "$missing_policy_target/etc/pki/containers/better-on-cosign.pub"
test ! -e "$missing_policy_target/etc/containers/registries.d/better-on-ghcr.yaml"

printf '%s\n' 'Cosign image-policy configuration checks passed.'
