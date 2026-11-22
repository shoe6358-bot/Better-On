#!/usr/bin/bash

set -euo pipefail

root="${1:-/}"

seed_kde_default() {
    local config_file="${root%/}$1"
    local group="$2"
    local key="$3"
    local value="$4"
    local assignment="${key}=${value}"

    mkdir -p "$(dirname "$config_file")"
    if [[ -f "$config_file" ]]; then
        local temporary_config
        temporary_config=$(mktemp "${config_file}.XXXXXX")
        awk -v target_group="[$group]" -v target_key="$key" -v setting="$assignment" '
            BEGIN { in_target = 0; seen_target = 0; has_setting = 0 }
            /^\[/ {
                if (in_target && !has_setting) {
                    print setting
                    has_setting = 1
                }
                in_target = ($0 == target_group)
                if (in_target) seen_target = 1
            }
            {
                if (in_target && index($0, target_key "=") == 1) has_setting = 1
                print
            }
            END {
                if (in_target && !has_setting) print setting
                if (!seen_target) print "\n" target_group "\n" setting
            }
        ' "$config_file" > "$temporary_config"
        chmod 0644 "$temporary_config"
        mv -f "$temporary_config" "$config_file"
    else
        printf '[%s]\n%s\n' "$group" "$assignment" > "$config_file"
        chmod 0644 "$config_file"
    fi
}

# Only the skeleton is changed; already-created user accounts are untouched.
seed_kde_default /etc/skel/.config/kdeglobals General ColorScheme BetterOn
seed_kde_default /etc/skel/.config/plasmarc Theme name better-on
