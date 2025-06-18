#!/bin/bash

# Check if exactly two arguments are passed
if [ $# -ne 2 ]; then
  echo "Error: Two arguments required. Usage: writer.sh <file path> <string to write>"
  exit 1
fi

writefile=$1
writestr=$2

# Extract directory path and create it if it doesn't exist
writedir=$(dirname "$writefile")
mkdir -p "$writedir"

# Try to write the string to the file
echo "$writestr" > "$writefile"

# Check if writing succeeded
if [ $? -ne 0 ]; then
  echo "Error: Could not write to file '$writefile'"
  exit 1
fi
