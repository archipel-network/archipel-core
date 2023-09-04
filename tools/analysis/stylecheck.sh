#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0

CHECKPATCH_FLAGS="--no-tree --terse --show-types --ignore AVOID_EXTERNS \
--ignore COMPLEX_MACRO --ignore NEW_TYPEDEFS --ignore FUNCTION_ARGUMENTS \
--ignore SPDX_LICENSE_TAG --ignore PREFER_DEFINED_ATTRIBUTE_MACRO --ignore REPEATED_WORD \
--ignore PREFER_KERNEL_TYPES --ignore LONG_LINE_STRING --ignore CAMELCASE \
--typedefsfile ./tools/analysis/stylecheck_typedefs.txt --emacs --file \
--max-line-length=100 \
--strict --ignore OPEN_ENDED_LINE --ignore PARENTHESIS_ALIGNMENT \
--ignore LINE_SPACING --ignore COMPARISON_TO_NULL --ignore BIT_MACRO \
--ignore UNNECESSARY_PARENTHESES"

S=0

CHECKPATCH=./external/checkpatch/checkpatch.pl

check_dirs=(include components test/unit test/decoder)

for check_dir in "${check_dirs[@]}"; do
	sub_dirs="$(find "$check_dir" -type d -a -not -path "*/generated*")"
	for dir in $sub_dirs; do
		cnt=`ls -1 $dir/*.c 2>/dev/null | wc -l`
		if [ $cnt != 0 ]
		then
			echo "Checking $dir/*.c..."
			$CHECKPATCH $CHECKPATCH_FLAGS $dir/*.c
			S=$(($S + $?))
		fi
		cnt=`ls -1 $dir/*.h 2>/dev/null | wc -l`
		if [ $cnt != 0 ]
		then
			echo "Checking $dir/*.h..."
			$CHECKPATCH $CHECKPATCH_FLAGS $dir/*.h
			S=$(($S + $?))
		fi
	done
done

exit $S
