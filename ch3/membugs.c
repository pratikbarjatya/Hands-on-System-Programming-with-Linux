/*
 * ch3:membugs.c
 * 
 ***************************************************************
 * This program is part of the source code released for the book
 *  "Hands-on System Programming with Linux"
 *  (c) Author: Kaiwan N Billimoria
 *  Publisher:  Packt
 *
 * From:
 *  Ch 3 : Dynamic Memory Management
 ****************************************************************
 * Brief Description:
 *
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../common.h"

/*---------------- Globals, Macros ----------------------------*/
#define MAXVAL     5
static const size_t BLK_1MB = 1024*1024;

/*---------------- Typedef's, constants, etc ------------------*/

/*---------------- Functions ----------------------------------*/

/*
 * A demo: this function allocates memory internally; the caller
 * is responsibe for freeing it!
 */
static void silly_getpath(char **ptr)
{
#include <linux/limits.h>
	*ptr = malloc(PATH_MAX);
	if (!ptr)
		FATAL("malloc failed\n");

	strcpy(*ptr, getenv("PATH"));
	if (!*ptr)
		WARN("getenv failed\n");
}

/* option = 13 : memory leak test case 3: "lib" API leak */
static void leakage_case3(int cond)
{
	char *mypath=NULL;

	printf("\n## Leakage test: case 3: \"lib\" API"
		": runtime cond = %d\n", cond);

	/* Use C's illusory 'pass-by-reference' model */
	silly_getpath(&mypath);
	printf("mypath = %s\n", mypath);

	if (cond) /* Bug: if cond==0 then we have a leak! */
		free(mypath);
}

static void amleaky(size_t mem)
{
	char *ptr;

	ptr = malloc(mem);
	if (!ptr)
		FATAL("malloc(%zu) failed\n", mem);
	memset(ptr, 0, mem);

	/* Bug: no free, leakage */
}

/* option = 12 : memory leak test case 2: leak in a loop */
static void leakage_case2(size_t size, unsigned int reps)
{
	unsigned int i, threshold = 3*BLK_1MB;
	double mem_leaked;

	if (reps == 0)
		reps = 1;
	mem_leaked = size * reps;
	printf("%s(): will now leak a total of %.0f bytes (%.2f MB)"
			" [%zu bytes * %u loops]\n",
			__FUNCTION__, mem_leaked, mem_leaked/(1024*1024),
			size, reps);

	if (mem_leaked >= threshold)
		system("free|sed '1d'|head -n1");

	for (i=0; i<reps; i++) {
		if (i%10000 == 0)
			printf("%s():%6d:malloc(%zu)\n", __FUNCTION__, i, size);
		amleaky(size);
	}

	if (mem_leaked >= threshold)
		system("free|sed '1d'|head -n1");
	printf("\n");
}

/* option = 11 : memory leak test case 1: simple leak */
static void leakage_case1(size_t size)
{
	printf("%s(): will now leak %zu bytes (%ld MB)\n",
			__FUNCTION__, size, size/(1024*1024));
	amleaky(size);
}

/* option = 10 : double-free test case */
static void doublefree(int cond)
{
	char *ptr, *bogus;
	char name[]="Hands-on Linux Sys Prg";
	int n=512;

	printf("%s(): cond %d\n", __FUNCTION__, cond);
	ptr = malloc(n);
	if (!ptr)
		FATAL("malloc failed\n");
	strncpy(ptr, name, strlen(name));
	free(ptr);

	if (cond) {
		bogus = malloc(-1UL); /* will fail! */
		if (!bogus) {
			fprintf(stderr, "%s:%s:%d: malloc failed\n",
			   __FILE__, __FUNCTION__, __LINE__);
			free(ptr); /* Bug: double-free */
			exit(EXIT_FAILURE);
		}
	}
}

/* option =  9 : UAR (use-after-return) test case */
static void * uar(void)
{
	char name[32];
	
	memset(name, 0, 32);
	strncpy(name, "Hands-on Linux Sys Prg", 22);

	return name;
}

/* option =  8 : UAF (use-after-free) test case */
static void uaf(void)
{
	char *arr, *next;
	char name[]="Hands-on Linux Sys Prg";
	int n=512;

	arr = malloc(n);
	if (!arr)
		FATAL("malloc failed\n");
	memset(arr, 'a', n);
	arr[n-1]='\0';
	printf("%s():%d: arr = %p:%.*s\n", __FUNCTION__, __LINE__, arr, 32, arr);

	next = malloc(n);
	if (!next)
		FATAL("malloc failed\n");
	free(arr);

	strncpy(arr, name, strlen(name)); /* Bug: UAF */
	printf("%s():%d: arr = %p:%.*s\n", __FUNCTION__, __LINE__, arr, 32, arr);
	free(next);
}

/* option =  7 : out-of-bounds : read underflow */
static void read_underflow(int cond)
{
	char *dest, src[] = "abcd56789", *orig;

	printf("%s(): cond %d\n", __FUNCTION__, cond);
	dest = malloc(25);
	if (!dest)
		FATAL("malloc failed\n");
	orig = dest;

	strncpy(dest, src, strlen(src));
	if (cond) { /* Bug, below.. */
		*(orig-1) = 'x';
		dest --;
	}
	printf(" dest: %s\n", dest);

	free(orig);
}

/* option =  6 : out-of-bounds : read overflow [on dynamic memory] */
static void read_overflow_dynmem(void)
{
	char *arr;

	arr = malloc(5);
	if (!arr)
		FATAL("malloc failed\n");
	memset(arr, 'a', 5);
	/* Bug 1: Steal secrets via a buffer overread.
	 * Ensure the next few bytes are _not_ NULL.
	 * Ideally, this should be caught as a bug by the compiler,
	 * but isn't! (Tools do; seen later).
	 */
	arr[5] = 'S'; arr[6] = 'e'; arr[7] = 'c';
	arr[8] = 'r'; arr[9] = 'e'; arr[10] = 'T';
	printf("arr = %s\n", arr);

	/* Bug 2, 3: more read buffer overflows */
	printf("*(arr+100)=%d\n", *(arr + 100));
	printf("*(arr+10000)=%d\n", *(arr + 10000));

	free(arr);
}

/* option =  5 : out-of-bounds : read overflow [on compile-time memory] */
static void read_overflow_compilemem(void)
{
	char arr[5], tmp[8];

	memset(arr, 'a', 5);
	memset(tmp, 't', 8);
	tmp[7] = '\0';

	printf("arr = %s\n", arr);	/* Bug: read buffer overflow */
}

/* option =  4 : out-of-bounds : write underflow */
static void write_underflow(void)
{
	char *p = malloc(8);

	if (!p)
		FATAL("malloc failed\n");

	p--;
	strncpy(p, "abcd5678", 8);	/* Bug: write underflow */
	free(++p);
}

/* option =  3 : out-of-bounds : write overflow [on dynamic memory] */
static void write_overflow_dynmem(void)
{
	char *dest, src[] = "abcd56789";

	dest = malloc(8);
	if (!dest)
		FATAL("malloc failed\n");

	strcpy(dest, src);	/* Bug: write overflow */
	free(dest);
}

/* option =  2 : out-of-bounds : write overflow [on compile-time memory] */
static void write_overflow_compilemem(void)
{
	int i, arr[5];

	for (i = 0; i <= 5; i++) {
		arr[i] = 100;	/* Bug: 'arr' overflows on i==5,
				   overwriting part of the 'tmp'
				   variable - a stack overflow! */
	}
}

/* option =  1 : uninitialized var test case */
static void uninit_var()
{
	int x;       /* static mem */
	char *dyn;   /* dynamic mem */

#if 1
	if (x > MAXVAL)
		printf("true: x=%d\n", x);
	else
		printf("false: x=%d\n", x);
#else
	dyn = malloc(128*1024);
	memset(dyn, 'a', 128);
	free(dyn);

	dyn = malloc(56*1024);
	if (dyn[1000] == 'g')
		printf("it's a girl!\n");
	free(dyn);
#endif
}

static void usage(char *name)
{
	fprintf(stderr,
		"Usage: %s option [ -h | --help]\n"
		" option =  1 : uninitialized var test case\n"
		" option =  2 : out-of-bounds : write overflow [on compile-time memory]\n"
		" option =  3 : out-of-bounds : write overflow [on dynamic memory]\n"
		" option =  4 : out-of-bounds : write underflow\n"
		" option =  5 : out-of-bounds : read overflow [on compile-time memory]\n"
		" option =  6 : out-of-bounds : read overflow [on dynamic memory]\n"
		" option =  7 : out-of-bounds : read underflow\n"
		" option =  8 : UAF (use-after-free) test case\n"
		" option =  9 : UAR (use-after-return) test case\n"
		" option = 10 : double-free test case\n"
		" option = 11 : memory leak test case 1: simple leak\n"
		" option = 12 : memory leak test case 2: leak more (in a loop)\n"
		" option = 13 : memory leak test case 3: \"lib\" API leak\n"
		"-h | --help : show this help screen\n", name);
}

static void process_args(int argc, char **argv)
{
	void *res;

	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if ((argc == 2) && ((strncmp(argv[1], "-h", 2) == 0) ||
			    (strncmp(argv[1], "--help", 6) == 0))) {
		usage(argv[0]);
		exit(EXIT_SUCCESS);
	}

	if (argc == 2) {
		switch (atoi(argv[1])) {
		case 1:
			uninit_var();
			break;
		case 2:
			write_overflow_compilemem();
			break;
		case 3:
			write_overflow_dynmem();
			break;
		case 4:
			write_underflow();
			break;
		case 5:
			read_overflow_compilemem();
			break;
		case 6:
			read_overflow_dynmem();
			break;
		case 7:
			read_underflow(0);
			read_underflow(1);
			break;
		case 8:
			uaf();
			break;
		case 9:
			res = uar();
			printf("res: %s\n", (char *)res);
			break;
		case 10:
			doublefree(0);
			doublefree(1);
			break;
		case 11:
			leakage_case1(32);
			leakage_case1(BLK_1MB);
			break;
		case 12:
			leakage_case2(32, 100000);
			leakage_case2(BLK_1MB, 12);
			break;
		case 13:
			leakage_case3(0);
			leakage_case3(1);
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char **argv)
{
	process_args(argc, argv);
	exit(EXIT_SUCCESS);
}

/* vi: ts=8 */
