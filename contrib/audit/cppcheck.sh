#!/usr/bin/sh

top=${0%/*}
top=${top:-.}

cppcargs=
cppcargs="${cppcargs} --check-library"
cppcargs="${cppcargs} --library=std"
# cppcargs="${cppcargs} --library=/usr/local/share/cppcheck/gnu.cfg"
cppcargs="${cppcargs} --include=/usr/local/"
cppcargs="${cppcargs} --include=/usr/lib/gcc/x86_64-redhat-linux/4.4.7/include/"
cppcargs="${cppcargs} --include=/usr/include/"
cppcargs="${cppcargs} --platform=unix64 --inconclusive --inline-suppr"
cppcargs="${cppcargs} --enable=information,style --error-exitcode=1"
cppcargs="${cppcargs} --suppress=missingIncludeSystem"
cppcargs="${cppcargs} --suppress=checkLibraryNoReturn"
cppcargs="${cppcargs} --suppress=unreachableCode"
cppcargs="${cppcargs} --template='{file}:{line}:{severity}:{id}:{message}'"

cppcargs="${cppcargs} --library=${top}/cppcheck.cfg"
cppcargs="${cppcargs} -DXAUTH=1" # -DHAVE_CONFIG_H

args= sep=; for arg; do args="$args$sep'$arg'"; sep=' '; done

eval cppcheck $cppcargs $args 2>&1

exit

// cppcheck-suppress
//  memlink
//  unreadVariale
//  ignoredReturnValue
//  nullPointer
