#!/bin/sh
# Downloads the pinned Slang release for this OS (Linux, macOS) into extern/slang and prints the path of slangc.
# Does nothing when that version is already there. Needs curl, tar and sha256sum or shasum.
set -e

VERSION=2026.22

cd "$(dirname "$0")/.."
DEST=extern/slang

if [ -x "$DEST/bin/slangc" ] && [ "$(cat "$DEST/VERSION" 2>/dev/null)" = "$VERSION" ]; then
    echo "$PWD/$DEST/bin/slangc"
    exit 0
fi

# SHA-256 from https://github.com/shader-slang/slang/releases/tag/v2026.22
# tar.gz keeps the executable bits and the library symlinks; the glibc-2.28 builds run on older distributions.
case "$(uname -s)-$(uname -m)" in
    Linux-x86_64)                 ASSET=linux-x86_64-glibc-2.28.tar.gz;  SHA=35c765d20b69f6217b02848cdb82a5216bf03990044a8991ceb0b89a198a80fb ;;
    Linux-aarch64|Linux-arm64)    ASSET=linux-aarch64-glibc-2.28.tar.gz; SHA=08a9690c41a7624cf9f091ab1e5d5f02fa8a6891c57a635b5698386581e8e826 ;;
    Darwin-x86_64)                ASSET=macos-x86_64.tar.gz;             SHA=baad48f7432c1826a84a396d9bd6a2ebc3c9ceb395a187baad49d29066fbcd02 ;;
    Darwin-arm64)                 ASSET=macos-aarch64.tar.gz;            SHA=885b5bd9a79a1ae81fdb92212f5f7c785001f870d45c39f9bb5df01339db3fea ;;
    *) echo "Error: no Slang release for $(uname -s) $(uname -m)" >&2; exit 1 ;;
esac

URL="https://github.com/shader-slang/slang/releases/download/v$VERSION/slang-$VERSION-$ASSET"
ARCHIVE="extern/slang-$VERSION-$ASSET"
mkdir -p extern
echo "Downloading $URL" >&2
curl -fL --retry 3 -o "$ARCHIVE" "$URL"

if command -v sha256sum >/dev/null 2>&1; then
    GOT=$(sha256sum "$ARCHIVE" | cut -d' ' -f1)
else
    GOT=$(shasum -a 256 "$ARCHIVE" | cut -d' ' -f1)
fi
if [ "$GOT" != "$SHA" ]; then
    rm -f "$ARCHIVE"
    echo "Error: SHA-256 mismatch for $URL" >&2
    exit 1
fi

# A previous version's files must not mix with the new ones
rm -rf "$DEST"
mkdir -p "$DEST"
tar -xzf "$ARCHIVE" -C "$DEST"
rm -f "$ARCHIVE"
echo "$VERSION" > "$DEST/VERSION"
echo "$PWD/$DEST/bin/slangc"
