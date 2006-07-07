/*
 * print.c	Routines to print stuff.
 *
 * Version:	$Id$
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Copyright 2000  The FreeRADIUS server project
 */

static const char rcsid[] = "$Id$";

#include	<freeradius-devel/autoconf.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<ctype.h>
#include	<string.h>

#include	<freeradius-devel/missing.h>
#include	<freeradius-devel/libradius.h>

/*
 *	Convert a string to something printable.
 *	The output string has to be _at least_ 4x the size
 *	of the input string!
 */
void librad_safeprint(char *in, int inlen, char *out, int outlen)
{
	unsigned char	*str = (unsigned char *)in;
	int		done = 0;
	int		sp = 0;

	if (inlen < 0) inlen = strlen(in);

	while (inlen-- > 0 && (done + 3) < outlen) {
		/*
		 *	Hack: never print trailing zero.
		 *	Some clients send strings with an off-by-one
		 *	length (confused with strings in C).
		 */
		if (inlen == 0 && *str == 0)
			break;

		sp = 0;

		switch (*str) {
			case '\\':
				sp = '\\';
				break;
			case '\r':
				sp = 'r';
				break;
			case '\n':
				sp = 'n';
				break;
			case '\t':
				sp = 't';
				break;
			case '"':
				sp = '"';
				break;
			default:
			  if (*str < 32 || (*str >= 128)){
					snprintf(out, outlen, "\\%03o", *str);
					done += 4;
					out  += 4;
					outlen -= 4;
				} else {
					*out++ = *str;
					outlen--;
					done++;
				}
		}
		if (sp) {
			*out++ = '\\';
			*out++ = sp;
			outlen -= 2;
			done += 2;
		}
		str++;
	}
	*out = 0;
}


/*
 *  Print one value into a string.
 *  delimitst will define if strings and dates will be delimited by '"'
 */
int vp_prints_value(char * out, int outlen, VALUE_PAIR *vp, int delimitst)
{
	DICT_VALUE  *v;
	char        buf[1024];
	const char  *a = NULL;
	size_t      len;
	time_t      t;
	struct tm   s_tm;

	out[0] = '\0';
	if (!vp) return 0;

	switch (vp->type) {
		case PW_TYPE_STRING:
			if ((delimitst == 1) && vp->flags.has_tag) {
				/* Tagged attribute: print delimter and ignore tag */
				buf[0] = '"';
				librad_safeprint((char *)(vp->vp_strvalue),
						 vp->length, buf + 1, sizeof(buf) - 2);
				strcat(buf, "\"");
			} else if (delimitst == 1) {
				/* Non-tagged attribute: print delimter */
				buf[0] = '"';
				librad_safeprint((char *)vp->vp_strvalue,
						 vp->length, buf + 1, sizeof(buf) - 2);
				strcat(buf, "\"");

			} else if (delimitst < 0) {
				strNcpy(out, vp->vp_strvalue, outlen);
				return strlen(out);

			} else {
				/* Non-tagged attribute: no delimiter */
				librad_safeprint((char *)vp->vp_strvalue,
						 vp->length, buf, sizeof(buf));
			}
			a = buf;
			break;
		case PW_TYPE_INTEGER:
		        if ( vp->flags.has_tag ) {
			        /* Attribute value has a tag, need to ignore it */
			        if ((v = dict_valbyattr(vp->attribute, (vp->lvalue & 0xffffff)))
				    != NULL)
				        a = v->name;
				else {
				        snprintf(buf, sizeof(buf), "%u", (vp->lvalue & 0xffffff));
				        a = buf;
				}
			} else {
			        /* Normal, non-tagged attribute */
			        if ((v = dict_valbyattr(vp->attribute, vp->lvalue))
				    != NULL)
				        a = v->name;
				else {
				        snprintf(buf, sizeof(buf), "%u", vp->lvalue);
					a = buf;
				}
			}
			break;
		case PW_TYPE_DATE:
			t = vp->lvalue;
			if (delimitst) {
			  len = strftime(buf, sizeof(buf), "\"%b %e %Y %H:%M:%S %Z\"",
					 localtime_r(&t, &s_tm));
			} else {
			  len = strftime(buf, sizeof(buf), "%b %e %Y %H:%M:%S %Z",
					 localtime_r(&t, &s_tm));
			}
			if (len > 0) a = buf;
			break;
		case PW_TYPE_IPADDR:
			a = inet_ntop(AF_INET, &(vp->lvalue),
				      buf, sizeof(buf));
			break;
		case PW_TYPE_ABINARY:
#ifdef ASCEND_BINARY
			a = buf;
			print_abinary(vp, buf, sizeof(buf));
			break;
#else
		  /* FALL THROUGH */
#endif
		case PW_TYPE_OCTETS:
			if (outlen <= (2 * (vp->length + 1))) return 0;

			strcpy(buf, "0x");
			
			lrad_bin2hex(vp->vp_octets, buf + 2, vp->length);
			a = buf;
		  break;

		case PW_TYPE_IFID:
			a = ifid_ntoa(buf, sizeof(buf), vp->vp_octets);
			break;

		case PW_TYPE_IPV6ADDR:
			a = inet_ntop(AF_INET6,
				      (const struct in6_addr *) vp->vp_strvalue,
				      buf, sizeof(buf));
			break;

		case PW_TYPE_IPV6PREFIX:
		{
			struct in6_addr addr;

			/*
			 *	Alignment issues.
			 */
			memcpy(&addr, vp->vp_strvalue + 2, sizeof(addr));

			a = inet_ntop(AF_INET6, &addr, buf, sizeof(buf));
			if (a) {
				char *p = buf + strlen(buf);
				sprintf(p, "/%u", (unsigned int) vp->vp_strvalue[1]);
			}
		}
			break;

		default:
			a = "UNKNOWN-TYPE";
			break;
	}

	if (a != NULL) strlcpy(out, a, outlen);

	return strlen(out);
}

/*
 *  This is a hack, and has to be kept in sync with tokens.h
 */
static const char *vp_tokens[] = {
  "?",				/* T_OP_INVALID */
  "EOL",			/* T_EOL */
  "{",
  "}",
  "(",
  ")",
  ",",
  ";",
  "+=",
  "-=",
  ":=",
  "=",
  "!=",
  ">=",
  ">",
  "<=",
  "<",
  "=~",
  "!~",
  "=*",
  "!*",
  "==",
  "#",
  "<BARE-WORD>",
  "<\"STRING\">",
  "<'STRING'>",
  "<`STRING`>"
};


/*
 *	Print one attribute and value into a string.
 */
int vp_prints(char *out, int outlen, VALUE_PAIR *vp)
{
	int		len;
	const char	*token = NULL;
	const char	*name;

	out[0] = 0;
	if (!vp) return 0;
	name = vp->name;

	if (!*name) {
		DICT_ATTR *da = dict_attrbyvalue(vp->attribute);
		if (!da) return 0;

		name = da->name;
	}

	if (strlen(name) + 3 > (size_t)outlen) {
		return 0;
	}

	if ((vp->operator > T_OP_INVALID) &&
	    (vp->operator < T_TOKEN_LAST)) {
		token = vp_tokens[vp->operator];
	} else {
		token = "<INVALID-TOKEN>";
	}

	if( vp->flags.has_tag ) {

		snprintf(out, outlen, "%s:%d %s ", name, vp->flags.tag,
			 token);

		len = strlen(out);
		vp_prints_value(out + len, outlen - len, vp, 1);

	} else {

	        snprintf(out, outlen, "%s %s ", name, token);
		len = strlen(out);
		vp_prints_value(out + len, outlen - len, vp, 1);

	}

	return strlen(out);
}


/*
 *	Print one attribute and value.
 */
void vp_print(FILE *fp, VALUE_PAIR *vp)
{
	char	buf[1024];

	vp_prints(buf, sizeof(buf), vp);
	fputs(buf, fp);
}


/*
 *	Print a whole list of attributes, indented by a TAB
 *	and with a newline at the end.
 */
void vp_printlist(FILE *fp, VALUE_PAIR *vp)
{
	for (; vp; vp = vp->next) {
		fprintf(fp, "\t");
		vp_print(fp, vp);
		fprintf(fp, "\n");
	}
}

