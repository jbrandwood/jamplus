/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * headers.h - handle #includes in source files
 */

void headers( TARGET *t, int phase );

LIST* headerscan( TARGET *t );

#ifdef OPT_HEADER_CACHE_EXT
LIST *headers1( TARGET *t, const char *file, LIST *hdrscan, int *scansucceeded, int phase );
#endif

time_t headers_depfiletime( TARGET *t );
void headers_removedepfile( TARGET *t );
