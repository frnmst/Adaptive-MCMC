#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <math.h>

#include "xsb_config.h"

#include "auxlry.h"
#include "context.h"
#include "cinterf.h"

#define UNSPECIFIED -1
#define PASS 1
#define FAIL 0
#define SINGLESWITCH 1
#define MULTISWITCH 0
#define USED 0
#define UNUSED 1
#define CONSISTENT_REWARD 1.0
#define INCONSISTENT_REWARD 0.0

#define setresult(R) result=R

#define SWAPPOINT(P1, P2)			\
{						\
  void *tmp;					\
  tmp = P1;					\
  P1 = P2;					\
  P2 = tmp;					\
}

#define SWAP(V1, V2)				\
{						\
  int tmp;					\
  tmp = V1;					\
  V1 = V2;					\
  V2 = tmp;					\
}

#define EXPAND(P, OLDS, NEWS, TYPE)		\
{						\
  TYPE* tmp = P;				\
  int i;					\
  P = malloc(NEWS * sizeof(TYPE));		\
  for (i = 0; i < OLDS; i++) {			\
    P[i] = tmp[i];				\
  }						\
  for (i = OLDS; i < NEWS; i++) {		\
    P[i] = (TYPE) UNSPECIFIED;			\
  }						\
  free(tmp);					\
}
 
int numprocs; /* number of random processes */
int result; /* whether query is true of false in current world */
int pass, numsamples; /* number of samples where query is true and total no. */
int rejections; /* number of rejections */
int collectdata; /* whether samples should be collected(1) or not(0) */
double **distributions; /* distributions of random processes */
double **cmdistributions; /* cumulative distributions of random processes */
int *lengths; /* number of outcomes of random processes */
int *cstate; /* current state */
int *pstate; /* proposed state */
int *p2d; /* mapping from random process to distribution */
int *cused; /* used processes in cstate */
int *pused; /*used processes in pstate */

int adaptive; /* whether to do adaptive mcmc (1) or not (0) */
int resampling; /* whether Singleswitch (1) or Multiswitch (0) like resampling */
double resample_prob = 0; /* resampling probability for Multiswitch scheme */

int **samplecount; /* #samples per randproc + outcome combination */
double **qvalues; /* Q-values of randproc + outcome pairs */
double **adistributions; /* adapted transition probabilities */
double **acmdistributions; /* adapted cumulative transition probabilities */
int cpos; /* stack pointer for cpositions */
int ppos; /* stack pointer for ppositions */
int countpos; /* whether processes should be added to stack or not */
int *cpositions; /* stack of randprocs related to evidence in cstate */
int *ppositions; /* stack of randprocs related to evidence in pstate */

int termlength(prolog_term);
void updatecounts(void);
int sampledist(double*, int);
int sampleswitch(int);
double sumdist(double *, int);
void updateqvals(double, int*, int, int*);
double accept_reject_prob(void);
double log_stateprob(int*, int*);
double log_transprob(int*, int*, int*, int*);
void map_pid2did(int, int);
void freshen(int);
void expand(void);
void printused(void);
void printstate(void);
void printpositions(void);
void printstats(void);
void printdebugall(void);
void printdebugone(int);
void printheader(void);

DllExport int call_conv init_mcmc_distributions(CTXTdecl)
{
  prolog_term proclist = reg_term(1);
  prolog_term dists = reg_term(2);
  int i; double sum;

  /* allocate memory for data-structures */
  numprocs = termlength(proclist);
  pass = numsamples = rejections = 0;
  collectdata = 0;
  distributions = malloc(numprocs * sizeof(double *));
  cmdistributions = malloc(numprocs * sizeof(double *));
  lengths = malloc(numprocs * sizeof(int));
  cstate = malloc(numprocs * sizeof(int));
  pstate = malloc(numprocs * sizeof(int));
  p2d = malloc(numprocs * sizeof(int));
  cused = malloc(numprocs * sizeof(int));
  pused = malloc(numprocs * sizeof(int));

  cpos = 0; ppos = 0; countpos = 0;
  samplecount = malloc(numprocs * sizeof(int *));
  qvalues = malloc(numprocs * sizeof(double *));
  adistributions = malloc(numprocs * sizeof(double *));
  acmdistributions = malloc(numprocs * sizeof(double *));
  cpositions = malloc(numprocs * sizeof(int));
  ppositions = malloc(numprocs * sizeof(int));

  for (i = 0; i < numprocs; i++) {
    cstate[i] = pstate[i] = UNSPECIFIED;
    p2d[i] = UNSPECIFIED;
    cused[i] = UNSPECIFIED;
    pused[i] = UNSPECIFIED;
  }
  /* seed rand() for future use */
  srand(time(NULL));

  /* fill up distributions */
  /* we assume that set of process ids are contiguous and start from 0 */
  while (is_list(proclist) == TRUE && is_list(dists) == TRUE) {
      prolog_term p = p2p_car(proclist);
      prolog_term d = p2p_car(dists);
      int procid = p2c_int(p);
      int length = termlength(d);
      double *dist = malloc(length * sizeof(double));
      double *cmdist = malloc(length * sizeof(double));

      i = 0; sum = 0;
      lengths[procid] = length;
      while (is_list(d) == TRUE) {
	dist[i] = p2c_float(p2p_car(d));
	sum += dist[i];
	cmdist[i] = sum;
	i++;
	d = p2p_cdr(d);
      }

      distributions[procid] = dist;
      cmdistributions[procid] = cmdist;

      proclist = p2p_cdr(proclist);
      dists = p2p_cdr(dists);
  }
  return TRUE;
}

DllExport int call_conv init_mcmc_state(CTXTdecl)
{
  /* the consistent world found through systematic search
   * is used to initialize the state of mcmc module */
  prolog_term proclist = reg_term(1);
  prolog_term distlist = reg_term(2);
  prolog_term outclist = reg_term(3);
  while (is_list(proclist) == TRUE && is_list(distlist) == TRUE && 
	 is_list(outclist) == TRUE) {
    int procid = p2c_int(p2p_car(proclist));
    int distid = p2c_int(p2p_car(distlist));
    int outcome = p2c_int(p2p_car(outclist));

    while (procid >= numprocs) {
      expand();
    }
    map_pid2did(procid, distid);
    pstate[procid] = outcome;
    pused[procid] = USED;

    if (adaptive) {
      /* these random procs are used for evidence so we record them */
      ppositions[ppos] = procid;
      ppos++;
    }

    proclist = p2p_cdr(proclist);
    distlist = p2p_cdr(distlist);
    outclist = p2p_cdr(outclist);
  }
  return TRUE;
}

DllExport int call_conv init_mcmc_result(CTXTdecl)
{
  int r = ptoc_int(1);
  setresult(r);
  return TRUE;
}

DllExport int call_conv mcmc_accept(CTXTdecl)
{
  if (adaptive) {
    updateqvals(CONSISTENT_REWARD, ppositions, ppos, pstate);
    SWAPPOINT(cpositions, ppositions);
    SWAP(cpos,ppos);
  }
  SWAPPOINT(cstate,pstate);
  SWAPPOINT(cused,pused);
  updatecounts();
  return TRUE;
}

DllExport int call_conv mcmc_collectdata(CTXTdecl)
{
  collectdata = ptoc_int(1);
  return TRUE;
}

DllExport int call_conv mcmc_adaptive(CTXTdecl)
{
  adaptive = ptoc_int(1);
  return TRUE;
}

DllExport int call_conv mcmc_resampleprob(CTXTdecl)
{
  resample_prob = ptoc_float(1);
  return TRUE;
}

DllExport int call_conv mcmc_resampling(CTXTdecl)
{
  resampling = ptoc_int(1);
  return TRUE;
}

DllExport int call_conv mcmc_initproposed(CTXTdecl)
{
  int i;
  for (i = 0; i < numprocs; i++) {
    pstate[i] = UNSPECIFIED;
    pused[i] = UNUSED;
  }
  if (adaptive) {
    ppos = 0; /* clear the stack for proposed state */
  }
  return TRUE;
}

DllExport int call_conv mcmc_selectandresample(CTXTdecl)
{
  int numused = 0, changed = 0, i, r;
  double s;

  for (i = 0; i < numprocs; i++) {
    if (cused[i] == USED) {
      numused++;
    }
  }
  if (resampling == MULTISWITCH) {
    /* resample each switch in current state based on coin toss */
    for (i = 0; i < numprocs; i++) {
      if (cused[i] == USED) {
        s = (double) rand () / RAND_MAX;
        if (s < resample_prob) {
	  pstate[i] = sampleswitch(i);
          if (pstate[i] != cstate[i]) {
            changed=1;
          }
        }
      }
    }
    if (changed == 0) {
      if (adaptive) {
        updateqvals(CONSISTENT_REWARD, cpositions, cpos, cstate);
      }
      updatecounts();
      ctop_int(CTXTc 1, 1);
    }else {
      ctop_int(CTXTc 1, 0);
    }
    return TRUE;
  }else {
    /* Singleswitch style resampling */
    r = rand() % numused;
    for (i = 0; i < numprocs; i++) {
      if (cused[i] == USED) {
	if (r == 0)
	  break;
	else
	  r--;
      }
    }
    pstate[i] = sampleswitch(i);
    if (pstate[i] == cstate[i]) {
      /* we bypass the meta-interpreter and update qvalues and counts */
      if (adaptive) {
	updateqvals(CONSISTENT_REWARD, cpositions, cpos, cstate);
      }
      updatecounts();
      ctop_int(CTXTc 1, 1);
    } else {
      ctop_int(CTXTc 1, 0);
    }
    return TRUE;
  }
}

DllExport int call_conv mcmc_countpos(CTXTdecl)
{
  countpos = ptoc_int(1);
  return TRUE;
}

DllExport int call_conv mcmc_sample(CTXTdecl)
{
  int procid = ptoc_int(1);
  int distid = ptoc_int(2);
  while (procid >= numprocs) {
    expand();
  }
  map_pid2did(procid, distid);

  if (pstate[procid] == UNSPECIFIED) {
    if(cused[procid] != USED) {
      pstate[procid] = sampleswitch(procid);
    }else{
      pstate[procid] = cstate[procid];
    }
  }
  if (adaptive) {
    if (countpos && pused[procid] == UNUSED) {
      /* record the randprocs which are used in the evidence */
      ppositions[ppos] = procid;
      ppos++;
    }
  }
  pused[procid] = USED;
  ctop_int(CTXTc 3, pstate[procid]);
  return TRUE;
}

DllExport int call_conv mcmc_acceptreject(CTXTdecl)
{
  int r = ptoc_int(1);

  double a = (double) rand() / RAND_MAX;
  double ar = accept_reject_prob();

  if (adaptive) {
    /* consistent sample proposed -- adapt qvalues accordingly */
    updateqvals(CONSISTENT_REWARD, ppositions, ppos, pstate);
  }
  if (a < ar) {
    SWAPPOINT(cstate,pstate);
    SWAPPOINT(cused,pused);
    if (adaptive) {
      SWAPPOINT(cpositions,ppositions);
      SWAP(cpos,ppos);
    }
    setresult(r);
  }
  updatecounts();
  return TRUE;
}

DllExport int call_conv mcmc_reject(CTXTdecl)
{
  rejections++;
  if (adaptive) {
    /* inconsistent sample proposed -- adapt qvalues accordingly */
    updateqvals(INCONSISTENT_REWARD, ppositions, ppos, pstate);
  }
  updatecounts();
  return TRUE;
}

DllExport int call_conv mcmc_probability(CTXTdecl)
{
  ctop_float(CTXTc 1, ((double)pass)/numsamples);
  return TRUE;
}

DllExport int call_conv mcmc_rejections(CTXTdecl)
{
  ctop_int(CTXTc 1, rejections);
  return TRUE;
}

DllExport int call_conv mcmc_terminate(CTXTdecl)
{
  free(distributions);
  free(cmdistributions);
  free(lengths);
  free(cstate);
  free(pstate);
  free(samplecount);
  free(qvalues);
  free(adistributions);
  free(acmdistributions);
  free(cpositions);
  free(ppositions);
  free(p2d);
  free(cused);
  free(pused);
  return TRUE;
}

DllExport int call_conv mcmc_printstate(CTXTdecl)
{
  printstate();
  return TRUE;
}

DllExport int call_conv mcmc_printused(CTXTdecl)
{
  printused();
  return TRUE;
}

DllExport int call_conv mcmc_printpositions(CTXTdecl)
{
  printpositions();
  return TRUE;
}

DllExport int call_conv mcmc_printstats(CTXTdecl)
{
  printstats();
  return TRUE;
}

DllExport int call_conv mcmc_printdebug(CTXTdecl)
{
  int i = ptoc_int(1);
  printdebugone(i);
  return TRUE;
}

int termlength(prolog_term t) 
{
  int i = 0;
  while (is_list(t) == TRUE) {
    i++;
    t = p2p_cdr(t);
  }
  return i;
}

void updatecounts()
{
  if (collectdata) {
    if(result == PASS) {
      pass++;
    }
    numsamples++;
  }
}

int sampledist(double *c, int l) 
{
  int i;
  double r = ((double) rand()) / RAND_MAX;
  for (i = 0; i < l; i++) {
    if (r <= c[i]) {
      return i;
    }
  }
  return(l-1);
}

int sampleswitch(int i)
{
  if (adaptive) {
    return sampledist(acmdistributions[i], lengths[p2d[i]]);
  }else{
    return sampledist(cmdistributions[p2d[i]], lengths[p2d[i]]);
  }
}
double sumdist(double *dist, int length) 
{
  int i = 0; double sum = 0;
  while (i < length) {
    sum += dist[i];
    i++;
  }
  return sum;
}

void updateqvals(double reward, int *positions, int pos, int *state)
{
  while (pos > 0) {
    int procid, outcome, tcount, i;
    double cur_qval, tmp;
    pos--;
    procid = positions[pos];
    outcome = state[procid];
    tcount = *(*(samplecount+procid)+outcome);
    cur_qval = *(*(qvalues+procid)+outcome);
    /* update qvalue */
    *(*(qvalues+procid)+outcome) = (reward + tcount * cur_qval) / (tcount + 1);
    *(*(samplecount+procid)+outcome) += 1;
    /* modify the reward */
    tmp = 0;
    for(i=0; i < lengths[p2d[procid]]; i++) {
      tmp += (*(*(distributions+p2d[procid])+i)) * (*(*(qvalues+procid)+i));
    }
    reward = tmp;
    freshen(procid);
  }
}

double accept_reject_prob()
{
  if (adaptive) {
    double p1 = log_stateprob(cstate,cused);
    double p2 = log_stateprob(pstate,pused);
    double p12 = log_transprob(cstate, cused, pstate, pused);
    double p21 = log_transprob(pstate, pused, cstate, cused);
    return exp(p2 + p21 - p1 - p12);
  } else {
    if (resampling == MULTISWITCH) {
      return 1;
    } else {
      int nused_c, nused_p, i;
      nused_c = nused_p = 0;
      for (i = 0; i < numprocs; i++) {
	if (cused[i] == USED) {
	  nused_c++;
	}
	if(pused[i] == USED) {
	  nused_p++;
	}
      }
      return ((double) nused_c) / nused_p;
    }
  }
}

double log_stateprob(int *state, int *used)
{
  double prob = 0;
  int i;
  for (i = 0; i < numprocs; i++) {
    if (used[i] == USED) {
      int s = state[i];
      prob += log(*(*(distributions+p2d[i])+s));
    }
  }
  return prob;
}

double log_transprob(int *state1, int *used1, int *state2, int *used2) 
{
  /* compute transition probability from state1 to state2 */
  int i;
  double prob = 0;
  int numused = 0;

  /* It is assumed that state1 and state2 are distinct. If both states are
     identical and Singleswitch style resampling is done, then the retun value
     is not correct. However this doesn't affect the correctness of the overall
     algorithm. For Multiswitch style resampling correct value is returned in
     all cases */

  for (i = 0; i < numprocs; i++) {
    if (used1[i] == USED)
      numused++;
  }
  if (resampling == SINGLESWITCH) {
    prob -= log(numused);
  }
  for (i = 0; i < numprocs; i++) {
    if ((used1[i] == USED && used2[i] == USED && state1[i] != state2[i]) ||
	(used1[i] == UNUSED && used2[i] == USED)) {
      int s = state2[i];
      prob += log(*(*(adistributions+i)+s));
    }
  }
  return prob;
}

void map_pid2did(int procid, int distid)
{
  if (p2d[procid] == UNSPECIFIED) {
    p2d[procid] = distid;
    if (adaptive) {
      double *ad = malloc(lengths[distid] * sizeof(double));
      double *acmd = malloc(lengths[distid] * sizeof(double));
      double *qv = malloc(lengths[distid] * sizeof(double));
      int *tcount = malloc(lengths[distid] * sizeof(int));
      int i;
      double asum, acsum;
 
      for (i = 0; i < lengths[distid]; i++) {
	tcount[i] = 1;
	qv[i] = 1;
	ad[i] = qv[i] * (*(*(distributions+distid)+i));
      }
      asum = sumdist(ad, lengths[distid]);
      i = 0; acsum = 0;
      while (i < lengths[distid]) {
	ad[i] /= asum;
	acsum += ad[i];
	acmd[i] = acsum;
	i++;
      }
      samplecount[procid] = tcount;
      qvalues[procid] = qv;
      adistributions[procid] = ad;
      acmdistributions[procid] = acmd;
    }
  }
}

void freshen(int procid)
{
  int i;
  double asum, tmp;
  /* update the adaptive distributions */
  for(i=0; i < lengths[p2d[procid]]; i++) {
    *(*(adistributions+procid)+i) =
      (*(*(distributions+p2d[procid])+i)) * (*(*(qvalues+procid)+i));
  }
  asum = sumdist(adistributions[procid], lengths[p2d[procid]]);
  tmp = 0;
  for(i=0; i < lengths[p2d[procid]]; i++) {
    *(*(adistributions+procid)+i) /= asum;
    tmp += *(*(adistributions+procid)+i);
    *(*(acmdistributions+procid)+i) = tmp;
  }
}

void expand()
{
  /* double the size of the relevant datastructures */
  int oldsize = numprocs;
 
  numprocs *= 2;

  EXPAND(p2d, oldsize, numprocs, int);
  EXPAND(cstate, oldsize, numprocs, int);
  EXPAND(pstate, oldsize, numprocs, int);
  EXPAND(samplecount, oldsize, numprocs, int*);
  EXPAND(qvalues, oldsize, numprocs, double*);
  EXPAND(adistributions, oldsize, numprocs, double*);
  EXPAND(acmdistributions, oldsize, numprocs, double*);
  EXPAND(cpositions, oldsize, numprocs, int);
  EXPAND(ppositions, oldsize, numprocs, int);
  EXPAND(cused, oldsize, numprocs, int);
  EXPAND(pused, oldsize, numprocs, int);
}

void printused()
{
  int i;
  printf("current used: ");
  for (i = 0; i < numprocs; i++) {
    printf("%d ", cused[i]);
  }
  printf("\n");
  printf("proposed used: ");
  for (i = 0; i < numprocs; i++) {
    printf("%d ", pused[i]);
  }
  printf("\n");
}

void printstate() 
{
  int i;
  printf("current: ");
  for (i = 0; i < numprocs; i++) {
      printf("%d ", cstate[i]);
  }
  printf("\n");
  printf("proposed: ");
  for (i = 0; i < numprocs; i++) {
      printf("%d ", pstate[i]);
  }
  printf("\n");
}

void printpositions() 
{
  int i;
  printf("cpos: %d\n", cpos);
  printf("current stack\n");
  for (i = 0; i < cpos; i++) {
    printf("%d ", cpositions[i]);
  }
  printf("\n");
  printf("ppos: %d\n", ppos);
  printf("proposed stack\n");
  for (i=0; i < ppos; i++) {
    printf("%d ", ppositions[i]);
  }
  printf("\n");
}

void printstats() 
{
  printf("pass: %d numsamples: %d rejections: %d\n", pass, numsamples, 
	 rejections);
}

void printheader()
{
  printf("pid| process| samplecount| qvalues| adist| acmdist\n");
}

void printdebugone(int i)
{
  int j;
  if (adaptive) {
    for (j = 0; j < lengths[p2d[i]]; j++) {
      printf("%d ", *(*(samplecount+i)+j));
    }
    printf("| ");
    for (j = 0; j < lengths[p2d[i]]; j++) {
      printf("%f ", *(*(qvalues+i)+j));
    }
    printf("| ");
    for (j = 0; j < lengths[p2d[i]]; j++) {
      printf("%f ", *(*(adistributions+i)+j));
    }
    printf("| ");
    for (j = 0; j < lengths[p2d[i]]; j++) {
      printf("%f ", *(*(acmdistributions+i)+j));
    }
    printf("\n");
  }
}

void printdebugall()
{
  int i;
  if (adaptive) {
    for (i = 0; i < numprocs; i++) {
      printdebugone(i);
    }
  }
}
