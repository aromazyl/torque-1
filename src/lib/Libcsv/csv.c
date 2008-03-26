/* Copyright 2008 Cluster Resources */

/* A small library of functions dealing with comma separated variable strings.
 * This library is designed to work with small lists of 10 or so items.
 * It does not scale to larger lists which would require some kind of pointer
 * array to the list items with the associated parsing and construction of
 * this structure, etc.
 */

#include <string.h>

/**
 * Gets the number of items in a string list.
 * @param csv_str  The string list.
 * @return The number of items in the list.
 */
int csv_length( char *csv_str )
{
	int		length = 0;
	char	*cp;

	if (!csv_str || *csv_str == 0)
		return(0);

	length++;
	cp = csv_str;
	while ((cp = strchr(cp, ',')))
	{
		cp++;
		length++;
	}
	return(length);
}

/**
 * Gets the nth item from a comma seperated list of names.
 * @param csv_str  The string list.
 * @param n The item number requested (0 is the first item).
 * @return Null if csv_str is null or empty,
 *     otherwise, a pointer to a local buffer containing the nth item.
 */
char *csv_nth( char *csv_str, int n )
{
	int		i;
	char	*cp;
	char	*tp;
static	char	buffer[128];

	if (!csv_str || *csv_str == 0)
		return(0);

	cp = csv_str;
	for (i = 0; i < n; i++)
	{
		if (!(cp = strchr(cp, ',')))
		{
			return(0);
		}
		cp++;
	}
	memset(buffer, 0, sizeof(buffer));
	if ((tp = strchr(cp, ',')))
	{
		strncpy(buffer, cp, tp-cp);
	}
	else
	{
		strcpy(buffer, cp);
	}
	return(buffer);
}


/**
 *
 * csv_find_string
 *
 * Search a csv list for an entry that matches a specified search string.
 */
char *
csv_find_string( char *csv_str, char *search_str )
{
	int		i;
	int		nitems;
	int		search_length = 0;
	char *cp;

	if (!search_str)
		return(NULL);
	search_length = strlen(search_str);
	nitems = csv_length( csv_str );
	for (i=0; i<nitems; i++)
	{
		cp = csv_nth( csv_str, i );
		if (cp)
		{
			if (!strncmp(cp,search_str,search_length))
				return(cp);
		}
	}
	return(NULL);
}

/**
 *
 * csv_find_value
 *
 * Search a csv list for an entry that matches a specified search string.
 * Assuming that this is entry of the form "attribute=value", return a
 * pointer to the start of the value string.
 */
char *
csv_find_value( char *csv_str, char *search_str )
{
	char *cp;
	char *vp;

	cp = csv_find_string( csv_str, search_str );
	if (cp)
	{
		if ((vp = strchr(cp,'=')))
			return(vp+1);
	}
	return(NULL);
}



