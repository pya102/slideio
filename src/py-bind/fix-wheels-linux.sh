#!/bin/bash
set -e

# Check if the directory parameter is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <directory_with_wheel_files>"
    exit 1
fi

# Directory with the wheel files
wheel_dir="$1"
fixed_dir="$wheel_dir/fixed"
if [ ! -d "$fixed_dir" ]; then
    mkdir -p "$fixed_dir"
fi

find $fixed_dir -type f -name "*.whl" -delete

# Iterate through all *.wheel files in the specified directory
for wheel in "$wheel_dir"/*.whl; do
    echo "Processing $wheel file..."
    # Create a directory for the wheel file content
    dir="${wheel%.whl}"
    echo "Creating directory $dir"
    mkdir -p "$dir"
    
    # Unzip the wheel file content into the directory
    unzip "$wheel" -d "$dir"
    
    # Find and set rpath for all *.so files in the directory
    find "$dir" -name "*.so" -exec patchelf --set-rpath '$ORIGIN:$ORIGIN/slideio.libs' {} \;
    
    # Find and set rpath for all files in the slideio.libs subdirectory
    if [ -d "$dir/slideio.libs" ]; then
        find "$dir/slideio.libs" -type f -exec patchelf --set-rpath '$ORIGIN' {} \;
    fi
      
    # Compress the directory back into a zip file with the original wheel file name
    (cd "$dir" && zip -r "$fixed_dir/$(basename "$wheel")" .)
    
    # Clean up the directory
    rm -rf "$dir"
done