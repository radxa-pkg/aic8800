#ifndef LOWCFE_V3_H
#define LOWCFE_V3_H
typedef float        Float;

#define Fs8k16k
// #define Fs48k

#define REMOVESQRT

#ifdef  Fs8k16k
#define PITCH_MAX    240                             // 8k/16k, maximum allowed pitch, 66 Hz = 15ms * 16k = 240
#endif
#ifdef  Fs48k
#define PITCH_MAX	 720                             // 48k,    maximum allowed pitch, 66 Hz = 15ms * 48k = 720
#endif

#define	POVERLAP_MAX (PITCH_MAX >> 2)                // maximum pitch OLA window
#define	HISTLEN_MAX	 (PITCH_MAX * 3 + POVERLAP_MAX)  // history buffer length
#define	EOVERLAPINCR 32                              // end OLA increment per frame, 4ms
#define	ATTENFAC	 ((Float).2)                     // attenuation factor per frame

typedef struct _LowcFE_c
{
    int    pitchmin;
    int    pitchmax;    
    int    fs;
    int    histlen;
    int    corrlen;
    int    corrlenbuflen;
    int    plcdelay;
    int    framesize;
    int    erasecnt;                                 // consecutive erased frames
    int    poverlap;                                 // overlap based on pitch
    int    poffset;                                  // offset into pitch period
    int    pitch;                                    // pitch estimate
    int    pitchblen;                                // current pitch buffer length
    Float* pitchbufend;                              // end of pitch buffer
    Float* pitchbufstart;                            // start of pitch buffer
    Float  pitchbuf[HISTLEN_MAX];                    // buffer for cycles of speech
    Float  lastq[POVERLAP_MAX];                      // saved last quarter wavelengh
    short  history[HISTLEN_MAX];                     // history buffer
} LowcFE_c;

void g711plc_construct(LowcFE_c * lc, int pfs, Float msec);
void plc              (short* audioframe, LowcFE_c* plc, int frameLoss, int enablePLC); // funuction interface

#if 0
/* example */
/* =========== Initialization ============== */
/* LowcFE_c lc;
/* g711plc_construct(lc, 8)
/* =========== run each frame ============== */
/* input is audioframe, output is also saved in audioframe */
/* if packet loss 
/*   plc(audioframe, &lc, 1, 1);
/* else
/*   plc(audioframe, &lc, 0, 1);
*/
#endif
#endif