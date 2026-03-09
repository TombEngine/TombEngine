#!/bin/bash
# TombEngine launcher script.
# Sets up library paths and launches the engine from the correct directory.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENGINE_DIR="$SCRIPT_DIR/Engine/Linux"

export LD_LIBRARY_PATH="$ENGINE_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export VLC_PLUGIN_PATH="$ENGINE_DIR/plugins"

# If -gamedir is not specified, use the directory containing this script.
HAS_GAMEDIR=0
for arg in "$@"; do
    if [ "$arg" = "-gamedir" ]; then
        HAS_GAMEDIR=1
        break
    fi
done

if [ "$HAS_GAMEDIR" -eq 0 ]; then
    exec "$ENGINE_DIR/TombEngine" -gamedir "$SCRIPT_DIR" "$@"
else
    exec "$ENGINE_DIR/TombEngine" "$@"
fi
