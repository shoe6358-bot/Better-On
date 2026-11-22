#!/usr/bin/bash

set -euo pipefail

public_key_source="${1:-/ctx/cosign.pub}"
repository="${IMAGE_REPOSITORY:-}"
target_root="${2:-/}"

target_path() {
    if [[ "$target_root" == / ]]; then
        printf '%s' "$1"
    else
        printf '%s%s' "${target_root%/}" "$1"
    fi
}

# Local development builds can omit signing configuration. Published builds
# provide the project public key and repository so bootc verifies that image.
if [[ ! -s "$public_key_source" ]]; then
    echo "No Better On cosign public key supplied; retaining the base image policy."
    exit 0
fi

if [[ ! "$repository" =~ ^ghcr\.io/[a-z0-9_.-]+/[a-z0-9_.-]+$ ]]; then
    echo "IMAGE_REPOSITORY must be a lowercase GHCR repository when cosign.pub is supplied." >&2
    exit 1
fi

key_path=$(target_path /etc/pki/containers/better-on-cosign.pub)
policy_path=$(target_path /etc/containers/policy.json)
mkdir -p "$(dirname "$policy_path")"
temporary_policy=$(mktemp "${policy_path}.XXXXXX")
trap 'rm -f "$temporary_policy"' EXIT

command -v jq >/dev/null || { echo "jq is required to configure the image signature policy." >&2; exit 1; }

if [[ ! -s "$policy_path" ]]; then
    echo "The Bazzite base image has no container signature policy; refusing to publish an image that cannot enforce signed bootc switches." >&2
    exit 1
fi

if ! jq -e '(.default | type == "array" and length > 0 and all(.[]; .type != "insecureAcceptAnything"))' "$policy_path" >/dev/null; then
    echo "The base image default container policy does not require signatures; refusing to publish an image that cannot enforce signed bootc switches." >&2
    exit 1
fi

install -D -m 0644 "$public_key_source" "$key_path"
jq --arg repository "$repository" --arg key_path "$key_path" '
    .transports = (.transports // {}) |
    .transports.docker = (.transports.docker // {}) |
    .transports.docker[$repository] = [{
        "type": "sigstoreSigned",
        "keyPath": $key_path,
        "signedIdentity": { "type": "matchRepository" }
    }]
' "$policy_path" > "$temporary_policy"

chmod 0644 "$temporary_policy"
mv -f "$temporary_policy" "$policy_path"
trap - EXIT

registry_policy=$(target_path /etc/containers/registries.d/better-on-ghcr.yaml)
mkdir -p "$(dirname "$registry_policy")"
printf 'docker:\n  "%s":\n    use-sigstore-attachments: true\n' "$repository" > "$registry_policy"
chmod 0644 "$registry_policy"
