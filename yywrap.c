#include "config.h"
#ifndef HAVE_LIBL
#ifndef HAVE_LIBFL
int yywrap(void);
int yywrap(void)
{
    return 1;
}
#endif
#endif
