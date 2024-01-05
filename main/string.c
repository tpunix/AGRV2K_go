/*
 * string.c
 */

#include "main.h"

void *memset(void *s, int v, unsigned int n)
{
    register char *p = (char *)s;

    while(n){
        *p++ = (char)v;
		n -= 1;
    }

    return s;
}

void *memcpy(void *to, const void *from, unsigned int n)
{
    register char *t = (char *)to;

    while(n){
        *t++ = *(char*)from++;
		n -= 1;
    }

    return to;
}

int memcmp(const void *dst, const void *src, unsigned int n)
{
	register int i;
	register unsigned char *s = (unsigned char*)src;
	register unsigned char *d = (unsigned char*)dst;

	for(i=0; i<n; i++){
		unsigned char ds = *s;
		unsigned char dd = *d;
		if(ds!=dd){
			return dd-ds;
		}
		s++;
		d++;
	}

	return 0;
}



char *strcpy(char *dst, const char *src)
{
    register char *d = dst;
    register char t;

    do{
        t = *src++;
        *d++ = t;
    }while(t);

    return dst;
}

char *strncpy(char *dst, const char *src, unsigned int n)
{
    register char *d = dst;
    register char t;

    while(n){
        t = *src++;
        *d++ = t;
		if(t==0)
			break;
		n -= 1;
    }

    return dst;
}

int strcmp(const char *s1, const char *s2)
{
    int r;
    int t;

    while(1){
        t = (int)*s1++;
        r = t - (int)*s2++;
        if(r)
            break;
        if(t==0)
            break;
    }

    return r;
}

int strcasecmp(const char *s1, const char *s2)
{
    int r;
    int t1, t2;

    while(1){
        t1 = (int)*s1++;
        t2 = (int)*s1++;
		if(t1>='A' && t1<='Z') t1 = t1-'A'+'a';
		if(t2>='A' && t2<='Z') t2 = t2-'A'+'a';
        r = t1 - t2;
        if(r || t1==0)
            break;
    }

    return r;
}

int strncmp(const char *s1, const char *s2, unsigned int n)
{
    register int r = 0;
    register int t;

    while(n){
        t = (int)*s1++;
        r = t - (int)*s2++;
        if(r)
            break;
        if(t==0)
            break;
		n -= 1;
    }

    return r;
}


char *strchr(const char *s1, int ch)
{
	while(1){
		char t = *s1;
		if(t==0)
			return (char*)0;
		if(t==ch)
			return (char*)s1;
		s1++;
	}
}


unsigned int strlen(const char *s)
{
    register const char *p = s;

    while(*p++);

    return p-s-1;
}


unsigned int strtoul(char *str, char **endptr, int requestedbase, int *ret)
{
	unsigned int num = 0;
	int c, digit, retv;
	int base, nchars, leadingZero;

	base = 10;
	nchars = 0;
	leadingZero = 0;

	if(str==0 || *str==0){
		retv = 2;
		goto exit;
	}
	retv = 0;

	if(requestedbase)
		base = requestedbase;

	while ((c = *str) != 0) {
		if (nchars == 0 && c == '0') {
			leadingZero = 1;
			goto step;
		} else if (leadingZero && nchars == 1) {
			if (c == 'x') {
				base = 16;
				goto step;
			} else if (c == 'o') {
				base = 8;
				goto step;
			}
		}
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'z') {
			digit = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'Z') {
			digit = c - 'A' + 10;
		} else {
			retv = 3;
			break;
		}
		if (digit >= base) {
			retv = 4;
			break;
		}
		num *= base;
		num += digit;
step:
		str++;
		nchars++;
	}

exit:
	if(ret)
		*ret = retv;
	if(endptr)
		*endptr = str;

	return num;
}

