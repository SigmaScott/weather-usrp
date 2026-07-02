#include "dsp.h"
#include "log.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void design_lpf(float *taps, int num_taps, double cutoff)
{
    int M = num_taps - 1;
    double sum = 0.0;

    for (int i = 0; i < num_taps; i++) {
        double n = i - M / 2.0;
        /* Windowed sinc */
        double h;
        if (fabs(n) < 1e-10)
            h = 2.0 * cutoff;
        else
            h = sin(2.0 * M_PI * cutoff * n) / (M_PI * n);

        /* Blackman window */
        double w = 0.42 - 0.5 * cos(2.0 * M_PI * i / M)
                        + 0.08 * cos(4.0 * M_PI * i / M);
        taps[i] = (float)(h * w);
        sum += taps[i];
    }

    /* Normalize for unity DC gain */
    for (int i = 0; i < num_taps; i++)
        taps[i] /= (float)sum;
}

void fir_chan_init(fir_chan_t *fir, double freq_offset,
                   double sample_rate, double bandwidth)
{
    memset(fir, 0, sizeof(*fir));
    fir->decim_factor = (int)(sample_rate / CHANNEL_AUDIO_RATE);
    fir->decim_count = 0;

    /*
     * Design LPF prototype then frequency-shift by multiplying
     * each tap by complex exponential: tap[n] * exp(j*2*pi*f_offset*n/fs)
     * This bakes the frequency translation into the filter.
     */
    float proto[FIR_TAPS];
    double norm_bw = bandwidth / sample_rate;
    design_lpf(proto, FIR_TAPS, norm_bw / 2.0);

    double phase_inc = 2.0 * M_PI * freq_offset / sample_rate;
    for (int i = 0; i < FIR_TAPS; i++) {
        double phase = phase_inc * i;
        fir->taps_i[i] = proto[i] * (float)cos(phase);
        fir->taps_q[i] = proto[i] * (float)sin(phase);
    }
}

int fir_chan_process(fir_chan_t *fir, iq_sample_t in, iq_sample_t *out)
{
    fir->history[fir->hist_idx] = in;
    fir->hist_idx = (fir->hist_idx + 1) % FIR_TAPS;

    if (++fir->decim_count < fir->decim_factor)
        return 0;
    fir->decim_count = 0;

    /*
     * Compute complex dot product: sum(history[n] * conj(taps[n]))
     * Since taps encode the frequency shift, this simultaneously
     * translates the channel to baseband and low-pass filters it.
     */
    float acc_i = 0.0f, acc_q = 0.0f;
    int idx = fir->hist_idx;
    for (int i = 0; i < FIR_TAPS; i++) {
        int k = (idx + i) % FIR_TAPS;
        float si = fir->history[k].i;
        float sq = fir->history[k].q;
        float ti = fir->taps_i[i];
        float tq = fir->taps_q[i];
        /* Complex multiply: (si+j*sq) * (ti-j*tq) */
        acc_i += si * ti + sq * tq;
        acc_q += sq * ti - si * tq;
    }

    out->i = acc_i;
    out->q = acc_q;
    return 1;
}

void fm_demod_init(fm_demod_t *fm)
{
    memset(fm, 0, sizeof(*fm));
}

float fm_demod_process(fm_demod_t *fm, iq_sample_t in)
{
    /*
     * Polar discriminator: instantaneous frequency is the phase
     * difference between successive samples.
     * f = atan2(Im(in * conj(prev)), Re(in * conj(prev))) / pi
     */
    float di = in.i * fm->prev.i + in.q * fm->prev.q;
    float dq = in.q * fm->prev.i - in.i * fm->prev.q;
    fm->prev = in;

    return atan2f(dq, di) / (float)M_PI;
}

void decimator_init(decimator_t *dec)
{
    memset(dec, 0, sizeof(*dec));
    dec->decim_factor = DECIMATION_AUDIO;
    design_lpf(dec->taps, FIR_TAPS, 0.5 / DECIMATION_AUDIO);
}

int decimator_process(decimator_t *dec, float in, float *out)
{
    dec->history[dec->hist_idx] = in;
    dec->hist_idx = (dec->hist_idx + 1) % FIR_TAPS;

    if (++dec->decim_count < dec->decim_factor)
        return 0;
    dec->decim_count = 0;

    float acc = 0.0f;
    int idx = dec->hist_idx;
    for (int i = 0; i < FIR_TAPS; i++) {
        int k = (idx + i) % FIR_TAPS;
        acc += dec->history[k] * dec->taps[i];
    }

    *out = acc;
    return 1;
}
