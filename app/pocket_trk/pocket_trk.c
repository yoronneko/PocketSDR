//
//  Pocket SDR C AP - GNSS Signal Tracking
//
//  Author:
//  T.TAKASU
//
//  History:
//  2022-07-05  1.0  port pocket_trk.py to C.
//  2022-08-08  1.1  add option -w, -d
//  2023-12-15  1.2  support multi-threading
//
#include "pocket_sdr.h"

// constants --------------------------------------------------------------------
#define SP_CORR    0.5        // default correlator spacing (chip) 
#define T_CYC      1e-3       // data read cycle (s)
#define LOG_CYC    1000       // receiver channel log cycle (* T_CYC)
#define TH_CYC     10         // receiver channel thread cycle (ms)
#define MIN_LOCK   2.0        // min lock time to print channel status (s)
#define MAX_BUFF   1000       // max number of IF data buffer (20 * n)
#define MAX_DOP    5000.0     // default max Doppler for acquisition (Hz) 
#define ESC_CLS    "\033[H\033[2J" // ANSI escape erase screen
#define ESC_COL    "\033[34m" // ANSI escape color blue
#define ESC_RES    "\033[0m"  // ANSI escape reset
#define FFTW_WISDOM "../python/fftw_wisdom.txt"

// type definitions -------------------------------------------------------------
struct rcv_tag;

typedef struct {              // receiver channel type
    sdr_ch_t *ch;             // SDR receiver channel
    int64_t ix;               // IF buffer read pointer (cyc)
    struct rcv_tag *rcv;      // pointer to receiver
    pthread_t thread;         // receiver channel thread
} rcv_ch_t;

typedef struct rcv_tag {      // receiver type
    int nch;                  // number of receiver channels
    int ich;                  // signal search channel index
    rcv_ch_t *ch[SDR_MAX_NCH]; // receiver channels
    int64_t ix;               // IF buffer write pointer (cyc)
    sdr_cpx_t *buff;          // IF buffer
    int N, len_buff;          // cycle and total length of IF buffer (bytes)
    FILE *fp;                 // data file pointer
    int fmt;                  // data format (0:packed,1:int8,2:int8+int8)
    int8_t *raw;              // data buffer
} rcv_t;

// IF buffer usage -------------------------------------------------------------
static double buff_usage(rcv_t *rcv)
{
    int64_t max_nx = 0;
    for (int i = 0; i < rcv->nch; i++) {
        int64_t nx = rcv->ix + 1 - rcv->ch[i]->ix;
        if (nx > max_nx) max_nx = nx;
    }
    return (double)max_nx / MAX_BUFF;
}

// C/N0 bar --------------------------------------------------------------------
static void cn0_bar(float cn0, char *bar)
{
    int n = (int)((cn0 - 30.0) / 1.5);
    bar[0] = '\0';
    for (int i = 0; i < n && i < 13; i++) {
        sprintf(bar + i, "|");
    }
}

// channel sync status ---------------------------------------------------------
static void sync_stat(sdr_ch_t *ch, char *stat)
{
    sprintf(stat, "%s%s%s%s", (ch->trk->sec_sync > 0) ? "S" : "-",
        (ch->nav->ssync > 0) ? "B" : "-", (ch->nav->fsync > 0) ? "F" : "-",
        (ch->nav->rev) ? "R" : "-");
}

// print receiver status header ------------------------------------------------
static void print_head(rcv_t *rcv)
{
    int nch = 0;
    
    for (int i = 0; i < rcv->nch; i++) {
        if (!strcmp(rcv->ch[i]->ch->state, "LOCK")) nch++;
    }
    printf("%s TIME(s):%10.2f%60sBUFF:%4.0f%%  LOCK:%3d/%3d\n", ESC_CLS,
        rcv->ix * T_CYC, "", buff_usage(rcv) * 100.0, nch, rcv->nch);
    printf("%3s %5s %3s %5s %8s %4s %-12s %11s %7s %11s %4s %5s %4s %4s %3s\n",
        "CH", "SIG", "PRN", "STATE", "LOCK(s)", "C/N0", "(dB-Hz)",
        "COFF(ms)", "DOP(Hz)", "ADR(cyc)", "SYNC", "#NAV", "#ERR", "#LOL",
        "NER");
}

// print receiver channel status -----------------------------------------------
static void print_ch_stat(sdr_ch_t *ch)
{
    char bar[16], stat[16];
    cn0_bar(ch->cn0, bar);
    sync_stat(ch, stat);
    printf("%s%3d %5s %3d %5s %8.2f %4.1f %-13s%11.7f %7.1f %11.1f %s %5d %4d %4d %3d%s\n",
        ESC_COL, ch->no, ch->sig, ch->prn, ch->state, ch->lock * ch->T, ch->cn0,
        bar, ch->coff * 1e3, ch->fd, ch->adr, stat, ch->nav->count[0],
        ch->nav->count[1], ch->lost, ch->nav->nerr, ESC_RES);
}

// print receiver status -------------------------------------------------------
static void rcv_print_stat(rcv_t *rcv)
{
    print_head(rcv);
    for (int i = 0; i < rcv->nch; i++) {
        sdr_ch_t *ch = rcv->ch[i]->ch;
        if (!strcmp(ch->state, "LOCK") && ch->lock * ch->T >= MIN_LOCK) {
            print_ch_stat(ch);
        }
    }
    fflush(stdout);
}

// output log $TIME ------------------------------------------------------------
static void out_log_time(double time)
{
    double t[6] = {0};
    
    sdr_get_time(t);
    sdr_log(3, "$TIME,%.3f,%.0f,%.0f,%.0f,%.0f,%.0f,%.6f,UTC", time, t[0], t[1],
       t[2], t[3], t[4], t[5]);
}

// output log $CH --------------------------------------------------------------
static void out_log_ch(sdr_ch_t *ch)
{
    sdr_log(3, "$CH,%.3f,%s,%d,%d,%.1f,%.9f,%.3f,%.3f,%d,%d", ch->time, ch->sig,
        ch->prn, ch->lock, ch->cn0, ch->coff * 1e3, ch->fd, ch->adr,
        ch->nav->count[0], ch->nav->count[1]);
}

// show usage ------------------------------------------------------------------
static void show_usage(void)
{
    printf("Usage: pocket_trk [-sig sig] [-prn prn[,...]] [-sig ... -prn ... ...]\n");
    printf("       [-toff toff] [-f freq] [-fi freq] [-d freq[,freq]] [-IQ]\n");
    printf("       [-ti tint] [-log path] [-out path] [-q] [file]\n");
    exit(0);
}

// receiver channel thread -----------------------------------------------------
static void *rcv_ch_thread(void *arg)
{
    rcv_ch_t *ch = (rcv_ch_t *)arg;
    int n = ch->ch->N / ch->rcv->N;
    int64_t ix = 0;
    
    while (*ch->ch->state) {
        for ( ; ix + 2 * n <= ch->rcv->ix + 1; ix += n) {
            
            // update SDR receiver channel
            sdr_ch_update(ch->ch, ix * T_CYC, ch->rcv->buff, ch->rcv->len_buff,
                ch->rcv->N * (ix % MAX_BUFF));
            
            if (!strcmp(ch->ch->state, "LOCK") && ix % LOG_CYC == 0) {
                out_log_ch(ch->ch);
            }
            ch->ix = ix;
        }
        sdr_sleep_msec(TH_CYC);
    }
    return NULL;
}

// new receiver channel --------------------------------------------------------
static rcv_ch_t *rcv_ch_new(const char *sig, int prn, double fs, double fi,
    const double *dop, rcv_t *rcv)
{
    rcv_ch_t *ch = (rcv_ch_t *)sdr_malloc(sizeof(rcv_ch_t));
    int sp = SP_CORR;
    
    if (!(ch->ch = sdr_ch_new(sig, prn, fs, fi, sp, 0, dop[0], dop[1], ""))) {
        sdr_free(ch);
        return NULL;
    }
    ch->rcv = rcv;
    if (pthread_create(&ch->thread, NULL, rcv_ch_thread, ch)) {
        sdr_ch_free(ch->ch);
        sdr_free(ch);
        return NULL;
    }
    return ch;
}

// free receiver channel -------------------------------------------------------
static void rcv_ch_free(rcv_ch_t *ch)
{
    ch->ch->state = "";
    pthread_join(ch->thread, NULL);
    sdr_ch_free(ch->ch);
    sdr_free(ch);
}

// new receiver ----------------------------------------------------------------
static rcv_t *rcv_new(char **sigs, const int *prns, const double *fis, int n,
    double fs, const double *dop, FILE *fp, int fmt)
{
    rcv_t *rcv = (rcv_t *)sdr_malloc(sizeof(rcv_t));
    
    rcv->ich = -1;
    rcv->N = (int)(T_CYC * fs);
    rcv->len_buff = rcv->N * MAX_BUFF;
    rcv->buff = sdr_cpx_malloc(rcv->len_buff);
    rcv->fp = fp;
    rcv->fmt = fmt;
    rcv->raw = (int8_t *)sdr_malloc(rcv->N * (fmt == 2 ? 2 : 1));
    for (int i = 0; i < n && rcv->nch < SDR_MAX_NCH; i++) {
        if ((rcv->ch[rcv->nch] = rcv_ch_new(sigs[i], prns[i], fs, fis[i], dop,
                rcv))) {
            rcv->ch[rcv->nch]->ch->no = rcv->nch + 1;
            rcv->nch++;
        }
        else {
            fprintf(stderr, "signal / prn error: %s / %d\n", sigs[i], prns[i]);
        }
    }
    return rcv;
}

// free receiver ---------------------------------------------------------------
static void rcv_free(rcv_t *rcv)
{
    for (int i = 0; i < rcv->nch; i++) {
        rcv_ch_free(rcv->ch[i]);
    }
    sdr_cpx_free(rcv->buff);
    sdr_free(rcv->raw);
    sdr_free(rcv);
}

// read IF data ----------------------------------------------------------------
static int rcv_read_data(rcv_t *rcv, int64_t ix)
{
    int i = rcv->N * (ix % MAX_BUFF);
    
    if (fread(rcv->raw, rcv->N * (rcv->fmt == 2 ? 2 : 1), 1, rcv->fp) < 1) {
        return 0;
    }
    if (rcv->fmt == 1) { // I (int8)
        for (int j = 0; j < rcv->N; i++, j++) {
            rcv->buff[i][0] = rcv->raw[j];
            rcv->buff[i][1] = 0.0;
        }
    }
    else if (rcv->fmt == 2) { // I+Q (interleaved int8)
        for (int j = 0; j < rcv->N; i++, j++) {
            rcv->buff[i][0] =  rcv->raw[j*2  ];
            rcv->buff[i][1] = -rcv->raw[j*2+1];
        }
    }
    else { // I1+Q1+I2+Q2 (packed 2bit * 4)
        ; // to be implemented
    }
    rcv->ix = ix; // IF buffer write pointer
    return 1;
}

// update signal search channel ------------------------------------------------
static void rcv_update_srch(rcv_t *rcv)
{
    int i = rcv->ich;
    
    if (i >= 0 && !strcmp(rcv->ch[i]->ch->state, "SRCH")) {
        return;
    }
    for (i = (i + 1) % rcv->nch; i != rcv->ich; i = (i + 1) % rcv->nch) {
        if (strcmp(rcv->ch[i]->ch->state, "IDLE")) continue;
        rcv->ich = i;
        rcv->ch[i]->ch->state = "SRCH";
        break;
    }
}

// wait receiver channels completed --------------------------------------------
static void rcv_wait(rcv_t *rcv, int64_t ix)
{
    for (int i = 0; i < rcv->nch; i++) {
        while (rcv->ch[i]->ix < ix) {
            sdr_sleep_msec(TH_CYC);
        }
    }
}

//------------------------------------------------------------------------------
//
//   Synopsis
//
//     pocket_trk [-sig sig] [-prn prn[,...]] [-sig ... -prn ... ...]
//         [-toff toff] [-f freq] [-fi freq] [-d freq[,freq]] [-IQ] [-ti tint]
//         [-log path] [-out path] [-q] [file]
//
//   Description
//
//     It tracks GNSS signals in digital IF data and decode navigation data in
//     the signals.
//     If single PRN number by -prn option, it plots correlation power and
//     correlation shape of the specified GNSS signal. If multiple PRN numbers
//     specified by -prn option, it plots C/N0 for each PRN.
//
//   Options ([]: default)
//
//     -sig sig
//         GNSS signal type ID (L1CA, L2CM, ...). Refer pocket_acq.py manual for
//         details. [L1CA]
//
//     -prn prn[,...]
//         PRN numbers of the GNSS signal separated by ','. A PRN number can be a
//         PRN number range like 1-32 with start and end PRN numbers. For GLONASS
//         FDMA signals (G1CA, G2CA), the PRN number is treated as FCN (frequency
//         channel number).
//
//     -toff toff
//         Time offset from the start of digital IF data in s. [0.0]
//
//     -f freq
//         Sampling frequency of digital IF data in MHz. [12.0]
//
//     -fi freq
//         IF frequency of digital IF data in MHz. The IF frequency is equal 0,
//         the IF data is treated as IQ-sampling without -IQ option (zero-IF).
//         [0.0]
//
//     -d freq[,freq]
//         Reference and max Doppler frequency to search the signal in Hz.
//         [0.0,5000.0]
//
//     -IQ
//         IQ-sampling even if the IF frequency is not equal 0.
//
//     -ti tint
//         Update interval of signal tracking status, plot and log in s. [0.05]
//
//     -w file
//         Specify FFTW wisdowm file. [../python/fftw_wisdom.txt]
//
//     -log path
//         Log stream path to write signal tracking status. The log includes
//         decoded navigation data and code offset, including navigation data
//         decoded. The stream path should be one of the followings.
//         (1) local file  file path without ':'. The file path can be contain
//             time keywords (%Y, %m, %d, %h, %M) as same as RTKLIB stream.
//         (2) TCP server  :port
//         (3) TCP client  address:port
//
//     -out path
//         Output stream path to write special messages. Currently only UBX-RXM-
//         QZSSL6 message is supported as a special message,
//
//     -q
//         Suppress showing signal tracking status.
//
//     [file]
//         File path of the input digital IF data. The format should be a series of
//         int8_t (signed byte) for real-sampling (I-sampling) or interleaved int8_t
//         for complex-sampling (IQ-sampling). PocketSDR and AP pocket_dump can be
//         used to capture such digital IF data. If the option omitted, the input
//         is taken from stdin.
//
int main(int argc, char **argv)
{
    FILE *fp = stdin;
    rcv_t *rcv;
    int prns[SDR_MAX_NCH], fmt = 1, log_lvl = 4, quiet = 0, nch = 0, ixs = 0;
    double fs = 12e6, fi = 0.0, toff = 0.0, tint = 0.1, dop[2] = {0.0, MAX_DOP};
    double fis[SDR_MAX_NCH] = {0};
    char *sig = "L1CA", *file = "", *log_file = "", *fftw_wisdom = FFTW_WISDOM;
    char *sigs[SDR_MAX_NCH];
    
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-sig") && i + 1 < argc) {
            sig = argv[++i];
        }
        else if (!strcmp(argv[i], "-prn") && i + 1 < argc) {
            int nums[SDR_MAX_NCH];
            int n = sdr_parse_nums(argv[++i], nums);
            for (int j = 0; j < n && nch < SDR_MAX_NCH; j++) {
                fis[nch] = fi;
                sigs[nch] = sig;
                prns[nch++] = nums[j];
            }
        }
        else if (!strcmp(argv[i], "-toff") && i + 1 < argc) {
            toff = atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
            fs = atof(argv[++i]) * 1e6;
        }
        else if (!strcmp(argv[i], "-fi") && i + 1 < argc) {
            fi = atof(argv[++i]) * 1e6;
        }
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) {
            sscanf(argv[++i], "%lf,%lf", dop, dop + 1);
        }
        else if (!strcmp(argv[i], "-IQ")) {
            fmt = 2;
        }
        else if (!strcmp(argv[i], "-ti") && i + 1 < argc) {
            tint = atof(argv[++i]);
        }
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) {
            fftw_wisdom = argv[++i];
        }
        else if (!strcmp(argv[i], "-log") && i + 1 < argc) {
            log_file = argv[++i];
        }
        else if (!strcmp(argv[i], "-q")) {
            quiet = 1;
        }
        else if (argv[i][0] == '-') {
            show_usage();
        }
        else {
            file = argv[i];
        }
    }
    fmt = (fmt == 1 && fi > 0.0) ? 1 : 2;
    
    if (*file) {
        if (!(fp = fopen(file, "rb"))) {
            fprintf(stderr, "file open error: %s\n", file);
            exit(-1);
        }
        fseek(fp, (long)(toff * fs * (fmt == 2 ? 2 : 1)), SEEK_SET);
    }
    sdr_func_init(fftw_wisdom);
    
    if (*log_file) {
        sdr_log_open(log_file);
        sdr_log_level(log_lvl);
    }
    // new receiver
    rcv = rcv_new(sigs, prns, fis, nch, fs, dop, fp, fmt);
    
    //if (*file) {
    //    for (int i = 0; i < rcv->nch; i++) {
    //        rcv->ch[i]->ch->state = "SRCH";
    //    }
    //}
    uint32_t tt = sdr_get_tick();
    sdr_log(3, "$LOG,%.3f,%s,%d,START FILE=%s FS=%.3f FMT=%d", 0.0, "", 0, file,
        fs * 1e-6, fmt);
    
    for (int64_t ix = 0; ; ix++) {
        // output log $TIME
        if (ix % LOG_CYC == 0) {
            out_log_time(ix * T_CYC);
        }
        // read IF data
        if (!rcv_read_data(rcv, ix)) break;
        
        // update signal search channel
        rcv_update_srch(rcv);
        
        // print receiver status
        if (!quiet && ix % (int)(tint / T_CYC) == 0) {
            rcv_print_stat(rcv);
        }
        // wait receiver channel completed
        //if (*file) {
        //    rcv_wait(rcv, ix - 1000);
        //}
    }
    tt = sdr_get_tick() - tt;
    sdr_log(3, "$LOG,%.3f,%s,%d,END FILE=%s", tt * 1e-3, "", 0, file);
    if (!quiet) printf("  TIME(s) = %.3f\n", tt * 1e-3);
    rcv_free(rcv);
    fclose(fp);
    sdr_log_close();
    return 0;
}
