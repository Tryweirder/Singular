/****************************************
*  Computer Algebra System SINGULAR     *
****************************************/
/* $Id: longalg.cc,v 1.45 2009-08-13 15:17:03 Singular Exp $ */
/*
* ABSTRACT:   algebraic numbers
* convention: A) minpoly==0: n->z, n->n are from Z[a] resp. Z/p[a],
*                no common div., L(n->n) >0 (or n->n==NULL)
*             B) minpoly!=NULL: n->z from k[a], n->n==NULL
*/

#include <stdio.h>
#include <string.h>
#include "mod2.h"
#include "structs.h"
#include "omalloc.h"
#include "febase.h"
#include "longrat.h"
#include "modulop.h"
#include "numbers.h"
#include "polys.h"
#include "ideals.h"
#include "ring.h"
#ifdef HAVE_FACTORY
#include "factory.h"
#include "clapsing.h"
#include "clapconv.h"
#endif
#include "longalg.h"

naIdeal naI=NULL;

int naNumbOfPar;
napoly naMinimalPoly;
#define naParNames (currRing->parameter)
static int naIsChar0;
static int naPrimeM;

#ifdef LDEBUG
#define naTest(a) naDBTest(a,__FILE__,__LINE__)
BOOLEAN naDBTest(number a, const char *f,const int l);
#else
#define naTest(a)
#endif

number (*naMap)(number from);
/* procedure variables */
static numberfunc
                nacMult, nacSub, nacAdd, nacDiv, nacIntDiv;
static number   (*nacGcd)(number a, number b, const ring r);
static number   (*nacLcm)(number a, number b, const ring r);
       number   (*nacInit)(int i);
static int      (*nacInt)(number &n);
       void     (*nacNormalize)(number &a);
static number   (*nacNeg)(number a);
static void     (*nacWrite)(number &a);
       number   (*nacCopy)(number a);
static number   (*nacInvers)(number a);
       BOOLEAN  (*nacIsZero)(number a);
static BOOLEAN  (*nacIsOne)(number a);
static BOOLEAN  (*nacIsMOne)(number a);
static BOOLEAN  (*nacGreaterZero)(number a);
static const char   * (*nacRead) (const char *s, number *a);
static napoly napRedp(napoly q);
static napoly napTailred(napoly q);
static BOOLEAN napDivPoly(napoly p, napoly q);
static int napExpi(int i, napoly a, napoly b);
static ring nacRing;

#define napCopy(p)       (napoly)p_Copy((poly)p,nacRing)

static number nadGcd( number a, number b, const ring r) { return nacInit(1); }
/*2
*  sets the appropriate operators
*/
void naSetChar(int i, ring r)
{
  if (naI!=NULL)
  {
    int j;
    for (j=naI->anz-1; j>=0; j--)
       napDelete (&naI->liste[j]);
    omFreeSize((ADDRESS)naI->liste,naI->anz*sizeof(napoly));
    omFreeBin((ADDRESS)naI, snaIdeal_bin);
    naI=NULL;
  }
  naMap = naCopy;

  if (r->minpoly!=NULL)
  {
    naMinimalPoly=((lnumber)r->minpoly)->z;
    omCheckAddr(naMinimalPoly);
  }
  else
    naMinimalPoly = NULL;
  if (r->minideal!=NULL)
  {
    naI=(naIdeal)omAllocBin(snaIdeal_bin);
    naI->anz=IDELEMS(r->minideal);
    naI->liste=(napoly*)omAlloc(naI->anz*sizeof(napoly));
    int j;
    for (j=naI->anz-1; j>=0; j--)
    {
      lnumber a = (lnumber)pGetCoeff(r->minideal->m[j]);
      naI->liste[j]=napCopy(a->z);
    }
  }

  naNumbOfPar=rPar(r);
  if (i == 1)
  {
    naIsChar0 = 1;
  }
  else if (i < 0)
  {
    naIsChar0 = 0;
    npSetChar(-i, r->algring); // to be changed HS
  }
#ifdef TEST
  else
  {
    Print("naSetChar:c=%d param=%d\n",i,rPar(r));
  }
#endif
  nacRing        = r->algring;
  nacInit        = nacRing->cf->nInit;
  nacInt         = nacRing->cf->nInt;
  nacCopy        = nacRing->cf->nCopy;
  nacAdd         = nacRing->cf->nAdd;
  nacSub         = nacRing->cf->nSub;
  nacNormalize   = nacRing->cf->nNormalize;
  nacNeg         = nacRing->cf->nNeg;
  nacIsZero      = nacRing->cf->nIsZero;
  nacRead        = nacRing->cf->nRead;
  nacWrite       = nacRing->cf->nWrite;
  nacGreaterZero = nacRing->cf->nGreaterZero;
  nacIsOne       = nacRing->cf->nIsOne;
  nacIsMOne      = nacRing->cf->nIsMOne;
  nacGcd         = nacRing->cf->nGcd;
  nacLcm         = nacRing->cf->nLcm;
  nacMult        = nacRing->cf->nMult;
  nacDiv         = nacRing->cf->nDiv;
  nacIntDiv      = nacRing->cf->nIntDiv;
  nacInvers      = nacRing->cf->nInvers;
}

/*============= procedure for polynomials: napXXXX =======================*/



#ifdef LDEBUG
static void napTest(napoly p)
{
  while (p != NULL)
  {
    if (naIsChar0)
      nlDBTest(napGetCoeff(p), "", 0);
    napIter(p);
  }
}
#else
#define napTest(p) ((void) 0)
#endif

#define napInit(i)       (napoly)p_ISet(i,nacRing)
#define napSetCoeff(p,n) p_SetCoeff(p,n,nacRing);
#define napDelete1(p)    p_LmDelete((poly *)p, nacRing)
#define nap_Copy(p,r)       (napoly)p_Copy((poly)p,r->algring)
#define napComp(p,q)     p_LmCmp((poly)p,(poly)q, nacRing)
#define napMultT(A,E)    A=(napoly)p_Mult_mm((poly)A,(poly)E,nacRing)
#define napMult(A,B)     (napoly)p_Mult_q((poly)A,(poly)B,nacRing)
#define napMultCopy(A,B)   (napoly)pp_Mult_qq((poly)A,(poly)B,nacRing)
#define napIsConstant(p) p_LmIsConstant(p,nacRing)
#define napDeg(p)        (int)p_ExpVectorQuerSum(p, nacRing)

/*3
* creates  an napoly
*/
napoly napInitz(number z)
{
  napoly a = (napoly)p_Init(nacRing);
  napGetCoeff(a) = z;
  return a;
}

/*3
* copy a napoly. poly
*/
static napoly napCopyNeg(napoly p)
{
  napoly r=napCopy(p);
  r=(napoly)p_Neg((poly)r, nacRing);
  return r;
}

/*3
*  division with rest; f = g*q + r, returns r and destroy f
*/
napoly napRemainder(napoly f, const napoly  g)
{
  napoly a, h, qq;

  qq = (napoly)p_Init(nacRing);
  napNext(qq) = NULL;
  p_Normalize(g,nacRing);
  p_Normalize(f,nacRing);
  a = f;
  do
  {
    napSetExp(qq,1, napGetExp(a,1) - napGetExp(g,1));
    napSetm(qq);
    napGetCoeff(qq) = nacDiv(napGetCoeff(a), napGetCoeff(g));
    napGetCoeff(qq) = nacNeg(napGetCoeff(qq));
    nacNormalize(napGetCoeff(qq));
    h = napCopy(g);
    napMultT(h, qq);
    p_Normalize(h,nacRing);
    n_Delete(&napGetCoeff(qq),nacRing);
    a = napAdd(a, h);
  }
  while ((a!=NULL) && (napGetExp(a,1) >= napGetExp(g,1)));
  omFreeBinAddr(qq);
  return a;
}

/*3
*  division with rest; f = g*q + r,  destroy f
*/
static void napDivMod(napoly f, napoly  g, napoly *q, napoly *r)
{
  napoly a, h, b, qq;

  qq = (napoly)p_Init(nacRing);
  napNext(qq) = b = NULL;
  p_Normalize(g,nacRing);
  p_Normalize(f,nacRing);
  a = f;
  do
  {
    napSetExp(qq,1, napGetExp(a,1) - napGetExp(g,1));
    p_Setm(qq,nacRing);
    napGetCoeff(qq) = nacDiv(napGetCoeff(a), napGetCoeff(g));
    nacNormalize(napGetCoeff(qq));
    b = napAdd(b, napCopy(qq));
    napGetCoeff(qq) = nacNeg(napGetCoeff(qq));
    h = napCopy(g);
    napMultT(h, qq);
    p_Normalize(h,nacRing);
    n_Delete(&napGetCoeff(qq),nacRing);
    a = napAdd(a, h);
  }
  while ((a!=NULL) && (napGetExp(a,1) >= napGetExp(g,1)));
  omFreeBinAddr(qq);
  *q = b;
  *r = a;
}

/*3
*  returns z with z*x mod c = 1
*/
static napoly napInvers(napoly x, const napoly c)
{
  napoly y, r, qa, qn, q;
  number t, h;

  if (napGetExp(x,1) >= napGetExp(c,1))
    x = napRemainder(x, c);
  if (x==NULL)
  {
    goto zero_divisor;
  }
  if (napGetExp(x,1)==0)
  {
    if (!nacIsOne(napGetCoeff(x)))
    {
      nacNormalize(napGetCoeff(x));
      t = nacInvers(napGetCoeff(x));
      nacNormalize(t);
      n_Delete(&napGetCoeff(x),nacRing);
      napGetCoeff(x) = t;
    }
    return x;
  }
  y = napCopy(c);
  napDivMod(y, x, &qa, &r);
  if (r==NULL)
  {
    goto zero_divisor;
  }
  if (napGetExp(r,1)==0)
  {
    nacNormalize(napGetCoeff(r));
    t = nacInvers(napGetCoeff(r));
    nacNormalize(t);
    t = nacNeg(t);
    qa=p_Mult_nn(qa,t,nacRing); p_Normalize(qa,nacRing);
    n_Delete(&t,nacRing);
    p_Normalize(qa,nacRing);
    napDelete(&x);
    napDelete(&r);
    return qa;
  }
  y = x;
  x = r;
  napDivMod(y, x, &q, &r);
  if (r==NULL)
  {
    goto zero_divisor;
  }
  if (napGetExp(r,1)==0)
  {
    q = napMult(q, qa);
    q = napAdd(q, napInit(1));
    nacNormalize(napGetCoeff(r));
    t = nacInvers(napGetCoeff(r));
    q=p_Mult_nn(q,t,nacRing); p_Normalize(q,nacRing);
    p_Normalize(q,nacRing);
    n_Delete(&t,nacRing);
    napDelete(&x);
    napDelete(&r);
    if (napGetExp(q,1) >= napGetExp(c,1))
      q = napRemainder(q, c);
    return q;
  }
  q = napMult(q, napCopy(qa));
  q = napAdd(q, napInit(1));
  qa = napNeg(qa);
  loop
  {
    y = x;
    x = r;
    napDivMod(y, x, &qn, &r);
    if (r==NULL)
    {
      break;
    }
    if (napGetExp(r,1)==0)
    {
      q = napMult(q, qn);
      q = napNeg(q);
      q = napAdd(q, qa);
      nacNormalize(napGetCoeff(r));
      t = nacInvers(napGetCoeff(r));
      //nacNormalize(t);
      q=p_Mult_nn(q,t,nacRing); p_Normalize(q,nacRing);
      p_Normalize(q,nacRing);
      n_Delete(&t,nacRing);
      napDelete(&x);
      napDelete(&r);
      if (napGetExp(q,1) >= napGetExp(c,1))
        q = napRemainder(q, c);
      return q;
    }
    y = q;
    q = napMult(napCopy(q), qn);
    q = napNeg(q);
    q = napAdd(q, qa);
    qa = y;
  }
// zero divisor found:
zero_divisor:
  Werror("zero divisor found - your minpoly is not irreducible");
  return x;
}

/*3
* the max degree of an napoly poly (used for test of "simple" et al.)
*/
static int  napMaxDeg(napoly p)
{
  int  d = 0;
  while(p!=NULL)
  {
    d=si_max(d,napDeg(p));
    napIter(p);
  }
  return d;
}

/*3
* the max degree of an napoly poly (used for test of "simple" et al.)
*/
static int  napMaxDegLen(napoly p, int &l)
{
  int  d = 0;
  int ll=0;
  while(p!=NULL)
  {
    d=si_max(d,napDeg(p));
    napIter(p);
    ll++;
  }
  l=ll;
  return d;
}


/*3
*writes a polynomial number
*/
void napWrite(napoly p,const BOOLEAN has_denom)
{
  if (p==NULL)
    StringAppendS("0");
  else if (napIsConstant(p))
  {
    BOOLEAN kl=FALSE;
    if (has_denom)
    {
      number den=n_GetDenom(napGetCoeff(p), nacRing);
      kl=!n_IsOne(den,nacRing);
      n_Delete(&den, nacRing);
    }
    if (kl) StringAppendS("(");
    //StringAppendS("-1");
    nacWrite(napGetCoeff(p));
    if (kl) StringAppendS(")");
  }
  else
  {
    StringAppendS("(");
    loop
    {
      BOOLEAN wroteCoeff=FALSE;
      if ((napIsConstant(p))
      || ((!nacIsOne(napGetCoeff(p)))
        && (!nacIsMOne(napGetCoeff(p)))))
      {
        nacWrite(napGetCoeff(p));
        wroteCoeff=(currRing->ShortOut==0);
      }
      else if (nacIsMOne(napGetCoeff(p)))
      {
        StringAppendS("-");
      }
      int  i;
      for (i = 0; i < naNumbOfPar; i++)
      {
        if (napGetExp(p,i+1) > 0)
        {
          if (wroteCoeff)
            StringAppendS("*");
          else
            wroteCoeff=(currRing->ShortOut==0);
          StringAppendS(naParNames[i]);
          if (napGetExp(p,(i+1)) > 1)
          {
            if (currRing->ShortOut == 0)
              StringAppendS("^");
            StringAppend("%d", napGetExp(p,i+1));
          }
        }
      } /*for*/
      napIter(p);
      if (p==NULL)
        break;
      if (nacGreaterZero(napGetCoeff(p)))
        StringAppendS("+");
    }
    StringAppendS(")");
  }
}


static const char *napHandleMons(const char *s, int i, napoly ex)
{
  int  j;
  if (strncmp(s,naParNames[i],strlen(naParNames[i]))==0)
  {
    s+=strlen(naParNames[i]);
    if ((*s >= '0') && (*s <= '9'))
    {
      s = eati(s, &j);
      napAddExp(ex,i+1,j);
    }
    else
      napAddExp(ex,i+1,1);
  }
  return s;
}
static const char *napHandlePars(const char *s, int i, napoly ex)
{
  int  j;
  if (strcmp(s,naParNames[i])==0)
  {
    s+=strlen(naParNames[i]);
    napSetExp(ex,i+1,1);
  }
  return s;
}

/*3  reads a monomial  */
static const char  *napRead(const char *s, napoly *b)
{
  napoly a;
  int  i;
  a = (napoly)p_Init(nacRing);
  if ((*s >= '0') && (*s <= '9'))
  {
    s = nacRead(s, &napGetCoeff(a));
    if (nacIsZero(napGetCoeff(a)))
    {
      napDelete1(&a);
      *b = NULL;
      return s;
    }
  }
  else
    napGetCoeff(a) = nacInit(1);
  i = 0;
  const char  *olds=s;
  loop
  {
    s = napHandlePars(s, i, a);
    if (olds == s)
      i++;
    else if (*s == '\0')
    {
      *b = a;
      return s;
    }
    if (i >= naNumbOfPar)
      break;
  }
  i=0;
  loop
  {
    olds = s;
    s = napHandleMons(s, i, a);
    if (olds == s)
      i++;
    else
      i = 0;
    if ((*s == '\0') || (i >= naNumbOfPar))
      break;
  }
  *b = a;
  return s;
}

static int napExp(napoly a, napoly b)
{
  while (napNext(a)!=NULL) napIter(a);
  int m = napGetExp(a,1);
  if (m==0) return 0;
  while (napNext(b)!=NULL) napIter(b);
  int mm=napGetExp(b,1);
  if (m > mm) m = mm;
  return m;
}

/*
* finds the smallest i-th exponent in a and b
* used to find it in a fraction
*/
static int napExpi(int i, napoly a, napoly b)
{
  if (a==NULL || b==NULL) return 0;
  int m = napGetExp(a,i+1);
  if (m==0) return 0;
  while (napNext(a) != NULL)
  {
    napIter(a);
    if (m > napGetExp(a,i+1))
    {
      m = napGetExp(a,i+1);
      if (m==0) return 0;
    }
  }
  do
  {
    if (m > napGetExp(b,i+1))
    {
      m = napGetExp(b,i+1);
      if (m==0) return 0;
    }
    napIter(b);
  }
  while (b != NULL);
  return m;
}

static void napContent(napoly ph)
{
  number h,d;
  napoly p;

  p = ph;
  if (nacIsOne(napGetCoeff(p)))
    return;
  h = nacCopy(napGetCoeff(p));
  napIter(p);
  do
  {
    d=nacGcd(napGetCoeff(p), h, nacRing);
    if(nacIsOne(d))
    {
      n_Delete(&h,nacRing);
      n_Delete(&d,nacRing);
      return;
    }
    n_Delete(&h,nacRing);
    h = d;
    napIter(p);
  }
  while (p!=NULL);
  h = nacInvers(d);
  n_Delete(&d,nacRing);
  p = ph;
  while (p!=NULL)
  {
    d = nacMult(napGetCoeff(p), h);
    n_Delete(&napGetCoeff(p),nacRing);
    napGetCoeff(p) = d;
    napIter(p);
  }
  n_Delete(&h,nacRing);
}

static void napCleardenom(napoly ph)
{
  number d, h;
  napoly p;

  if (!naIsChar0)
    return;
  p = ph;
  h = nacInit(1);
  while (p!=NULL)
  {
    d = nacLcm(h, napGetCoeff(p), nacRing);
    n_Delete(&h,nacRing);
    h = d;
    napIter(p);
  }
  if(!nacIsOne(h))
  {
    p = ph;
    p=p_Mult_nn(p,h,nacRing);p_Normalize(p,nacRing);
    n_Delete(&h,nacRing);
  }
  napContent(ph);
}

static napoly napGcd0(napoly a, napoly b)
{
  number x, y;
  if (!naIsChar0)
    return napInit(1);
  x = nacCopy(napGetCoeff(a));
  if (nacIsOne(x))
    return napInitz(x);
  while (napNext(a)!=NULL)
  {
    napIter(a);
    y = nacGcd(x, napGetCoeff(a), nacRing);
    n_Delete(&x,nacRing);
    x = y;
    if (nacIsOne(x))
      return napInitz(x);
  }
  do
  {
    y = nacGcd(x, napGetCoeff(b), nacRing);
    n_Delete(&x,nacRing);
    x = y;
    if (nacIsOne(x))
      return napInitz(x);
    napIter(b);
  }
  while (b!=NULL);
  return napInitz(x);
}

/*3
* result =gcd(a,b)
*/
static napoly napGcd(napoly a, napoly b)
{
  int i;
  napoly g, x, y, h;
  if ((a==NULL)
  || ((napNext(a)==NULL)&&(nacIsZero(napGetCoeff(a)))))
  {
    if ((b==NULL)
    || ((napNext(b)==NULL)&&(nacIsZero(napGetCoeff(b)))))
    {
      return napInit(1);
    }
    return napCopy(b);
  }
  else
  if ((b==NULL)
  || ((napNext(b)==NULL)&&(nacIsZero(napGetCoeff(b)))))
  {
    return napCopy(a);
  }
  if (naMinimalPoly != NULL)
  {
    if (napGetExp(a,1) >= napGetExp(b,1))
    {
      x = a;
      y = b;
    }
    else
    {
      x = b;
      y = a;
    }
    if (!naIsChar0) g = napInit(1);
    else            g = napGcd0(x, y);
    if (napNext(y)==NULL)
    {
      napSetExp(g,1, napExp(x, y));
      p_Setm(g,nacRing);
      return g;
    }
    x = napCopy(x);
    y = napCopy(y);
    loop
    {
      h = napRemainder(x, y);
      if (h==NULL)
      {
        napCleardenom(y);
        if (!nacIsOne(napGetCoeff(g)))
	{
          y=p_Mult_nn(y,napGetCoeff(g),nacRing);
	  p_Normalize(y,nacRing);
        }
        napDelete1(&g);
        return y;
      }
      else if (napNext(h)==NULL)
        break;
      x = y;
      y = h;
    }
    napDelete(&y);
    napDelete1(&h);
    napSetExp(g,1, napExp(a, b));
    p_Setm(g,nacRing);
    return g;
  }
  // Hmm ... this is a memory leak
  // x = (napoly)p_Init(nacRing);
  g=a;
  h=b;
  if (!naIsChar0) x = napInit(1);
  else            x = napGcd0(g,h);
  for (i=(naNumbOfPar-1); i>=0; i--)
  {
    napSetExp(x,i+1, napExpi(i,a,b));
    p_Setm(x,nacRing);
  }
  return x;
}


number napLcm(napoly a)
{
  number h = nacInit(1);

  if (naIsChar0)
  {
    number d;
    napoly b = a;

    while (b!=NULL)
    {
      d = nacLcm(h, napGetCoeff(b), nacRing);
      n_Delete(&h,nacRing);
      h = d;
      napIter(b);
    }
  }
  return h;
}


/*2
* meins  (for reduction in algebraic extension)
* checks if head of p divides head of q
* doesn't delete p and q
*/
static BOOLEAN napDivPoly (napoly p, napoly q)
{
  int j=1;   /* evtl. von naNumber.. -1 abwaerts zaehlen */

  while (napGetExp(p,j) <= napGetExp(q,j))
  {
    j++;
    if (j > naNumbOfPar)
      return 1;
  }
  return 0;
}


/*2
* meins  (for reduction in algebraic extension)
* Normalform of poly with naI
* changes q and returns it
*/
napoly napRedp (napoly q)
{
  napoly h = (napoly)p_Init(nacRing);
  int i=0,j;

  loop
  {
    if (napDivPoly (naI->liste[i], q))
    {
      /* h = lt(q)/lt(naI->liste[i])*/
      napGetCoeff(h) = nacCopy(napGetCoeff(q));
      for (j=naNumbOfPar; j>0; j--)
        napSetExp(h,j, napGetExp(q,j) - napGetExp(naI->liste[i],j));
      p_Setm(h,nacRing);
      h = napMult (h, napCopy(naI->liste[i]));
      h = napNeg (h);
      q = napAdd (q, napCopy(h));
      napDelete (&napNext(h));
      if (q == NULL)
      {
        napDelete(&h);
        return q;
      }
      /* try to reduce further */
      i = 0;
    }
    else
    {
      i++;
      if (i >= naI->anz)
      {
        napDelete(&h);
        return q;
      }
    }
  }
}


/*2
* meins  (for reduction in algebraic extension)
* reduces the tail of Poly q
* needs q != NULL
* changes q and returns it
*/
napoly napTailred (napoly q)
{
  napoly h;

  h = napNext(q);
  while (h != NULL)
  {
     h = napRedp (h);
     if (h == NULL)
        return q;
     napIter(h);
  }
  return q;
}


/*================ procedure for rational functions: naXXXX =================*/

/*2
*  z:= i
*/
number naInit(int i)
{
  if (i!=0)
  {
    napoly z=napInit(i);
    if (z!=NULL)
    {
      lnumber l = (lnumber)omAllocBin(rnumber_bin);
      l->z = z;
      l->s = 2;
      l->n = NULL;
      return (number)l;
    }
  }
  /*else*/
  return NULL;
}

number  naPar(int i)
{
  lnumber l = (lnumber)omAllocBin(rnumber_bin);
  l->s = 2;
  l->z = napInit(1);
  napSetExp(l->z,i,1);
  p_Setm(l->z,nacRing);
  l->n = NULL;
  return (number)l;
}

int     naParDeg(number n)     /* i := deg(n) */
{
  lnumber l = (lnumber)n;
  if (l==NULL) return -1;
  return napDeg(l->z);
}

//int     naParDeg(number n)     /* i := deg(n) */
//{
//  lnumber l = (lnumber)n;
//  if (l==NULL) return -1;
//  return napMaxDeg(l->z)+napMaxDeg(l->n);
//}

int     naSize(number n)     /* size desc. */
{
  lnumber l = (lnumber)n;
  if (l==NULL) return -1;
  int len_z;
  int len_n;
  int o=napMaxDegLen(l->z,len_z)+napMaxDegLen(l->n,len_n);
  return (len_z+len_n)+o;
}

/*2
* convert a number to int (if possible)
*/
int naInt(number &n)
{
  lnumber l=(lnumber)n;
  naNormalize(n);
  if ((l!=NULL)&&(l->n==NULL)&&(napIsConstant(l->z)))
  {
    return nacInt(napGetCoeff(l->z));
  }
  return 0;
}

/*2
*  deletes p
*/
void naDelete(number *p, const ring r)
{
  if ((*p)!=r->minpoly)
  {
    lnumber l = (lnumber) * p;
    if (l==NULL) return;
    p_Delete(&(l->z),r->algring);
    p_Delete(&(l->n),r->algring);
    omFreeBin((ADDRESS)l,  rnumber_bin);
  }
  *p = NULL;
}

/*2
* copy p to erg
*/
number naCopy(number p)
{
  if (p==NULL) return NULL;
  naTest(p);
  naNormalize(p);
  lnumber erg;
  lnumber src = (lnumber)p;
  erg = (lnumber)omAlloc0Bin(rnumber_bin);
  erg->z = napCopy(src->z);
  erg->n = napCopy(src->n);
  erg->s = src->s;
  return (number)erg;
}
number na_Copy(number p, const ring r)
{
  if (p==NULL) return NULL;
  lnumber erg;
  lnumber src = (lnumber)p;
  erg = (lnumber)omAlloc0Bin(rnumber_bin);
  erg->z = nap_Copy(src->z,r);
  erg->n = nap_Copy(src->n,r);
  erg->s = src->s;
  return (number)erg;
}

/*2
* a dummy number: 0
*/
void naNew(number *z)
{
  *z = NULL;
}

/*2
*  multiplication; r:= la * lb
*/
number naMult(number la, number lb)
{
  if ((la==NULL) || (lb==NULL))
    return NULL;

  lnumber a = (lnumber)la;
  lnumber b = (lnumber)lb;
  lnumber lo;
  napoly x;

  omCheckAddrSize(a,sizeof(snumber));
  omCheckAddrSize(b,sizeof(snumber));
  naTest(la);
  naTest(lb);

  lo = (lnumber)omAllocBin(rnumber_bin);
  lo->z = napMultCopy(a->z, b->z);

  if (a->n==NULL)
  {
    if (b->n==NULL)
      x = NULL;
    else
      x = napCopy(b->n);
  }
  else
  {
    if (b->n==NULL)
    {
      x = napCopy(a->n);
    }
    else
    {
      x = napMultCopy(b->n, a->n);
    }
  }
  if (naMinimalPoly!=NULL)
  {
    if (napGetExp(lo->z,1) >= napGetExp(naMinimalPoly,1))
      lo->z = napRemainder(lo->z, naMinimalPoly);
    if ((x!=NULL) && (napGetExp(x,1) >= napGetExp(naMinimalPoly,1)))
      x = napRemainder(x, naMinimalPoly);
  }
  if (naI!=NULL)
  {
    lo->z = napRedp (lo->z);
    if (lo->z != NULL)
       lo->z = napTailred (lo->z);
    if (x!=NULL)
    {
      x = napRedp (x);
      if (x!=NULL)
        x = napTailred (x);
    }
  }
  if ((x!=NULL) && (napIsConstant(x)) && nacIsOne(napGetCoeff(x)))
    napDelete(&x);
  lo->n = x;
  if(lo->z==NULL)
  {
    omFreeBin((ADDRESS)lo, rnumber_bin);
    lo=NULL;
  }
#if 1
  else if (lo->n!=NULL)
  {
    lo->s = 0;
    number luu=(number)lo;
    naNormalize(luu);
    lo=(lnumber)luu;
  }
  else
    lo->s=3;
#endif
  naTest((number)lo);
  return (number)lo;
}

number naIntDiv(number la, number lb)
{
  lnumber res;
  lnumber a = (lnumber)la;
  lnumber b = (lnumber)lb;
  if (a==NULL)
  {
    return NULL;
  }
  if (b==NULL)
  {
    WerrorS(nDivBy0);
    return NULL;
  }
  naNormalize(la);
  assume(a->z!=NULL && b->z!=NULL);
  assume(a->n==NULL && b->n==NULL);
  res = (lnumber)omAllocBin(rnumber_bin);
  res->z = napCopy(a->z);
  res->n = napCopy(b->z);
  res->s = 0;
  number nres=(number)res;
  naNormalize(nres);

  //napDelete(&res->n);
  naTest(nres);
  return nres;
}

/*2
*  division; lo:= la / lb
*/
number naDiv(number la, number lb)
{
  lnumber lo;
  lnumber a = (lnumber)la;
  lnumber b = (lnumber)lb;
  napoly x;

  if ((a==NULL) || (a->z==NULL))
    return NULL;

  if ((b==NULL) || (b->z==NULL))
  {
    WerrorS(nDivBy0);
    return NULL;
  }
  omCheckAddrSize(a,sizeof(snumber));
  omCheckAddrSize(b,sizeof(snumber));
  lo = (lnumber)omAllocBin(rnumber_bin);
  if (b->n!=NULL)
    lo->z = napMultCopy(a->z, b->n);
  else
    lo->z = napCopy(a->z);
  if (a->n!=NULL)
    x = napMultCopy(b->z, a->n);
  else
    x = napCopy(b->z);
  if (naMinimalPoly!=NULL)
  {
    if (napGetExp(lo->z,1) >= napGetExp(naMinimalPoly,1))
      lo->z = napRemainder(lo->z, naMinimalPoly);
    if (napGetExp(x,1) >= napGetExp(naMinimalPoly,1))
      x = napRemainder(x, naMinimalPoly);
  }
  if (naI!=NULL)
  {
    lo->z = napRedp (lo->z);
    if (lo->z != NULL)
       lo->z = napTailred (lo->z);
    if (x!=NULL)
    {
      x = napRedp (x);
      if (x!=NULL)
        x = napTailred (x);
    }
  }
  if ((napIsConstant(x)) && nacIsOne(napGetCoeff(x)))
    napDelete(&x);
  lo->n = x;
  if (lo->n!=NULL)
  {
    lo->s = 0;
    number luu=(number)lo;
    naNormalize(luu);
    lo=(lnumber)luu;
  }
  else
    lo->s=3;
  naTest((number)lo);
  return (number)lo;
}

/*2
*  za:= - za
*/
number naNeg(number za)
{
  if (za!=NULL)
  {
    lnumber e = (lnumber)za;
    naTest(za);
    e->z = napNeg(e->z);
  }
  return za;
}

/*2
* 1/a
*/
number naInvers(number a)
{
  lnumber lo;
  lnumber b = (lnumber)a;
  napoly x;

  if (b==NULL)
  {
    WerrorS(nDivBy0);
    return NULL;
  }
  omCheckAddrSize(b,sizeof(snumber));
  lo = (lnumber)omAlloc0Bin(rnumber_bin);
  lo->s = b->s;
  if (b->n!=NULL)
    lo->z = napCopy(b->n);
  else
    lo->z = napInit(1);
  x = b->z;
  if ((!napIsConstant(x)) || !nacIsOne(napGetCoeff(x)))
    x = napCopy(x);
  else
  {
    lo->n = NULL;
    naTest((number)lo);
    return (number)lo;
  }
  if (naMinimalPoly!=NULL)
  {
    x = napInvers(x, naMinimalPoly);
    x = napMult(x, lo->z);
    if (napGetExp(x,1) >= napGetExp(naMinimalPoly,1))
      x = napRemainder(x, naMinimalPoly);
    lo->z = x;
    lo->n = NULL;
    lo->s = 2;
    while (x!=NULL)
    {
      nacNormalize(napGetCoeff(x));
      napIter(x);
    }
  }
  else
    lo->n = x;
  if (lo->n!=NULL)
  {
     number luu=(number)lo;
     naNormalize(luu);
     lo=(lnumber)luu;
  }
  naTest((number)lo);
  return (number)lo;
}


BOOLEAN naIsZero(number za)
{
  lnumber zb = (lnumber)za;
  naTest(za);
#ifdef LDEBUG
  if ((zb!=NULL) && (zb->z==NULL)) WerrorS("internal zero error(2)");
#endif
  return ((zb==NULL) || (zb->z==NULL));
}


BOOLEAN naGreaterZero(number za)
{
  lnumber zb = (lnumber)za;
#ifdef LDEBUG
  if ((zb!=NULL) && (zb->z==NULL)) WerrorS("internal zero error(3)");
#endif
  naTest(za);
  if ((zb!=NULL) && (zb->z!=NULL))
  {
    return (nacGreaterZero(napGetCoeff(zb->z))||(!napIsConstant(zb->z)));
  }
  /* else */ return FALSE;
}


/*2
* a = b ?
*/
BOOLEAN naEqual (number a, number b)
{
  if(a==b) return TRUE;
  if((a==NULL)&&(b!=NULL)) return FALSE;
  if((b==NULL)&&(a!=NULL)) return FALSE;

  lnumber aa=(lnumber)a;
  lnumber bb=(lnumber)b;

  int an_deg=0;
  if(aa->n!=NULL)
    an_deg=napDeg(aa->n);
  int bn_deg=0;
  if(bb->n!=NULL)
    bn_deg=napDeg(bb->n);
  if(an_deg+napDeg(bb->z)!=bn_deg+napDeg(aa->z))
    return FALSE;
#if 0
  naNormalize(a);
  aa=(lnumber)a;
  naNormalize(b);
  bb=(lnumber)b;
  if((aa->n==NULL)&&(bb->n!=NULL)) return FALSE;
  if((bb->n==NULL)&&(aa->n!=NULL)) return FALSE;
  if(napComp(aa->z,bb->z)!=0) return FALSE;
  if((aa->n!=NULL) && (napComp(aa->n,bb->n))) return FALSE;
#endif
  number h = naSub(a, b);
  BOOLEAN bo = naIsZero(h);
  naDelete(&h,currRing);
  return bo;
}


BOOLEAN naGreater (number a, number b)
{
  if (naIsZero(a))
    return FALSE;
  if (naIsZero(b))
    return TRUE; /* a!= 0)*/
  return napDeg(((lnumber)a)->z)>napDeg(((lnumber)b)->z);
}

/*2
* reads a number
*/
const char  *naRead(const char *s, number *p)
{
  napoly x;
  lnumber a;
  s = napRead(s, &x);
  if (x==NULL)
  {
    *p = NULL;
    return s;
  }
  *p = (number)omAlloc0Bin(rnumber_bin);
  a = (lnumber)*p;
  if ((naMinimalPoly!=NULL)
  && (napGetExp(x,1) >= napGetExp(naMinimalPoly,1)))
    a->z = napRemainder(x, naMinimalPoly);
  else if (naI!=NULL)
  {
    a->z = napRedp(x);
    if (a->z != NULL)
      a->z = napTailred (a->z);
  }
  else
    a->z = x;
  if(a->z==NULL)
  {
    omFreeBin((ADDRESS)*p, rnumber_bin);
    *p=NULL;
  }
  else
  {
    a->n = NULL;
    a->s = 0;
    naTest(*p);
  }
  return s;
}

/*2
* tries to convert a number to a name
*/
char * naName(number n)
{
  lnumber ph = (lnumber)n;
  if ((ph==NULL)||(ph->z==NULL))
    return NULL;
  int i;
  char *s=(char *)omAlloc(4* naNumbOfPar);
  char *t=(char *)omAlloc(8);
  s[0]='\0';
  for (i = 0; i <= naNumbOfPar - 1; i++)
  {
    if (napGetExp(ph->z,i+1) > 0)
    {
      if (napGetExp(ph->z,i+1) >1)
      {
        sprintf(t,"%s%d",naParNames[i],(int)napGetExp(ph->z,i+1));
        strcat(s,t);
      }
      else
      {
        strcat(s,naParNames[i]);
      }
    }
  }
  omFreeSize((ADDRESS)t,8);
  if (s[0]=='\0')
  {
    omFree((ADDRESS)s);
    return NULL;
  }
  return s;
}

/*2
*  writes a number
*/
void naWrite(number &phn)
{
  lnumber ph = (lnumber)phn;
  if ((ph==NULL)||(ph->z==NULL))
    StringAppendS("0");
  else
  {
    phn->s = 0;
    naNormalize(phn);
    BOOLEAN has_denom=(ph->n!=NULL);
    napWrite(ph->z,has_denom/*(ph->n!=NULL)*/);
    if (has_denom/*(ph->n!=NULL)*/)
    {
      StringAppendS("/");
      napWrite(ph->n,TRUE);
    }
  }
}

/*2
* za == 1 ?
*/
BOOLEAN naIsOne(number za)
{
  lnumber a = (lnumber)za;
  napoly x, y;
  number t;
  if (a==NULL) return FALSE;
  omCheckAddrSize(a,sizeof(snumber));
#ifdef LDEBUG
  if (a->z==NULL) WerrorS("internal zero error(4)");
#endif
  if (a->n==NULL)
  {
    if (napIsConstant(a->z))
    {
#ifdef LDEBUG
       if (a->z==NULL) return FALSE;
       else
#endif
         return nacIsOne(napGetCoeff(a->z));
    }
    else                 return FALSE;
  }
  x = a->z;
  y = a->n;
  do
  {
    if (napComp(x, y))
      return FALSE;
    else
    {
      t = nacSub(napGetCoeff(x), napGetCoeff(y));
      if (!nacIsZero(t))
      {
        n_Delete(&t,nacRing);
        return FALSE;
      }
      else
        n_Delete(&t,nacRing);
    }
    napIter(x);
    napIter(y);
  }
  while ((x!=NULL) && (y!=NULL));
  if ((x!=NULL) || (y!=NULL)) return FALSE;
  napDelete(&a->z);
  napDelete(&a->n);
  a->z = napInit(1);
  a->n = NULL;
  a->s = 2;
  return TRUE;
}

/*2
* za == -1 ?
*/
BOOLEAN naIsMOne(number za)
{
  lnumber a = (lnumber)za;
  napoly x, y;
  number t;
  if (a==NULL) return FALSE;
  omCheckAddrSize(a,sizeof(snumber));
#ifdef LDEBUG
  if (a->z==NULL)
  {
    WerrorS("internal zero error(5)");
    return FALSE;
  }
#endif
  if (a->n==NULL)
  {
    if (napIsConstant(a->z)) return nacIsMOne(napGetCoeff(a->z));
    /*else                   return FALSE;*/
  }
  return FALSE;
}

/*2
* returns the i-th power of p (i>=0)
*/
void naPower(number p, int i, number *rc)
{
  number x;
  *rc = naInit(1);
  for (; i > 0; i--)
  {
    x = naMult(*rc, p);
    naDelete(rc,currRing);
    *rc = x;
  }
}

/*2
* result =gcd(a,b)
*/
number naGcd(number a, number b, const ring r)
{
  if (a==NULL)  return naCopy(b);
  if (b==NULL)  return naCopy(a);

  lnumber x, y;
  lnumber result = (lnumber)omAlloc0Bin(rnumber_bin);

  x = (lnumber)a;
  y = (lnumber)b;
  if ((naNumbOfPar == 1) && (naMinimalPoly!=NULL))
  {
    if (napNext(x->z)!=NULL)
      result->z = napCopy(x->z);
    else
      result->z = napGcd0(x->z, y->z);
  }
  else
#ifndef HAVE_FACTORY
    result->z = napGcd(x->z, y->z); // change from napGcd0
#else
  {
    int c=ABS(nGetChar());
    if (c==1) c=0;
    setCharacteristic( c );

    napoly rz=napGcd(x->z, y->z);
    CanonicalForm F, G, R;
    R=convSingPFactoryP(rz,0,nacRing);
    p_Normalize(x->z,nacRing);
    F=convSingPFactoryP(x->z,0,nacRing)/R;
    p_Normalize(y->z,nacRing);
    G=convSingPFactoryP(y->z,0,nacRing)/R;
    F = gcd( F, G );
    if (F.isOne())
      result->z= rz;
    else
    {
      napDelete(&rz);
      result->z=convFactoryPSingP( F*R, nacRing );
      p_Normalize(result->z,nacRing);
    }
  }
#endif
  naTest((number)result);
  return (number)result;
}


/*2
* naNumbOfPar = 1:
* clears denominator         algebraic case;
* tries to simplify ratio    transcendental case;
*
* cancels monomials
* occuring in denominator
* and enumerator  ?          naNumbOfPar != 1;
*
*/
void naNormalize(number &pp)
{

  //naTest(pp); // input may not be "normal"
  lnumber p = (lnumber)pp;

  if ((p==NULL) /*|| (p->s==2)*/)
    return;
  p->s = 2;
  napoly x = p->z;
  napoly y = p->n;
  if ((y!=NULL) && (naMinimalPoly!=NULL))
  {
    y = napInvers(y, naMinimalPoly);
    x = napMult(x, y);
    if (napGetExp(x,1) >= napGetExp(naMinimalPoly,1))
      x = napRemainder(x, naMinimalPoly);
    p->z = x;
    p->n = y = NULL;
  }
  /* check for degree of x too high: */
  if ((x!=NULL) && (naMinimalPoly!=NULL) && (x!=naMinimalPoly)
  && (napGetExp(x,1)>napGetExp(naMinimalPoly,1)))
  // DO NOT REDUCE naMinimalPoly with itself
  {
    x = napRemainder(x, naMinimalPoly);
    p->z = x;
  }
  /* normalize all coefficients in n and z (if in Q) */
  if (naIsChar0)
  {
    while(x!=NULL)
    {
      nacNormalize(napGetCoeff(x));
      napIter(x);
    }
    x = p->z;
  }
  if (y==NULL) return;
  if (naIsChar0)
  {
    while(y!=NULL)
    {
      nacNormalize(napGetCoeff(y));
      napIter(y);
    }
    y = p->n;
  // p->n !=NULL:
  /* collect all denoms from y and multiply x and y by it */
    number n=napLcm(y);
    x=p_Mult_nn(x,n,nacRing);p_Normalize(x,nacRing);
    y=p_Mult_nn(y,n,nacRing);p_Normalize(y,nacRing);
    n_Delete(&n,nacRing);
    while(x!=NULL)
    {
      nacNormalize(napGetCoeff(x));
      napIter(x);
    }
    x = p->z;
    while(y!=NULL)
    {
      nacNormalize(napGetCoeff(y));
      napIter(y);
    }
    y = p->n;
  }
  if ((naMinimalPoly == NULL) && (x!=NULL) && (y!=NULL))
  {
    int i;
    for (i=naNumbOfPar-1; i>=0; i--)
    {
      napoly xx=x;
      napoly yy=y;
      int m = napExpi(i, yy, xx);
      if (m != 0)          // in this case xx!=NULL!=yy
      {
        while (xx != NULL)
        {
          napAddExp(xx,i+1, -m);
          napIter(xx);
        }
        while (yy != NULL)
        {
          napAddExp(yy,i+1, -m);
          napIter(yy);
        }
      }
    }
  }
  if (napIsConstant(y)) /* i.e. => simplify to (1/c)*z / monom */
  {
    if (nacIsOne(napGetCoeff(y)))
    {
      napDelete1(&y);
      p->n = NULL;
      naTest(pp);
      return;
    }
    number h1 = nacInvers(napGetCoeff(y));
    nacNormalize(h1);
    x=p_Mult_nn(x,h1,nacRing);p_Normalize(x,nacRing);
    n_Delete(&h1,nacRing);
    napDelete1(&y);
    p->n = NULL;
    naTest(pp);
    return;
  }
  if (naNumbOfPar == 1) /* apply built-in gcd */
  {
    napoly x1,y1;
    if (napGetExp(x,1) >= napGetExp(y,1))
    {
      x1 = napCopy(x);
      y1 = napCopy(y);
    }
    else
    {
      x1 = napCopy(y);
      y1 = napCopy(x);
    }
    napoly r;
    loop
    {
      r = napRemainder(x1, y1);
      if ((r==NULL) || (napNext(r)==NULL)) break;
      x1 = y1;
      y1 = r;
    }
    if (r!=NULL)
    {
      napDelete(&r);
      napDelete(&y1);
    }
    else
    {
      napDivMod(x, y1, &(p->z), &r);
      napDivMod(y, y1, &(p->n), &r);
      napDelete(&y1);
    }
    x = p->z;
    y = p->n;
    /* collect all denoms from y and multiply x and y by it */
    if (naIsChar0)
    {
      number n=napLcm(y);
      x=p_Mult_nn(x,n,nacRing);p_Normalize(x,nacRing);
      y=p_Mult_nn(y,n,nacRing);p_Normalize(y,nacRing);
      n_Delete(&n,nacRing);
      while(x!=NULL)
      {
        nacNormalize(napGetCoeff(x));
        napIter(x);
      }
      x = p->z;
      while(y!=NULL)
      {
        nacNormalize(napGetCoeff(y));
        napIter(y);
      }
      y = p->n;
    }
    if (napNext(y)==NULL)
    {
      if (nacIsOne(napGetCoeff(y)))
      {
        if (napGetExp(y,1)==0)
        {
          napDelete1(&y);
          p->n = NULL;
        }
        naTest(pp);
        return;
      }
    }
  }
#ifdef HAVE_FACTORY
  else
  {
    napoly xx,yy;
    singclap_algdividecontent(x,y,xx,yy);
    if (xx!=NULL)
    {
      p->z=xx;
      p->n=yy;
      napDelete(&x);
      napDelete(&y);
    }
  }
#endif
  /* remove common factors from z and n */
  x=p->z;
  y=p->n;
  if(!nacGreaterZero(napGetCoeff(y)))
  {
    x=napNeg(x);
    y=napNeg(y);
  }
  number g=nacCopy(napGetCoeff(x));
  napIter(x);
  while (x!=NULL)
  {
    number d=nacGcd(g,napGetCoeff(x), nacRing);
    if(nacIsOne(d))
    {
      n_Delete(&g,nacRing);
      n_Delete(&d,nacRing);
      naTest(pp);
      return;
    }
    n_Delete(&g,nacRing);
    g = d;
    napIter(x);
  }
  while (y!=NULL)
  {
    number d=nacGcd(g,napGetCoeff(y), nacRing);
    if(nacIsOne(d))
    {
      n_Delete(&g,nacRing);
      n_Delete(&d,nacRing);
      naTest(pp);
      return;
    }
    n_Delete(&g,nacRing);
    g = d;
    napIter(y);
  }
  x=p->z;
  y=p->n;
  while (x!=NULL)
  {
    number d = nacIntDiv(napGetCoeff(x),g);
    napSetCoeff(x,d);
    napIter(x);
  }
  while (y!=NULL)
  {
    number d = nacIntDiv(napGetCoeff(y),g);
    napSetCoeff(y,d);
    napIter(y);
  }
  n_Delete(&g,nacRing);
  naTest(pp);
}

/*2
* returns in result->n 1
* and in     result->z the lcm(a->z,b->n)
*/
number naLcm(number la, number lb, const ring r)
{
  lnumber result;
  lnumber a = (lnumber)la;
  lnumber b = (lnumber)lb;
  result = (lnumber)omAlloc0Bin(rnumber_bin);
  //if (((naMinimalPoly==NULL) && (naI==NULL)) || !naIsChar0)
  //{
  //  result->z = napInit(1);
  //  return (number)result;
  //}
  naNormalize(lb);
  naTest(la);
  naTest(lb);
  napoly x = napCopy(a->z);
  number t = napLcm(b->z); // get all denom of b->z
  if (!nacIsOne(t))
  {
    number bt, r;
    napoly xx=x;
    while (xx!=NULL)
    {
      bt = nacGcd(t, napGetCoeff(xx), nacRing);
      r = nacMult(t, napGetCoeff(xx));
      n_Delete(&napGetCoeff(xx),nacRing);
      napGetCoeff(xx) = nacDiv(r, bt);
      nacNormalize(napGetCoeff(xx));
      n_Delete(&bt,nacRing);
      n_Delete(&r,nacRing);
      napIter(xx);
    }
  }
  n_Delete(&t,nacRing);
  result->z = x;
#ifdef HAVE_FACTORY
  if (b->n!=NULL)
  {
    result->z=singclap_alglcm(result->z,b->n);
    napDelete(&x);
  }
#endif
  naTest(la);
  naTest(lb);
  naTest((number)result);
  return ((number)result);
}

/*2
* input: a set of constant polynomials
* sets the global variable naI
*/
void naSetIdeal(ideal I)
{
  int i;

  if (idIs0(I))
  {
    for (i=naI->anz-1; i>=0; i--)
      napDelete(&naI->liste[i]);
    omFreeBin((ADDRESS)naI, snaIdeal_bin);
    naI=NULL;
  }
  else
  {
    lnumber h;
    number a;
    napoly x;

    naI=(naIdeal)omAllocBin(snaIdeal_bin);
    naI->anz=IDELEMS(I);
    naI->liste=(napoly*)omAlloc(naI->anz*sizeof(napoly));
    for (i=IDELEMS(I)-1; i>=0; i--)
    {
      h=(lnumber)pGetCoeff(I->m[i]);
      /* We only need the enumerator of h, as we expect it to be a polynomial */
      naI->liste[i]=napCopy(h->z);
      /* If it isn't normalized (lc = 1) do this */
      if (!nacIsOne(napGetCoeff(naI->liste[i])))
      {
        x=naI->liste[i];
        nacNormalize(napGetCoeff(x));
        a=nacCopy(napGetCoeff(x));
        number aa=nacInvers(a);
        n_Delete(&a,nacRing);
        x=p_Mult_nn(x,aa,nacRing);p_Normalize(x,nacRing);
        n_Delete(&aa,nacRing);
      }
    }
  }
}

/*2
* map Z/p -> Q(a)
*/
number naMapP0(number c)
{
  if (npIsZero(c)) return NULL;
  lnumber l=(lnumber)omAllocBin(rnumber_bin);
  l->s=2;
  l->z=(napoly)p_Init(nacRing);
  int i=(int)((long)c);
  if (i>(naPrimeM>>2)) i-=naPrimeM;
  napGetCoeff(l->z)=nlInit(i);
  l->n=NULL;
  return (number)l;
}

/*2
* map Q -> Q(a)
*/
number naMap00(number c)
{
  if (nlIsZero(c)) return NULL;
  lnumber l=(lnumber)omAllocBin(rnumber_bin);
  l->s=0;
  l->z=(napoly)p_Init(nacRing);
  napGetCoeff(l->z)=nlCopy(c);
  l->n=NULL;
  return (number)l;
}

/*2
* map Z/p -> Z/p(a)
*/
number naMapPP(number c)
{
  if (npIsZero(c)) return NULL;
  lnumber l=(lnumber)omAllocBin(rnumber_bin);
  l->s=2;
  l->z=(napoly)p_Init(nacRing);
  napGetCoeff(l->z)=c; /* omit npCopy, because npCopy is a no-op */
  l->n=NULL;
  return (number)l;
}

/*2
* map Z/p' -> Z/p(a)
*/
number naMapPP1(number c)
{
  if (npIsZero(c)) return NULL;
  int i=(int)((long)c);
  if (i>naPrimeM) i-=naPrimeM;
  number n=npInit(i);
  if (npIsZero(n)) return NULL;
  lnumber l=(lnumber)omAllocBin(rnumber_bin);
  l->s=2;
  l->z=(napoly)p_Init(nacRing);
  napGetCoeff(l->z)=n;
  l->n=NULL;
  return (number)l;
}

/*2
* map Q -> Z/p(a)
*/
number naMap0P(number c)
{
  if (nlIsZero(c)) return NULL;
  number n=npInit(nlModP(c,npPrimeM));
  if (npIsZero(n)) return NULL;
  npTest(n);
  lnumber l=(lnumber)omAllocBin(rnumber_bin);
  l->s=2;
  l->z=(napoly)p_Init(nacRing);
  napGetCoeff(l->z)=n;
  l->n=NULL;
  return (number)l;
}

static number (*nacMap)(number);
static int naParsToCopy;
static ring napMapRing;
static napoly napMap(napoly p)
{
  napoly w, a;

  if (p==NULL) return NULL;
  a = w = (napoly)p_Init(nacRing);
  int i;
  for(i=1;i<=naParsToCopy;i++)
    napSetExp(a,i,napGetExpFrom(p,i,napMapRing));
  p_Setm(a,nacRing);
  napGetCoeff(w) = nacMap(napGetCoeff(p));
  loop
  {
    napIter(p);
    if (p==NULL) break;
    napNext(a) = (napoly)p_Init(nacRing);
    napIter(a);
    for(i=1;i<=naParsToCopy;i++)
      napSetExp(a,i,napGetExpFrom(p,i,napMapRing));
    p_Setm(a,nacRing);
    napGetCoeff(a) = nacMap(napGetCoeff(p));
  }
  napNext(a) = NULL;
  return w;
}

static napoly napPerm(napoly p,const int *par_perm,const ring src_ring,const nMapFunc nMap)
{
  napoly w, a;

  if (p==NULL) return NULL;
  w = (napoly)p_Init(nacRing);
  int i;
  BOOLEAN not_null=TRUE;
  loop
  {
    for(i=1;i<=rPar(src_ring);i++)
    {
      int e;
      if (par_perm!=NULL) e=par_perm[i-1];
      else                e=-i;
      int ee=napGetExpFrom(p,i,src_ring);
      if (e<0)
        napSetExp(w,-e,ee);
      else if (ee>0)
        not_null=FALSE;
    }
    napGetCoeff(w) = nMap(napGetCoeff(p));
    p_Setm(w,nacRing);
    napIter(p);
    if (!not_null)
    {
      if (p==NULL)
      {
        p_Delete(&w,nacRing);
        return NULL;
      }
      /* else continue*/
      n_Delete(&(napGetCoeff(w)),nacRing);
    }
    else
    {
      if (p==NULL) return w;
      else
      {
        napNext(w)=napPerm(p,par_perm,src_ring,nMap);
        return w;
      }
    }
  }
}

/*2
* map _(a) -> _(b)
*/
number naMapQaQb(number c)
{
  if (c==NULL) return NULL;
  lnumber erg= (lnumber)omAlloc0Bin(rnumber_bin);
  lnumber src =(lnumber)c;
  erg->s=src->s;
  erg->z=napMap(src->z);
  erg->n=napMap(src->n);
  if (naMinimalPoly!=NULL)
  {
    if (napGetExp(erg->z,1) >= napGetExp(naMinimalPoly,1))
    {
      erg->z = napRemainder(erg->z, naMinimalPoly);
      if (erg->z==NULL)
      {
        number t_erg=(number)erg;
        naDelete(&t_erg,currRing);
        return (number)NULL;
      }
    }
    if (erg->n!=NULL)
    {
      if (napGetExp(erg->n,1) >= napGetExp(naMinimalPoly,1))
        erg->n = napRemainder(erg->n, naMinimalPoly);
      if ((napIsConstant(erg->n)) && nacIsOne(napGetCoeff(erg->n)))
        napDelete(&(erg->n));
    }
  }
  return (number)erg;
}

nMapFunc naSetMap(ring src, ring dst)
{
  if (rField_is_Q_a(dst)) /* -> Q(a) */
  {
    if (rField_is_Q(src))
    {
      return naMap00;   /*Q -> Q(a)*/
    }
    if (rField_is_Zp(src))
    {
      naPrimeM = rChar(src);
      return naMapP0;  /* Z/p -> Q(a)*/
    }
    if (rField_is_Q_a(src))
    {
      int i;
      naParsToCopy=0;
      for(i=0;i<rPar(src);i++)
      {
        if ((i>=rPar(dst))
        ||(strcmp(src->parameter[i],dst->parameter[i])!=0))
           return NULL;
        naParsToCopy++;
      }
      napMapRing=src;
      nacMap=nacCopy;
      if ((naParsToCopy==rPar(dst))&&(naParsToCopy==rPar(src)))
        return naCopy;    /* Q(a) -> Q(a) */
      return naMapQaQb;   /* Q(a..) -> Q(a..) */
    }
  }
  /*-----------------------------------------------------*/
  if (rField_is_Zp_a(dst)) /* -> Z/p(a) */
  {
    if (rField_is_Q(src))
    {
      return naMap0P;   /*Q -> Z/p(a)*/
    }
    if (rField_is_Zp(src))
    {
      int c=rChar(src);
      if (c==npPrimeM)
      {
        return naMapPP;  /* Z/p -> Z/p(a)*/
      }
      else
      {
        naPrimeM = c;
        return naMapPP1;  /* Z/p' -> Z/p(a)*/
      }
    }
    if (rField_is_Zp_a(src))
    {
      if (rChar(src)==rChar(dst))
      {
        nacMap=nacCopy;
      }
      else
      {
        npMapPrime=rChar(src);
        nacMap = npMapP;
      }
      int i;
      naParsToCopy=0;
      for(i=0;i<rPar(src);i++)
      {
        if ((i>=rPar(dst))
        ||(strcmp(src->parameter[i],dst->parameter[i])!=0))
           return NULL;
        naParsToCopy++;
      }
      napMapRing=src;
      if ((naParsToCopy==rPar(dst))&&(naParsToCopy==rPar(src))
      && (nacMap==nacCopy))
        return naCopy;    /* Z/p(a) -> Z/p(a) */
      return naMapQaQb;   /* Z/p(a),Z/p'(a) -> Z/p(b)*/
    }
  }
  return NULL;      /* default */
}

/*2
* convert a napoly number into a poly
*/
poly naPermNumber(number z, int * par_perm, int P, ring oldRing)
{
  if (z==NULL) return NULL;
  poly res=NULL;
  poly p;
  napoly za=((lnumber)z)->z;
  napoly zb=((lnumber)z)->n;
  nMapFunc nMap=naSetMap(oldRing,currRing);
  if (currRing->parameter!=NULL)
    nMap=currRing->algring->cf->cfSetMap(oldRing->algring, nacRing);
  else
    nMap=currRing->cf->cfSetMap(oldRing->algring, currRing);
  if (nMap==NULL) return NULL; /* emergency exit only */
  do
  {
    p = pInit();
    pNext(p)=NULL;
    nNew(&pGetCoeff(p));
    int i;
    for(i=pVariables;i;i--)
       pSetExp(p,i, 0);
    pSetComp(p, 0);
    napoly pa=NULL;
    lnumber pan;
    if (currRing->parameter!=NULL)
    {
      assume(oldRing->algring!=NULL);
      pGetCoeff(p)=(number)omAlloc0Bin(rnumber_bin);
      pan=(lnumber)pGetCoeff(p);
      pan->s=2;
      pan->z=napInitz(nMap(napGetCoeff(za)));
      pa=pan->z;
    }
    else
    {
      pGetCoeff(p)=nMap(napGetCoeff(za));
    }
    for(i=0;i<P;i++)
    {
      if(napGetExpFrom(za,i+1,oldRing)!=0)
      {
        if(par_perm==NULL)
        {
          if ((rPar(currRing)>=i) && (pa!=NULL))
          {
            napSetExp(pa,i+1,napGetExpFrom(za,i+1,oldRing));
            p_Setm(pa,nacRing);
          }
          else
          {
            pDelete(&p);
            break;
          }
        }
        else if(par_perm[i]>0)
          pSetExp(p,par_perm[i],napGetExpFrom(za,i+1,oldRing));
        else if((par_perm[i]<0)&&(pa!=NULL))
        {
          napSetExp(pa,-par_perm[i], napGetExpFrom(za,i+1,oldRing));
          p_Setm(pa,nacRing);
        }
        else
        {
          pDelete(&p);
          break;
        }
      }
    }
    if (p!=NULL)
    {
      pSetm(p);
      if (zb!=NULL)
      {
        if  (currRing->P>0)
        {
          pan->n=napPerm(zb,par_perm,oldRing,nMap);
          if(pan->n==NULL) /* error in mapping or mapping to variable */
            pDelete(&p);
        }
        else
          pDelete(&p);
      }
      pTest(p);
      res=pAdd(res,p);
    }
    napIter(za);
  }
  while (za!=NULL);
  pTest(res);
  return res;
}

number   naGetDenom(number &n, const ring r)
{
  if (r==currRing) naNormalize(n);
  lnumber x=(lnumber)n;
  if (x->n!=NULL)
  {
    lnumber rr=(lnumber)omAlloc0Bin(rnumber_bin);
    rr->z=nap_Copy(naGetDenom0(x),r);
    rr->s = 2;
    return (number)rr;
  }
  return r->cf->nInit(1);
}
number   naGetNumerator(number &n, const ring r)
{
  if (r==currRing) naNormalize(n);
  lnumber x=(lnumber)n;
  if (x!=NULL)
  {
    lnumber rr=(lnumber)omAlloc0Bin(rnumber_bin);
    rr->z=nap_Copy(naGetNom0(x),r);
    rr->s = 2;
    return (number)rr;
  }
  return NULL;
}

#ifdef LDEBUG
BOOLEAN naDBTest(number a, const char *f,const int l)
{
  lnumber x=(lnumber)a;
  if (x == NULL)
    return TRUE;
  omCheckAddrSize(a, sizeof(snumber));
  napoly p = x->z;
  if (p==NULL)
  {
    Print("0/* in %s:%d\n",f,l);
    return FALSE;
  }
  while(p!=NULL)
  {
    if ((naIsChar0 && nlIsZero(napGetCoeff(p)))
    || ((!naIsChar0) && npIsZero(napGetCoeff(p))))
    {
      Print("coeff 0 in %s:%d\n",f,l);
      return FALSE;
    }
    if((naMinimalPoly!=NULL)&&(napGetExp(p,1)>napGetExp(naMinimalPoly,1))
    &&(p!=naMinimalPoly))
    {
      Print("deg>minpoly in %s:%d\n",f,l);
      return FALSE;
    }
    //if (naIsChar0 && (((int)p->ko &3) == 0) && (p->ko->s==0) && (x->s==2))
    //{
    //  Print("normalized with non-normal coeffs in %s:%d\n",f,l);
    //  return FALSE;
    //}
    if (naIsChar0 && !(nlDBTest(napGetCoeff(p),f,l)))
      return FALSE;
    napIter(p);
  }
  p = naGetDenom0(x);
  while(p!=NULL)
  {
    if (naIsChar0 && !(nlDBTest(napGetCoeff(p),f,l)))
      return FALSE;
    napIter(p);
  }
  return TRUE;
}
#endif
//-------------------------------------------------------------------------
/// static: Normalize: case mipo==0, result: n->z, n->n integer
static void naNormalize_0(number n)
{
  lnumber p = (lnumber) n;
  napoly x = p->z;
  napoly y = p->n;
  if (nacRing->cf->nNormalize!=nDummy2)
  {
    /* collect all denoms from x and multiply x and y by it */
    number n;
    if (y!=NULL)
    {
      n=napLcm(y);
      x=p_Mult_nn(x,n,nacRing); p_Normalize(x,nacRing);
      y=p_Mult_nn(y,n,nacRing); p_Normalize(y,nacRing);
      n_Delete(&n,nacRing);
    }
    /* collect all denoms from x and multiply x and y by it */
    n=napLcm(y);
    x=p_Mult_nn(x,n,nacRing);
    if (y!=NULL)   { y=p_Mult_nn(y,n,nacRing); n_Delete(&n,nacRing); }
    else           y=p_NSet(n,nacRing);
    p->n=y;
    // normalize all coeffs in x
    p_Normalize(x,nacRing);
    // normalize all coeffs in y
    p_Normalize(y,nacRing);
  }
}
/// return la+li
number  naAdd(number la, number lb)               /* lu := la+lb */
{
  napoly x, y;
  lnumber lu;
  lnumber a = (lnumber)la;
  lnumber b = (lnumber)lb;
  if (a==NULL) return naCopy(lb);
  if (b==NULL) return naCopy(la);
  omCheckAddrSize(a,sizeof(snumber));
  omCheckAddrSize(b,sizeof(snumber));
  lu = (lnumber)omAllocBin(rnumber_bin);
  if (b->n!=NULL) x = pp_Mult_qq(a->z, b->n, nacRing);
  else            x = p_Copy(a->z,nacRing);
  if (a->n!=NULL) y = pp_Mult_qq(b->z, a->n,nacRing);
  else            y = p_Copy(b->z,nacRing);
  lu->z = p_Add_q(x, y,nacRing);
  if (lu->z==NULL)
  {
    omFreeBin((ADDRESS)lu,  rnumber_bin);
    return (number)NULL;
  }
  if (a->n!=NULL)
  {
    if (b->n!=NULL) x = pp_Mult_qq(a->n, b->n,nacRing);
    else            x = p_Copy(a->n,nacRing);
  }
  else
  {
    if (b->n!=NULL) x = p_Copy(b->n,nacRing);
    else            x = NULL;
  }
  lu->n = x;
  lu->s = FALSE;
  if (lu->n!=NULL)
  {
     number luu=(number)lu;
     naNormalize_0(luu);
     lu=(lnumber)luu;
  }
  naTest((number)lu);
  return (number)lu;
}

/// return la -lb
number  naSub(number la, number lb)               /* lu := la-lb */
{
  number llb=naCopy(lb);
  llb=naNeg(llb);
  number res=naAdd(la,llb);
  naDelete(&llb,currRing);
  return res;
}
