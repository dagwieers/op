#!/usr/bin/sh

cpplargs=
cpplargs="${cpplargs} --extensions=c,h,l"

filters= sep=

# category list : cpplint.py --filter=
# ignore next line : // NOLINT(category)

filters="${filters}${sep}-build/include"; sep=,

filters="${filters}${sep}-legal/copyright"; sep=,

filters="${filters}${sep}-readability/braces"; sep=,
filters="${filters}${sep}-readability/casting"; sep=,

#filters="${filters}${sep}-runtime/int"; sep=,
filters="${filters}${sep}-runtime/threadsafe_fn"; sep=,

#filters="${filters}${sep}-whitespace/blank_line"; sep=,
filters="${filters}${sep}-whitespace/braces"; sep=,
#filters="${filters}${sep}-whitespace/comma"; sep=,
#filters="${filters}${sep}-whitespace/comments"; sep=,
#filters="${filters}${sep}-whitespace/end_of_line"; sep=,
#filters="${filters}${sep}-whitespace/indent"; sep=,
#filters="${filters}${sep}-whitespace/line_length"; sep=,
#filters="${filters}${sep}-whitespace/newline"; sep=,
#filters="${filters}${sep}-whitespace/operators"; sep=,
#filters="${filters}${sep}-whitespace/parens"; sep=,
#filters="${filters}${sep}-whitespace/semicolon"; sep=,
filters="${filters}${sep}-whitespace/tab"; sep=,

cpplargs="${cpplargs} --filter=${filters}"

cpplint.py $cpplargs "$@" 2>&1

exit

// cpplheck-suppress
//  memlink
//  unreadVariale
//  ignoredReturnValue
//  nullPointer
