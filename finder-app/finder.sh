#!/bin/bash

# Check if exactly two arguments are passed
if [ $# -ne 2 ]; then
  echo "Error: Two arguments required. Usage: finder.sh <directory> <search string>"
  exit 1
fi

filesdir=$1
searchstr=$2

# Check if the first argument is a valid directory
if [ ! -d "$filesdir" ]; then
  echo "Error: '$filesdir' is not a valid directory"
  exit 1
fi

# Count number of files
file_count=$(find "$filesdir" -type f | wc -l)

# Count number of matching lines
match_count=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

# Output the result in the format expected by the tester script
echo "The number of files are $file_count and the number of matching lines are $match_count"