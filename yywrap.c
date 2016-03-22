#include <config.h>
#ifndef HAVE_LIBL
#ifndef HAVE_LIBFL
int yywrap()
{
    return 1;
}
#endif
#endif
