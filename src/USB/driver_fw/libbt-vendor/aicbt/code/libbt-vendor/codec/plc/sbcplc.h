/********************************************************
SBC Example PLC ANSI-C Source Code
This is copy from HFP spec, just only for study and demo.
Please don't use in commercial product.
File: sbcplc.h
*****************************************************************************/
#ifndef SBCPLC_H
#define SBCPLC_H
#define FS 120 /* Frame Size */
#define N 256 /* 16ms - Window Length for pattern matching */
#define M 64 /* 4ms - Template for matching */
#define LHIST (N+FS-1) /* Length of history buffer required */
#define SBCRT 36 /* SBC Reconvergence Time (samples) */
#define OLAL 16 /* OverLap-Add Length (samples) */
/* PLC State Information */
struct PLC_State
{
    short hist[LHIST+FS+SBCRT+OLAL];
    short bestlag;
    int nbf;
};
/* Prototypes */
void InitPLC(struct PLC_State *plc_state);
void PLC_bad_frame(struct PLC_State *plc_state, short *ZIRbuf, short *out);
void PLC_good_frame(struct PLC_State *plc_state, short *in, short *out);
#endif /* SBCPLC_H */

