#!/bin/bash

if [ $# -ne 2 ]; then
  echo "Error: Two arguments required. Usage: finder.sh <directory> <search string>"
  exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d "$filesdir" ]; then
  echo "Error: '$filesdir' is not a valid directory"
  exit 1
fi

make clean
make
if [ $? -ne 0 ]; then
  echo "Error: Build failed"
  exit 1
fi

file_count=$(find "$filesdir" -type f | wc -l)
match_count=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

result="The number of files are $file_count and the number of matching lines are $match_count"

output_file="/tmp/assignment2/result.txt"
mkdir -p "$(dirname "$output_file")"

./writer "$output_file" "$result"

if [ $? -ne 0 ]; then
  echo "Error: writer failed"
  exit 1
fi

echo "$result"
