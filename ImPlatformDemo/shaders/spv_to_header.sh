#!/bin/bash
# Convert SPIR-V binary to C++ header with uint32_t array

if [ $# -ne 2 ]; then
    echo "Usage: $0 <input.spv> <array_name>"
    exit 1
fi

INPUT=$1
ARRAY_NAME=$2
OUTPUT="${INPUT%.spv}.h"

# Get file size
SIZE=$(stat -c%s "$INPUT" 2>/dev/null || stat -f%z "$INPUT")

# Create header
cat > "$OUTPUT" << EOF
// Auto-generated from $INPUT
// Size: $SIZE bytes

#pragma once

static const unsigned int ${ARRAY_NAME}[] = {
EOF

# Convert binary to uint32_t array
xxd -p -c 4 "$INPUT" | while read line; do
    # Reverse byte order for little-endian uint32_t
    byte1=${line:6:2}
    byte2=${line:4:2}
    byte3=${line:2:2}
    byte4=${line:0:2}
    echo "    0x${byte1}${byte2}${byte3}${byte4},"
done >> "$OUTPUT"

# Remove trailing comma
sed -i '$ s/,$//' "$OUTPUT"

# Close array and add size
cat >> "$OUTPUT" << EOF
};

static const unsigned int ${ARRAY_NAME}_size = sizeof(${ARRAY_NAME});
EOF

echo "Generated $OUTPUT"
