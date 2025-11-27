#include <math.h>
#include "malloc.h"
#include <stdbool.h>
#include "lowcfe_v3.h"

static void g711plc_scalespeech    (LowcFE_c *, short *out);
static void g711plc_getfespeech    (LowcFE_c *, short *out, int sz);
static void g711plc_savespeech     (LowcFE_c *, short *s);
static int  g711plc_findpitch      (LowcFE_c *);
static void g711plc_overlapadd     (Float * l, Float * r, Float * o, int cnt);
static void g711plc_overlapadds    (short *l, short *r, short *o, int cnt);
static void g711plc_overlapaddatend(LowcFE_c *, short *s, short *f, int cnt);
static void g711plc_convertsf      (short *f, Float * t, int cnt);
static void g711plc_convertfs      (Float * f, short *t, int cnt);
static void g711plc_copyf          (Float * f, Float * t, int cnt);
static void g711plc_copys          (short *f, short *t, int cnt);
static void g711plc_zeros          (short *s, int cnt);
static void g711plc_dofe           (LowcFE_c*, short* s);
static void g711plc_addtohistory   (LowcFE_c*, short* s);

void g711plc_construct(LowcFE_c * lc, int pfs, Float msec)
{
  lc->fs          = pfs;
  lc->pitchmin    = pfs * 5;                // minimum allowed pitch, 200 Hz
  lc->pitchmax    = pfs * 15;               // maximum allowed pitch, 66 Hz   
  lc->histlen     = lc->pitchmax * 3 + (lc->pitchmax >> 2);
  lc->corrlen     = pfs * 20;               // 20 msec correlation length
#ifdef Fs8k16k
  lc->framesize   = (int) round(pfs * msec); // 7.5 msec frame size
#endif
#ifdef Fs48k
  lc->framesize   = (int) round(pfs * 7);   // 7.5 msec frame size
#endif
  lc->plcdelay    = lc->framesize;          // POVERLAP_MAX or lc->framesize
  lc->erasecnt    = 0;
  lc->pitchbufend = &lc->pitchbuf[lc->histlen];
  g711plc_zeros(lc->history, HISTLEN_MAX);
}

/*
 * Get samples from the circular pitch buffer. Update poffset so when subsequent frames are erased the signal continues.
 */
static void g711plc_getfespeech(LowcFE_c * lc, short *out, int sz)
{
  while (sz)
  {
    int cnt = lc->pitchblen - lc->poffset;

    if (cnt > sz)
      cnt = sz;

    g711plc_convertfs(&lc->pitchbufstart[lc->poffset], out, cnt);
    
    lc->poffset += cnt;
    
    if (lc->poffset == lc->pitchblen)
      lc->poffset = 0;

    out += cnt;
    sz -= cnt;
  }
}

static void g711plc_scalespeech(LowcFE_c * lc, short *out)
{    
  Float g = (Float) 1. - (lc->erasecnt - 1) * ATTENFAC;

  for (int i = 0; i < lc->framesize; i++)
  {
    out[i] = (short) (out[i] * g);
    g -= (ATTENFAC / lc->framesize); // attenuation per sample
  }
}

/*
 * Generate the synthetic signal.
 * At the beginning of an erasure determine the pitch, and extract one pitch period from the tail of the signal. 
 * Do an OLA for 1/4 of the pitch to smooth the signal. Then repeat the extracted signal for the length of the erasure. 
 * If the erasure continues for more than 10 msec, increase the number of periods in the pitchbuffer.
 * At the end of an erasure, do an OLA with the start of the first good frame.
 * The gain decays as the erasure gets longer.
 */
static void g711plc_dofe(LowcFE_c * lc, short *out)
{
  short* tmp ;
  if (lc->erasecnt == 0)
  {    
    g711plc_convertsf(lc->history, lc->pitchbuf, lc->histlen);
    
    lc->pitch         = g711plc_findpitch(lc);

    /* save original last poverlap samples */
    lc->poverlap      = lc->pitch >> 2;    
    g711plc_copyf(lc->pitchbufend - lc->poverlap, lc->lastq, lc->poverlap); 

    /* create pitch buffer with 1 period */
    lc->poffset       = 0;            
    lc->pitchblen     = lc->pitch;
    lc->pitchbufstart = lc->pitchbufend - lc->pitchblen;
    g711plc_overlapadd(lc->lastq, lc->pitchbufstart - lc->poverlap, lc->pitchbufend - lc->poverlap, lc->poverlap);

    /* update last 1/4 wavelength in history buffer */
    g711plc_convertfs(lc->pitchbufend - lc->poverlap, &lc->history[lc->histlen - lc->poverlap], lc->poverlap);

    /* get synthesized speech */
    g711plc_getfespeech(lc, out, lc->framesize);
  }
  else if (lc->erasecnt == 1 || lc->erasecnt == 2)
  {
    /* tail of previous pitch estimate */
    tmp = (short*) malloc(sizeof(short) * lc->poverlap);

    /* save offset for OLA */
    int saveoffset = lc->poffset;

    /* continue with old pitchbuf */
    g711plc_getfespeech(lc, tmp, lc->poverlap);

    /* add periods to the pitch buffer */
    lc->poffset = saveoffset;
    while (lc->poffset > lc->pitch)
      lc->poffset -= lc->pitch;
    lc->pitchblen += lc->pitch; 
    lc->pitchbufstart = lc->pitchbufend - lc->pitchblen;

    g711plc_overlapadd(lc->lastq, lc->pitchbufstart - lc->poverlap, lc->pitchbufend - lc->poverlap, lc->poverlap);

    /* overlap add old pitchbuffer with new */
    g711plc_getfespeech(lc, out, lc->framesize);
    g711plc_overlapadds(tmp, out, out, lc->poverlap);
    g711plc_scalespeech(lc, out);

    free(tmp);
  }
  else if (lc->erasecnt > 5)
  {
    g711plc_zeros(out, lc->framesize);
  }
  else
  {
    g711plc_getfespeech(lc, out, lc->framesize);
    g711plc_scalespeech(lc, out);
  }

  lc->erasecnt++;

  g711plc_savespeech(lc, out);
}

/*
 * Save a frames worth of new speech in the history buffer.
 * Return the output speech delayed by PLCDELAY.
 */
static void g711plc_savespeech(LowcFE_c * lc, short *s)
{
  /* make room for new signal */
  g711plc_copys(&lc->history[lc->framesize], lc->history, lc->histlen - lc->framesize);

  /* copy in the new frame */
  g711plc_copys(s, &lc->history[lc->histlen - lc->framesize], lc->framesize);

  /* copy out the delayed frame */
  g711plc_copys(&lc->history[lc->histlen - lc->framesize - lc->plcdelay], s, lc->framesize);
}

/*
 * A good frame was received and decoded.
 * If right after an erasure, do an overlap add with the synthetic signal.
 * Add the frame to history buffer.
 */
static void g711plc_addtohistory(LowcFE_c * lc, short *s)
{
  short* overlapbuf;
  int    olen ;
  if (lc->erasecnt)
  {
    overlapbuf = (short *) malloc(sizeof(short) * lc->framesize);
    olen       = lc->poverlap + (lc->erasecnt - 1) * EOVERLAPINCR;

    if (olen > lc->framesize)
      olen = lc->framesize;

    g711plc_getfespeech(lc, overlapbuf, olen);
    g711plc_overlapaddatend(lc, s, overlapbuf, olen);
    
    lc->erasecnt = 0;
    
    free(overlapbuf);
  }

  g711plc_savespeech(lc, s);
}

/*
 * Overlapp add the end of the erasure with the start of the first good frame
 * Scale the synthetic speech by the gain factor before the OLA.
 */
static void g711plc_overlapaddatend(LowcFE_c * lc, short *s, short *f, int cnt)
{
  int i;
  Float incrg;
  Float lw, rw;
  Float t;
  Float incr = (Float) 1. / cnt;
  Float gain = (Float) 1. - (lc->erasecnt - 1) * ATTENFAC;
  if (gain < 0.)
    gain = (Float) 0.;
  incrg = incr * gain;
  lw = ((Float) 1. - incr) * gain;
  rw = incr;
  for (i = 0; i < cnt; i++)
  {
    t = lw * f[i] + rw * s[i];
    if (t > 32767.)
      t = (Float) 32767.;
    else if (t < -32768.)
      t = (Float) - 32768.;
    s[i] = (short) t;
    lw -= incrg;
    rw += incr;
  }
}

/*
 * Overlapp add left and right sides
 */
static void g711plc_overlapadd(Float * l, Float * r, Float * o, int cnt) 
{
  int i;
  Float incr, lw, rw, t;

  if (cnt == 0)
    return;
  incr = (Float) 1. / cnt;
  lw = (Float) 1. - incr;
  rw = incr;
  for (i = 0; i < cnt; i++) {
    t = lw * l[i] + rw * r[i];
    if (t > (Float) 32767.)
      t = (Float) 32767.;
    else if (t < (Float) - 32768.)
      t = (Float) - 32768.;
    o[i] = t;
    lw -= incr;
    rw += incr;
  }
}

/*
 * Overlapp add left and right sides
 */
static void g711plc_overlapadds(short *l, short *r, short *o, int cnt)
{
  int i;
  Float incr, lw, rw, t;

  if (cnt == 0)
    return;
  incr = (Float) 1. / cnt;
  lw = (Float) 1. - incr;
  rw = incr;
  for (i = 0; i < cnt; i++) {
    t = lw * l[i] + rw * r[i];
    if (t > (Float) 32767.)
      t = (Float) 32767.;
    else if (t < (Float) - 32768.)
      t = (Float) - 32768.;
    o[i] = (short) t;
    lw -= incr;
    rw += incr;
  }
}

/*
 * Estimate the pitch.
 * l - pointer to first sample in last 20 msec of speech.
 * r - points to the sample PITCH_MAX before l
 */
static int g711plc_findpitch(LowcFE_c * lc)
{
  int   NDEC            = 2;
  Float	CORRMINPOWER    = 250.0;



  int   i, j, k;
  int   bestmatch;  
  Float bestcorr;
  Float corr;      /* correlation */
  Float energy;    /* running energy for mov sig*/ 
  Float energyLast;
  Float scale;     /* scale correlation by average power */
  Float *rp;       /* segment to match */
  int   PITCHDIFF  = lc->pitchmax - lc->pitchmin;
  int   varCorrLen = lc->corrlen;
  Float *l         = lc->pitchbufend - varCorrLen;
  Float *r         = lc->pitchbufend - (varCorrLen + lc->pitchmax);
  Float *lnorm     = NULL;
  Float tmp        = (Float) 0.;

  if (lc->fs == 8)
    NDEC = 2;
  else if (lc->fs == 16)
    NDEC = 4;
  else if (lc->fs == 48)
    NDEC = 12;
  rp               = r;
  energy           = (Float) 0.;
  corr             = (Float) 0.;
  energyLast       = (Float) 0.;

#ifndef REMOVESQRT
  /* coarse search */
  for (i = 0; i < varCorrLen; i += NDEC)
  {
    energy += rp[i] * rp[i];
    corr   += rp[i] * l[i];
  }
  
  scale     = energy;
  if (scale < CORRMINPOWER)
    scale   = CORRMINPOWER;
  
  corr      = corr / (Float) sqrt (scale);
  bestcorr  = corr;
  bestmatch = 0;
 
  for (j = NDEC; j <= PITCHDIFF; j += NDEC)
  {
    energy -= rp[0] * rp[0];
    energy += rp[varCorrLen] * rp[varCorrLen];
    rp     += NDEC;

    corr    = 0.f;
    for (i = 0; i < varCorrLen; i += NDEC)
      corr += rp[i] * l[i];

    scale   = energy;
    if (scale < CORRMINPOWER)
      scale = CORRMINPOWER;
    corr   /= (Float) sqrt (scale);

    if (corr >= bestcorr)
    {
      bestcorr  = corr;
      bestmatch = j;
    }
  }

  /* fine search */
  j = bestmatch - (NDEC - 1);
  if (j < 0)
    j = 0;

  k = bestmatch + (NDEC - 1);
  if (k > PITCHDIFF)
    k = PITCHDIFF;

  rp     = &r[j];
  energy = 0.f;
  corr   = 0.f;

  for (i = 0; i < varCorrLen; i++)
  {
    energy += rp[i] * rp[i];
    corr   += rp[i] * l[i];
  }

  scale     = energy;
  if (scale < CORRMINPOWER)
    scale   = CORRMINPOWER;

  corr      = corr / (Float) sqrt (scale);
  bestcorr  = corr;
  bestmatch = j;

  for (j++; j <= k; j++)
  {
    energy -= rp[0] * rp[0];
    energy += rp[varCorrLen] * rp[varCorrLen];
    rp++;

    corr    = 0.f;
    for (i = 0; i < varCorrLen; i++)
      corr += rp[i] * l[i];

    scale   = energy;
    if (scale < CORRMINPOWER)
      scale = CORRMINPOWER;

    corr = corr / (Float) sqrt (scale);

    if (corr > bestcorr)
    {
      bestcorr  = corr;
      bestmatch = j;
    }
  }
#else

  /* coarse search */  
  lnorm     = (Float*) malloc(sizeof(Float) * varCorrLen);

  for (i = 0; i < varCorrLen; i += NDEC)
    energyLast += (l[i] > 0 ? l[i] : -l[i]);

  scale     = energyLast;
  if (scale < CORRMINPOWER)
    scale   = CORRMINPOWER;
  scale     = (Float) 1.0 / scale;

  for (i = 0; i < varCorrLen; i += NDEC)
    lnorm[i] = l[i] * scale;

  for (i = 0; i < varCorrLen; i += NDEC)
    energy += (rp[i] > 0 ? rp[i] : -rp[i]);
  
  scale     = energy;
  if (scale < CORRMINPOWER)
    scale   = CORRMINPOWER;
  scale     = (Float) 1.0 / scale;

  for (i = 0; i < varCorrLen; i += NDEC)
  {
      tmp   = lnorm[i] - rp[i] * scale;
      corr += (tmp > 0 ? tmp : -tmp);
  }

  bestcorr  = corr;
  bestmatch = 0;
 
  for (j = NDEC; j <= PITCHDIFF; j += NDEC)
  {
    energy   -= (rp[0] > 0 ? rp[0] : -rp[0]);
    energy   += (rp[varCorrLen] > 0 ? rp[varCorrLen] : -rp[varCorrLen]);
    rp       += NDEC;

    scale     = energy;
    if (scale < CORRMINPOWER)
      scale   = CORRMINPOWER;
    scale     = (Float) 1.0 / scale;

    corr      = 0.f;
    for (i = 0; i < varCorrLen; i += NDEC)
    {
        tmp   = lnorm[i] - rp[i] * scale;
        corr += (tmp > 0 ? tmp : -tmp);
    }
  
    if (corr <= bestcorr)
    {
      bestcorr  = corr;
      bestmatch = j;
    }
  }

  /* fine search */
  j = bestmatch - (NDEC - 1);
  if (j < 0)
    j = 0;

  k = bestmatch + (NDEC - 1);
  if (k > PITCHDIFF)
    k = PITCHDIFF;

  rp            = &r[j];
  energy        = 0.f;
  corr          = 0.f;
  energyLast    = 0.f;

  for (i = 0; i < varCorrLen; i++)    
    energyLast += (l[i] > 0 ? l[i] : -l[i]);

  scale         = energyLast;
  if (scale< CORRMINPOWER)
    scale       = CORRMINPOWER;
  scale         = (Float) 1.0 / scale;

  for (i = 0; i < varCorrLen; i++)
    lnorm[i]    = l[i] * scale;

  for (i = 0; i < varCorrLen; i++)    
    energy     += (rp[i] > 0 ? rp[i] : -rp[i]);
  
  scale         = energy;
  if (scale < CORRMINPOWER)
    scale       = CORRMINPOWER;
  scale         = (Float) 1.0 / scale;

  for (i = 0; i < varCorrLen; i++)
  {
     tmp        = lnorm[i] - rp[i] * scale;
     corr      += (tmp > 0 ? tmp : -tmp);
  }

  bestcorr      = corr;
  bestmatch     = j;

  for (j++; j <= k; j++)
  {    
    energy     -= (rp[0] > 0 ? rp[0] : -rp[0]);
    energy     += (rp[varCorrLen] > 0 ? rp[varCorrLen] : -rp[varCorrLen]);
    rp++;

    scale       = energy;
    if (scale < CORRMINPOWER)
      scale     = CORRMINPOWER;
    scale       = (Float) 1.0 / scale;

    corr        = 0.f;
    for (i = 0; i < varCorrLen; i++)
    {
       tmp      = lnorm[i] - rp[i] * scale;
       corr    += (tmp > 0 ? tmp : -tmp);
    }

    if (corr < bestcorr)
    {
      bestcorr  = corr;
      bestmatch = j;
    }
  }

  free(lnorm);
#endif
  return lc->pitchmax - bestmatch;
}

static void g711plc_convertsf(short *f, Float * t, int cnt)
{  
  int i ;
  for (i = 0; i < cnt; i++)
    t[i] = (Float) f[i];
}

static void g711plc_convertfs(Float * f, short *t, int cnt)
{
  int i ;
  for ( i = 0; i < cnt; i++)
    t[i] = (short) f[i];
}

static void g711plc_copyf(Float * f, Float * t, int cnt)
{
  int i;
  for (i = 0; i < cnt; i++)
    t[i] = f[i];
}

static void g711plc_copys(short *f, short *t, int cnt)
{
  int i ;
  for ( i = 0; i < cnt; i++)
    t[i] = f[i];
}

static void g711plc_zeros(short *s, int cnt)
{
  int i ;
  for ( i = 0; i < cnt; i++)
    s[i] = 0;
}

void plc(short* frame, LowcFE_c* lc, int packetloss, int doPLC)
{
  int i;
  if (packetloss == 1) // frame loss
  {
    if (doPLC) // with plc
    {            
      g711plc_dofe(lc, frame);
    }
    else // without plc
    {            
      for ( i = 0; i < lc->framesize; i++)
        frame[i] = 0;

      g711plc_addtohistory(lc, frame);
    }
  }
  else
    g711plc_addtohistory(lc, frame);
}