#!/usr/bin/bash

set -euo pipefail

# This is the single customization entry point for the immutable system image.
# Keep system_files as an overlay and avoid ad-hoc package/service changes here.
if [[ -d /ctx/system_files ]]; then
    cp -a /ctx/system_files/. /
fi

/usr/bin/bash /ctx/configure-signature-policy.sh /ctx/cosign.pub

# Seed KDE preferences for new accounts while preserving existing base defaults.
/usr/bin/bash /ctx/seed-kde-defaults.sh /

# No persistent services or update overrides are added here. The Containerfile
# adds pciutils for GPU model names and jq for image signature policy setup;
# the app falls back to DRM sysfs identifiers when lspci is unavailable.
# Welcome, Hub, and Gaming executables are built in an earlier stage and copied in.
