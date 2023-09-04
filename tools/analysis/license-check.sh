#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

if [ ! -e ./LICENSE ] || [ ! -e ./README.md ]; then
  echo "Please run this script from the project root!"
  exit 1
fi

SPDX_LICENSE_STR="SPDX-License-Identifier: $(head -n 1 LICENSE)"
echo 'Searching for SPDX expression "'"$SPDX_LICENSE_STR"'"'

DIRS="components doc dockerfiles include mk pyd3tn python-ud3tn-utils tools test"
FILES=$(find $DIRS -type f \( -name "*.[ch]" -o -name "*.options" -o -name "*.proto" -o -name "*.py" -o -name "*.rs" -o -name "*.sh" \) )
declare -a MISSING_SPDX_FILES

for file in $FILES; do
  if [[ "$file" == *"/generated/"* ]]; then
    # File generated from other source files - skip it.
    continue
  fi
  head -n 2 "$file" | grep -q --fixed-strings "$SPDX_LICENSE_STR"
  if [ $? -ne 0 ]; then
    MISSING_SPDX_FILES+=("$file")
  fi
done

grep -q --fixed-strings "$SPDX_LICENSE_STR" README.md
if [ $? -ne 0 ]; then
  MISSING_SPDX_FILES+=("README.md")
fi

if [ ${#MISSING_SPDX_FILES[@]} -gt 0 ]; then
  echo
  echo "SPDX expression not found in the following files:"
  echo
  printf '%s\n' "${MISSING_SPDX_FILES[@]}"
  exit 1
else
  echo "Looks good :-)"
fi

