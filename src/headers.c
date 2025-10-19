/*
 * Copyright 1993, 2000 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * headers.c - handle #includes in source files
 *
 * Using regular expressions provided as the variable $(HDRSCAN),
 * headers() searches a file for #include files and phonies up a
 * rule invocation:
 *
 *	$(HDRRULE) <target> : <include files> ;
 *
 * External routines:
 *    headers() - scan a target for include files and call HDRRULE
 *
 * Internal routines:
 *    headers1() - using regexp, scan a file and build include LIST
 *
 * 04/13/94 (seiwald) - added shorthand L0 for null list pointer
 * 09/10/00 (seiwald) - replaced call to compile_rule with evaluate_rule,
 *		so that headers() doesn't have to mock up a parse structure
 *		just to invoke a rule.
 * 03/02/02 (seiwald) - rules can be invoked via variable names
 * 10/22/02 (seiwald) - list_new() now does its own newstr()/copystr()
 * 11/04/02 (seiwald) - const-ing for string literals
 * 12/09/02 (seiwald) - push regexp creation down to headers1().
 */

# include "jam.h"
# include "lists.h"
# include "parse.h"
# include "compile.h"
# include "rules.h"
# include "variable.h"
# include "regexp.h"
# include "headers.h"
# include "newstr.h"
# include "hash.h"

#ifdef OPT_HEADER_CACHE_EXT
# include "hcache.h"
#endif

#ifndef OPT_HEADER_CACHE_EXT
static LIST *headers1( const char *file, LIST *hdrscan );
#endif

#ifdef OPT_HDRPIPE_EXT
# include "buffer.h"
# include "filesys.h"
#endif

#include "jsmn.h"

/*
 * headers() - scan a target for include files and call HDRRULE
 */

# define MAXINC 10

void
headers( TARGET *t, int phase )
{
	LIST	*hdrscan;
	LIST	*hdrrule;
	LIST	*hdrsource;
	TARGET *sourcet;
	LOL	lol;

	if ( t->scannedheaders )
	{
		return;
	}

	if( !list_first( hdrscan = var_get( "HDRSCAN" ) ) ||
	    !list_first( hdrrule = var_get( "HDRRULE" ) ) )
	        return;

	t->scannedheaders = 1;

	/* Doctor up call to HDRRULE rule */
	/* Call headers1() to get LIST of included files. */

	if ( 0 ) //t->includes )
	{
		targetlist_free( t->includes->depends );
		t->includes->depends = NULL;
	}

	hdrsource = var_get( "HDRSOURCE" );
	if ( hdrsource )
	{
		sourcet = bindtarget( list_value( list_first( hdrsource ) ) );
	}
	else
	{
		sourcet = t;
	}

	if( DEBUG_HEADER )
	    printf( "header scan %s\n", t->name );

	lol_init( &lol );

	lol_add( &lol, list_append( L0, sourcet->name, 1 ) );
#ifdef OPT_HEADER_CACHE_EXT
	lol_add( &lol, hcache( t, hdrscan, phase ) );
#else
	lol_add( &lol, headers1( t->boundname, hdrscan ) );
#endif

	if( list_first(lol_get( &lol, 1 )) )
	{
#ifdef OPT_HDRRULE_BOUNDNAME_ARG_EXT
	    /* The third argument to HDRRULE is the bound name of
	     * $(<) */
	    lol_add( &lol, list_append( L0, sourcet->boundname, 0 ) );
#endif
	    list_free( evaluate_rule( list_value(list_first(hdrrule)), &lol, L0 ) );
	}

	addsettings( t->settings, VAR_SET, "HDRPROCESSED", list_append( L0, "1", 0 ) );

	/* Clean up */

	lol_free( &lol );
}

LIST* headerscan( TARGET *t )
{
	LIST	*hdrscan;
	LIST	*list;

	if( !list_first( hdrscan = var_get( "HDRSCAN" ) ) )
		return L0;

	/* Call headers1() to get LIST of included files. */

	if( DEBUG_HEADER )
		printf( "header scan %s\n", t->name );

#ifdef OPT_HEADER_CACHE_EXT
	list = hcache( t, hdrscan, 0 );
#else
	list = headers1( t, t->boundname, hdrscan );
#endif

	return list;
}

#ifdef OPT_HDRPIPE_EXT

extern struct hash *regexhash;

typedef struct
{
    const char *name;
    regexp *re;
} headers_regexdata;

/* OPT_HDRPIPE_EXT -- http://maillist.perforce.com/pipermail/jamming/2002-June/001717.html */

/*
 * headers1() - using regexp, scan a file and build include LIST
 */

static LIST *headers1helper(
	FILE *f,
	LIST *hdrscan )
{
	int	i;
	int	rec = 0;
	LIST	*result = 0;
	regexp	*re[ MAXINC ];
	char	buf[ 1024 ];
	LIST	*hdrdownshift;
	int	dodownshift = 0;
	LIST	*hdrforceforwardslash;
	int	doforwardslash = 1;
	LISTITEM* pattern;

#ifdef OPT_IMPROVED_PATIENCE_EXT
	static int count = 0;
	++count;
	if ( ((count == 100) || !( count % 1000 )) && DEBUG_MAKE )
	    printf("*** patience...\n");
#endif

	hdrdownshift = var_get( "HDRDOWNSHIFT" );
	if ( list_first(hdrdownshift) )
	{
		char const* str = list_value(list_first(hdrdownshift));
	    dodownshift = strcmp( str, "false" ) != 0  &&
		    strcmp( str, "0" ) != 0;
	}
	hdrforceforwardslash = var_get( "HDRFORCEFORWARDSLASH" );
	if ( list_first(hdrforceforwardslash) )
	{
		char const* str = list_value(list_first(hdrforceforwardslash));
	    doforwardslash = strcmp( str, "false" ) != 0  &&
		    strcmp( str, "0" ) != 0;
	}

	if ( !regexhash )
	    regexhash = hashinit( sizeof(headers_regexdata), "regex" );

	pattern = list_first(hdrscan);
	while( rec < MAXINC && pattern )
	{
	    headers_regexdata data, *d = &data;
	    data.name = list_value(pattern);
	    if( !hashcheck( regexhash, (HASHDATA **)&d ) )
	    {
		d->re = jam_regcomp( list_value(pattern) );
		(void)hashenter( regexhash, (HASHDATA **)&d );
	    }
	    re[rec++] = d->re;
	    pattern = list_next( pattern );
	}

	while( fgets( buf, sizeof( buf ), f ) )
	{
	    for( i = 0; i < rec; i++ )
		if( jam_regexec( re[i], buf ) && re[i]->startp[1] )
	    {
		/* Copy and terminate extracted string. */

		char buf2[ MAXSYM ];
		int l = (int)(re[i]->endp[1] - re[i]->startp[1]);
		if (doforwardslash)
		{
			const char* target = re[i]->startp[1];
			char* p = buf2;

			while (l > 0)
			{
				char ch = *target++;
				if (ch == '\\')
				{
					*p++ = '/';
					if (*target == '\\')
					{
						++target;
						--l;
					}
				}
				else
				{
					*p++ = ch;
				}
				--l;
			}

			*p = 0;
		}
		else
		{
			memcpy( buf2, re[i]->startp[1], l );
			buf2[ l ] = 0;
		}

# ifdef DOWNSHIFT_PATHS
		if ( dodownshift )
		{
			char* p = buf2;

			while (*p)
			{
				*p = (char)tolower(*p);
				++p;
			}
		}
# endif

		result = list_append( result, buf2, 0 );

		if( DEBUG_HEADER )
		    printf( "header found: %s\n", buf2 );
	    }
	}

	return result;
}

static int jsoneq(const char* json, jsmntok_t* tok, const char* s)
{
	if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
		strncmp(json + tok->start, s, tok->end - tok->start) == 0)
	{
		return 0;
	}
	return -1;
}

static LIST *vc_sourcedependencies_parser( const char *file, int *scansucceeded )
{
	jsmn_parser parser;
	int numtokens = -1;
	char *json = NULL;
	jsmntok_t *tokens = NULL;
	LIST *list = L0;

	FILE *f = fopen( file, "rb" );
	if ( f )
	{
		int size;
		int ret;
		fseek( f, 0, SEEK_END );
		size = ftell( f );
		fseek( f, 0, SEEK_SET );

		json = malloc( size );
		fread( json, size, 1, f );

		fclose( f );

		jsmn_init(&parser);
		numtokens = jsmn_parse(&parser, json, size, NULL, 0);
		if ( numtokens > 0 )
		{
			tokens = (jsmntok_t*)malloc( sizeof( jsmntok_t ) * numtokens );
			jsmn_init(&parser);
			ret = jsmn_parse(&parser, json, size, tokens, numtokens);
			if ( ret >= 0 )
			{
				int index;

				if ( tokens[ 0 ].type != JSMN_OBJECT )
				{
					list_free(list);
					goto done;
				}

				for ( index = 1; index < numtokens; ++index )
				{
					if (jsoneq(json, &tokens[index], "Version") == 0)
					{
						++index;
					}
					else if (jsoneq(json, &tokens[index], "Data") == 0)
					{
						int data_index;
						if (tokens[index + 1].type != JSMN_OBJECT)
						{
							list_free(list);
							goto done;
						}

						for ( data_index = index + 2; data_index < numtokens; ++data_index )
						{
							if (jsoneq(json, &tokens[data_index], "Source") == 0)
							{
								++data_index;
							}
							else if (jsoneq(json, &tokens[data_index], "ProvidedModule") == 0)
							{
								++data_index;
							}
							else if (jsoneq(json, &tokens[data_index], "Includes") == 0)
							{
								int includes_index;
								if (tokens[data_index + 1].type != JSMN_ARRAY)
								{
									list_free(list);
									goto done;
								}
								for (includes_index = 0; includes_index < tokens[data_index + 1].size; includes_index++) {
									int pos;
									jsmntok_t* token = &tokens[data_index + includes_index + 2];
									BUFFER buff;
									buffer_init(&buff);
									for (pos = token->start; pos < token->end; ++pos)
									{
										char ch = json[pos];
										if (ch == '\\')
										{
											buffer_addchar(&buff, '/');
											ch = '/';
											if (pos < token->end  &&  json[pos + 1] == '\\')
											{
												++pos;
											}
										}
										else
										{
											buffer_addchar(&buff, tolower(ch));
										}
									}
									buffer_addchar(&buff, 0);
									list = list_append(list, buffer_ptr(&buff), 0);
									buffer_free(&buff);
								}
								data_index += tokens[data_index + 1].size + 1;
							}
						}
						index = data_index;
					}
				}
			}
		}

		if ( scansucceeded )
		{
			*scansucceeded = 1;
		}
	}

done:
	free( json );
	free( tokens );
	return list;
}


static LIST *gcc_clang_dependencies_parser( const char *file, int *scansucceeded )
{
	char *buffer = NULL;
	LIST *list = L0;

	FILE *f = fopen( file, "rb" );
	if ( f )
	{
		char *ptr = NULL;
		BUFFER buff;
		int phase = 0;

		int size;
		fseek( f, 0, SEEK_END );
		size = ftell( f );
		fseek( f, 0, SEEK_SET );

		buffer = malloc( size );
		fread( buffer, size, 1, f );

		fclose( f );

		ptr = buffer;

		while (ptr < buffer + size)
		{
			switch (phase)
			{
				case 0:
				{
					// Search for the main file line end.
					if (*ptr == ':'  &&  *(ptr + 1) == ' ')
					{
						phase = 1;
					}
					++ptr;
					break;
				}
				case 1:
				{
					if (*ptr == ' '  ||  *ptr == '\\'  ||  *ptr == '\r'  ||  *ptr == '\n')
					{
						++ptr;
						continue;
					}

					phase = 2;
					buffer_init(&buff);
					break;
				}
				case 2:
				{
					if (*ptr == ' '  ||  *ptr == '\r'  ||  *ptr == '\n')
					{
						++ptr;
						buffer_addchar(&buff, 0);
						list = list_append(list, buffer_ptr(&buff), 0);
						buffer_free(&buff);
						phase = 1;
						continue;
					}

					if (*ptr == '\\')
					{
						if (*(ptr + 1) == ' ')
						{
							buffer_addchar(&buff, ' ');
							ptr += 2;
							continue;
						}
						buffer_addchar(&buff, '/');
						++ptr;
						continue;
					}

					//buffer_addchar(&buff, tolower(*ptr++));
					buffer_addchar(&buff, *ptr++);
				}
			}
		}

		if (buffer_size(&buff) > 0)
		{
			buffer_addchar(&buff, 0);
			list = list_append(list, buffer_ptr(&buff), 0);
			buffer_free(&buff);
		}

		if ( scansucceeded )
		{
			*scansucceeded = 1;
		}
	}

	free( buffer );
	return list;
}

LIST *
headers1(
	TARGET *t,
	const char *file,
	LIST *hdrscan,
	int *scansucceeded,
	int phase )
{
	FILE	*f;
	LIST	*result = 0;
	LIST    *hdrpipe;
	LIST	*hdrpipefile;
	LIST	*hdrvcdepfile;
	LIST	*hdrdepfile;

	if ( scansucceeded )
	{
		*scansucceeded = 0;
	}


	if ( list_first(hdrpipe = var_get( "HDRPIPE" )) )
	{
		LOL args;
		BUFFER buff;
		lol_init( &args );
		lol_add( &args, list_append( L0, file, 0 ) );
		buffer_init( &buff );
		if ( var_string( list_value(list_first(hdrpipe)), &buff, 0, &args, ' ') < 0 )  {
		    printf( "Cannot expand HDRPIPE '%s' !\n", list_value(list_first(hdrpipe)) );
		    exit( EXITBAD );
		}
		buffer_addchar( &buff, 0 );
		if ( !( f = file_popen( (const char*)buffer_ptr( &buff ), "r" ) ) ) {
		    buffer_free( &buff );
		    return result;
		}
		buffer_free( &buff );
		lol_free( &args );

		result = headers1helper( f, hdrscan );

		if ( list_first(hdrpipe) )
			file_pclose( f );

		if ( list_first(hdrpipefile = var_get( "HDRPIPEFILE" )) )
		{
			if( !( f = fopen( list_value(list_first(hdrpipefile)), "r" ) ) )
				return result;
			result = headers1helper( f, hdrscan );
			fclose( f );
		}

		if (scansucceeded)
		{
			*scansucceeded = 1;
		}

		return result;
	}
	else if ( list_first( hdrvcdepfile = var_get( "HDRVCDEPFILE" ) ) )
	{
		if ( phase == 1 )
		{
			result = vc_sourcedependencies_parser( list_value( list_first( hdrvcdepfile ) ), scansucceeded );
		}
		return result;
	}
	else if ( list_first( hdrdepfile = var_get( "HDRDEPFILE" ) ) )
	{
		if ( phase == 1 )
		{
			result = gcc_clang_dependencies_parser( list_value( list_first( hdrdepfile ) ), scansucceeded );
		}
		return result;
	}
	else if ( list_first(hdrpipefile = var_get( "HDRPIPEFILE" )) )
	{
		if( f = fopen( list_value(list_first(hdrpipefile)), "r" ) )
		{
			result = headers1helper( f, hdrscan );
			fclose( f );

			if (scansucceeded)
			{
				*scansucceeded = 1;
			}
		}

		return result;
	}

	if( !( f = fopen( file, "r" ) ) )
		return result;

	result = headers1helper( f, hdrscan );

	fclose( f );

	if (scansucceeded)
	{
		*scansucceeded = 1;
	}

	return result;
}


time_t headers_depfiletime( TARGET *t )
{
	SETTINGS* settings;
	settings = quicksettingslookup( t, "HDRVCDEPFILE" );
	if ( !settings )
	{
		settings = quicksettingslookup( t, "HDRDEPFILE" );
	}
	if ( !settings )
	{
		settings = quicksettingslookup( t, "HDRPIPEFILE" );
	}
	if ( !settings )
	{
		return -1;
	}
	if ( settings->value )
	{
		time_t t;
		if ( file_time( list_value( list_first( settings->value ) ), &t ) != -1 )
		{
			return t;
		}
	}
	return -1;
}


void headers_removedepfile( TARGET *t )
{
	SETTINGS* settings;
	settings = quicksettingslookup( t, "HDRVCDEPFILE" );
	if ( !settings )
	{
		settings = quicksettingslookup( t, "HDRDEPFILE" );
	}
	if ( !settings || !settings->value )
	{
		return;
	}

	unlink( list_value( list_first( settings->value ) ) );
}


#else

struct hash *regexhash;

typedef struct
{
    const char *name;
    regexp *re;
} headers_regexdata;

#ifndef OPT_HEADER_CACHE_EXT
static	/* Needs to be global if header caching is on */
#endif
LIST *
headers1(
	const char *file,
	LIST *hdrscan )
{
	FILE	*f;
	int	i;
	int	rec = 0;
	LIST	*result = 0;
	LISTITEM* pattern;
	regexp	*re[ MAXINC ];
	char	buf[ 1024 ];

#ifdef OPT_IMPROVED_PATIENCE_EXT
	static int count = 0;
	++count;
	if ( ((count == 100) || !( count % 1000 )) && DEBUG_MAKE )
	    printf("*** patience...\n");
#endif

	if( !( f = fopen( file, "r" ) ) )
	    return result;

	if ( !regexhash )
	    regexhash = hashinit( sizeof(headers_regexdata), "regex" );

	pattern = list_first(hdrscan);
	while( rec < MAXINC && pattern )
	{
	    headers_regexdata data, *d = &data;
	    data.name = list_value(pattern);
	    if( !hashcheck( regexhash, (HASHDATA **)&d ) )
	    {
		d->re = jam_regcomp( hdrscan->string );
		(void)hashenter( regexhash, (HASHDATA **)&d );
	    }
	    re[rec++] = d->re;
		pattern = list_next(pattern);
	}

	while( fgets( buf, sizeof( buf ), f ) )
	{
	    for( i = 0; i < rec; i++ )
		if( jam_regexec( re[i], buf ) && re[i]->startp[1] )
	    {
		/* Copy and terminate extracted string. */

		char buf2[ MAXSYM ];
		int l = re[i]->endp[1] - re[i]->startp[1];
# ifdef DOWNSHIFT_PATHS
		const char *target = re[i]->startp[1];
		char *p = buf2;

		do *p++ = tolower( *target++ );
		while( --l );

		*p = 0;
#else
		memcpy( buf2, re[i]->startp[1], l );
		buf2[ l ] = 0;
# endif
		result = list_append( result, buf2, 0 );

		if( DEBUG_HEADER )
		    printf( "header found: %s\n", buf2 );
	    }
	}

/*	while( rec )
	    free( (char *)re[--rec] );
*/
	fclose( f );

	return result;
}

#endif
