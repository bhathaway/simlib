#ifndef __SIMLIB__
#define __SIMLIB__
/* This is simlib.h. */

#include "simlibdefs.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* Declare simlib global variables. */

extern int    *list_rank, *list_size, next_event_type, maxatr, maxlist;
extern double  *transfer, sim_time, prob_distrib[26];

struct master {
    double  *value;
    struct master *pr;
    struct master *sr;
};

extern struct master **head, **tail;

/* Declare simlib functions. */

void  init_simlib(void);
void  cleanup_simlib(void);
void  list_file(int option, int list);
void  list_remove(int option, int list);
void  timing(void);
void  event_schedule(double time_of_event, int type_of_event);
double sampst(double value, int varibl);
double timest(double value, int varibl);
double filest(int list);
void  out_sampst(FILE *unit, int lowvar, int highvar);
void  out_timest(FILE *unit, int lowvar, int highvar);
void  out_filest(FILE *unit, int lowlist, int highlist);
double expon(double mean, int stream);
int   random_integer(double prob_distrib[], int stream);
double uniform(double a, double b, int stream);
double erlang(int m, double mean, int stream);
double lcgrand(int stream);
void  lcgrandst(long zset, int stream);
long  lcgrandgt(int stream);

#endif

