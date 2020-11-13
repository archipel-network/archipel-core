#!/bin/bash

CHECKPATCH_FLAGS="--no-tree --terse --show-types --ignore AVOID_EXTERNS \
--ignore COMPLEX_MACRO --ignore NEW_TYPEDEFS --ignore FUNCTION_ARGUMENTS \
--ignore SPDX_LICENSE_TAG --ignore LONG_LINE_STRING --ignore REPEATED_WORD \
--typedefsfile ./tools/analysis/stylecheck_typedefs.txt --emacs"

S=0

CHECKPATCH=./external/checkpatch/checkpatch.pl

check_dirs=(include components test/unit)

for check_dir in "${check_dirs[@]}"; do
	sub_dirs="$(find "$check_dir" -type d)"
	for dir in $sub_dirs; do
		cnt=`ls -1 $dir/*.c 2>/dev/null | wc -l`
		if [ $cnt != 0 ]
		then
			echo "Checking $dir/*.c..."
			$CHECKPATCH $CHECKPATCH_FLAGS --file $dir/*.c
			S=$(($S + $?))
		fi
		cnt=`ls -1 $dir/*.h 2>/dev/null | wc -l`
		if [ $cnt != 0 ]
		then
			echo "Checking $dir/*.h..."
			$CHECKPATCH $CHECKPATCH_FLAGS --file $dir/*.h
			S=$(($S + $?))
		fi
	done
done

exit $S
