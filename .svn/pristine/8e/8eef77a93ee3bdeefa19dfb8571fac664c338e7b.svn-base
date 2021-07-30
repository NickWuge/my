#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		/*sleep*/

static int func1(){
	int rv;
	rv = *(int *)0;
	return rv;
}

static int func2()
{
	return func1();
}

static int* func3()
{
	int *b = malloc(sizeof(int));/*能检出*/
	int *c = malloc(sizeof(int));
	return c;
}	

//ulimit -c unlimited
int my1(){
	return func2();
}

int my2(){
	int *a = malloc(sizeof(int));
	int *c = func3();
	while(1){
		sleep(1);
	}
	return 0;
}

int my3(){
	int *a = malloc(sizeof(int));
	free(a);
	free(a);/*能检出*/
	return 0;
}

int my4(){
	int a[4];
	int *b = malloc(sizeof(int)*4);
	a[4] = 0;/*不能检出*/
	b[4] = 0;/*能检出*/
	return 0;
}

int my5(){
	int *a = malloc(sizeof(int));
	int b = *a;/*不能检出*/
	return 0;
}

int my6(){
	int *a = malloc(sizeof(int));
	free(a);
	*a = 0;/*能检出*/
	return 0;
}

