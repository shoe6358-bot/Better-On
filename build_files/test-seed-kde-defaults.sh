#!/usr/bin/bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
temporary_root=$(mktemp -d)
trap 'rm -rf "$temporary_root"' EXIT

# A fresh skeleton gets the Better On scheme and desktop style.
bash "$script_dir/seed-kde-defaults.sh" "$temporary_root/fresh"
grep -Fxq 'ColorScheme=BetterOn' "$temporary_root/fresh/etc/skel/.config/kdeglobals"
grep -Fxq 'name=better-on' "$temporary_root/fresh/etc/skel/.config/plasmarc"

# Existing base defaults and unrelated settings must be retained.
mkdir -p "$temporary_root/preserved/etc/skel/.config"
cat > "$temporary_root/preserved/etc/skel/.config/kdeglobals" <<'EOF'
[General]
ColorScheme=BreezeDark

[KDE]
SingleClick=false
EOF
cat > "$temporary_root/preserved/etc/skel/.config/plasmarc" <<'EOF'
[Theme]
name=breeze-dark

[Wallpapers]
fallbackWallpaper=upstream
EOF
bash "$script_dir/seed-kde-defaults.sh" "$temporary_root/preserved"
grep -Fxq 'ColorScheme=BreezeDark' "$temporary_root/preserved/etc/skel/.config/kdeglobals"
grep -Fxq 'SingleClick=false' "$temporary_root/preserved/etc/skel/.config/kdeglobals"
grep -Fxq 'name=breeze-dark' "$temporary_root/preserved/etc/skel/.config/plasmarc"
grep -Fxq 'fallbackWallpaper=upstream' "$temporary_root/preserved/etc/skel/.config/plasmarc"

# A missing key in an existing target group is added without dropping siblings.
mkdir -p "$temporary_root/partial/etc/skel/.config"
cat > "$temporary_root/partial/etc/skel/.config/kdeglobals" <<'EOF'
[General]
UseSingleClick=false

[KDE]
SingleClick=true
EOF
bash "$script_dir/seed-kde-defaults.sh" "$temporary_root/partial"
grep -Fxq 'ColorScheme=BetterOn' "$temporary_root/partial/etc/skel/.config/kdeglobals"
grep -Fxq 'UseSingleClick=false' "$temporary_root/partial/etc/skel/.config/kdeglobals"
grep -Fxq 'SingleClick=true' "$temporary_root/partial/etc/skel/.config/kdeglobals"

printf '%s\n' 'KDE default seeding checks passed.'
