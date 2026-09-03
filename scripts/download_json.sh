#!/usr/bin/env bash
set -euo pipefail

URL="https://raw.githubusercontent.com/nlohmann/json/v3.11.2/single_include/nlohmann/json.hpp"
DEST="third_party/nlohmann-json/single_include/nlohmann/json.hpp"

mkdir -p "$(dirname "$DEST")"

echo "Downloading nlohmann/json.hpp from $URL to $DEST"

if command -v curl >/dev/null 2>&1; then
  curl -fsSL "$URL" -o "$DEST"
elif command -v wget >/dev/null 2>&1; then
  wget -qO "$DEST" "$URL"
else
  echo "Neither curl nor wget found. Please download $URL to $DEST manually." >&2
  exit 1
fi

echo "Downloaded successfully."
