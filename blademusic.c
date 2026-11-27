#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define SR 44100
#define DURATION 75.0
#define NUM_SAMPLES ((int)(SR * DURATION))
#define PI 3.14159265358979323846

/* ============ WAV WRITER ============ */
typedef struct {
    FILE *f;
    int channels;
    int bits;
    int sample_rate;
} WavWriter;

void wav_write_header(WavWriter *w, int total_samples) {
    int byte_rate = w->sample_rate * w->channels * (w->bits / 8);
    int block_align = w->channels * (w->bits / 8);
    int data_size = total_samples * block_align;
    int file_size = 36 + data_size;

    fwrite("RIFF", 1, 4, w->f);
    fwrite(&file_size, 4, 1, w->f);
    fwrite("WAVE", 1, 4, w->f);
    fwrite("fmt ", 1, 4, w->f);
    int fmt_size = 16;
    fwrite(&fmt_size, 4, 1, w->f);
    int16_t audio_fmt = 1;
    fwrite(&audio_fmt, 2, 1, w->f);
    int16_t ch = (int16_t)w->channels;
    fwrite(&ch, 2, 1, w->f);
    fwrite(&w->sample_rate, 4, 1, w->f);
    fwrite(&byte_rate, 4, 1, w->f);
    int16_t ba = (int16_t)block_align;
    fwrite(&ba, 2, 1, w->f);
    int16_t bps = (int16_t)w->bits;
    fwrite(&bps, 2, 1, w->f);
    fwrite("data", 1, 4, w->f);
    fwrite(&data_size, 4, 1, w->f);
}

void wav_write_samples(WavWriter *w, float *samples, int count) {
    for (int i = 0; i < count; i++) {
        float s = samples[i];
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t v = (int16_t)(s * 32767.0f);
        fwrite(&v, 2, 1, w->f);
    }
}

/* ============ OSCILLATORS ============ */
typedef struct {
    double phase;
    double freq;
} Osc;

void osc_init(Osc *o, double freq) {
    o->phase = 0.0;
    o->freq = freq;
}

double osc_saw(Osc *o) {
    double v = 2.0 * o->phase - 1.0;
    o->phase += o->freq / SR;
    if (o->phase >= 1.0) o->phase -= 1.0;
    return v;
}

double osc_sine(Osc *o) {
    double v = sin(2.0 * PI * o->phase);
    o->phase += o->freq / SR;
    if (o->phase >= 1.0) o->phase -= 1.0;
    return v;
}

/* ============ ADSR ENVELOPE ============ */
typedef struct {
    double attack, decay, sustain, release;
    double level;
    int state; /* 0=idle, 1=attack, 2=decay, 3=sustain, 4=release */
    double a_rate, d_rate, r_rate;
    int gate; /* 1=note on, 0=note off */
} ADSR;

void adsr_init(ADSR *e, double a, double d, double s, double r) {
    e->attack = a; e->decay = d; e->sustain = s; e->release = r;
    e->level = 0.0; e->state = 0; e->gate = 0;
    e->a_rate = (a > 0) ? 1.0 / (a * SR) : 1.0;
    e->d_rate = (d > 0) ? (1.0 - s) / (d * SR) : 1.0;
    e->r_rate = (r > 0) ? s / (r * SR) : 1.0;
}

void adsr_gate(ADSR *e, int on) {
    if (on && !e->gate) {
        e->gate = 1;
        e->state = 1; /* attack */
    } else if (!on && e->gate) {
        e->gate = 0;
        if (e->state > 0 && e->state < 4) e->state = 4; /* release */
    }
}

double adsr_tick(ADSR *e) {
    switch (e->state) {
        case 0: return 0.0;
        case 1:
            e->level += e->a_rate;
            if (e->level >= 1.0) { e->level = 1.0; e->state = 2; }
            break;
        case 2:
            e->level -= e->d_rate;
            if (e->level <= e->sustain) { e->level = e->sustain; e->state = 3; }
            break;
        case 3: break;
        case 4:
            e->level -= e->r_rate;
            if (e->level <= 0.0) { e->level = 0.0; e->state = 0; }
            break;
    }
    return e->level;
}

/* ============ STATE VARIABLE FILTER ============ */
typedef struct {
    double lowpass, bandpass, highpass;
    double freq, q;
    double f, k;
} SVFilter;

void svf_init(SVFilter *f, double freq, double q) {
    f->lowpass = 0; f->bandpass = 0; f->highpass = 0;
    f->freq = freq; f->q = q;
    f->f = 2.0 * sin(PI * freq / SR);
    f->k = 1.0 / q;
}

void svf_set_freq(SVFilter *f, double freq) {
    if (freq < 20) freq = 20;
    if (freq > SR * 0.45) freq = SR * 0.45;
    f->freq = freq;
    f->f = 2.0 * sin(PI * freq / SR);
}

double svf_process(SVFilter *f, double input) {
    double highpass = input - f->lowpass - f->k * f->bandpass;
    double bandpass = f->bandpass + f->f * highpass;
    double lowpass = f->lowpass + f->f * bandpass;
    f->highpass = highpass;
    f->bandpass = bandpass;
    f->lowpass = lowpass;
    return lowpass;
}

/* ============ REVERB (Schroeder) ============ */
#define NUM_COMBS 8
#define NUM_ALLPASS 4

typedef struct {
    float *buffer;
    int size, idx;
    float feedback;
} Comb;

typedef struct {
    float *buffer;
    int size, idx;
    float gain;
} Allpass;

typedef struct {
    Comb combs[NUM_COMBS];
    Allpass allpasses[NUM_ALLPASS];
} Reverb;

void comb_init(Comb *c, int size, float fb) {
    c->buffer = (float*)calloc(size, sizeof(float));
    c->size = size; c->idx = 0; c->feedback = fb;
}

float comb_process(Comb *c, float input) {
    float out = c->buffer[c->idx];
    c->buffer[c->idx] = input + out * c->feedback;
    c->idx = (c->idx + 1) % c->size;
    return out;
}

void allpass_init(Allpass *a, int size, float g) {
    a->buffer = (float*)calloc(size, sizeof(float));
    a->size = size; a->idx = 0; a->gain = g;
}

float allpass_process(Allpass *a, float input) {
    float bufout = a->buffer[a->idx];
    float output = -a->gain * input + bufout;
    a->buffer[a->idx] = input + a->gain * bufout;
    a->idx = (a->idx + 1) % a->size;
    return output;
}

void reverb_init(Reverb *r) {
    int comb_sizes[NUM_COMBS] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
    float comb_fb = 0.84;
    for (int i = 0; i < NUM_COMBS; i++)
        comb_init(&r->combs[i], comb_sizes[i], comb_fb);
    int ap_sizes[NUM_ALLPASS] = {225, 556, 441, 341};
    float ap_gain = 0.5;
    for (int i = 0; i < NUM_ALLPASS; i++)
        allpass_init(&r->allpasses[i], ap_sizes[i], ap_gain);
}

float reverb_process(Reverb *r, float input) {
    float sum = 0;
    for (int i = 0; i < NUM_COMBS; i++)
        sum += comb_process(&r->combs[i], input);
    sum /= NUM_COMBS;
    for (int i = 0; i < NUM_ALLPASS; i++)
        sum = allpass_process(&r->allpasses[i], sum);
    return sum;
}

/* ============ CHORUS ============ */
typedef struct {
    float *buffer;
    int size, idx;
    double lfo_phase, lfo_freq, depth;
} Chorus;

void chorus_init(Chorus *c, int size, double lfo_freq, double depth) {
    c->buffer = (float*)calloc(size, sizeof(float));
    c->size = size; c->idx = 0;
    c->lfo_phase = 0; c->lfo_freq = lfo_freq; c->depth = depth;
}

float chorus_process(Chorus *c, float input) {
    c->buffer[c->idx] = input;
    double lfo = sin(2.0 * PI * c->lfo_phase);
    double delay = c->depth * (0.5 + 0.5 * lfo);
    int read_idx = c->idx - (int)delay;
    if (read_idx < 0) read_idx += c->size;
    float out = c->buffer[read_idx];
    c->idx = (c->idx + 1) % c->size;
    c->lfo_phase += c->lfo_freq / SR;
    if (c->lfo_phase >= 1.0) c->lfo_phase -= 1.0;
    return out;
}

/* ============ NOTE FREQUENCIES ============ */
double note_freq(int midi) {
    return 440.0 * pow(2.0, (midi - 69) / 12.0);
}

/* ============ COMPOSITION DATA ============ */
typedef struct {
    double start_time;
    double duration;
    int notes[6];
    int num_notes;
    double filter_start;
    double filter_end;
    double volume;
} ChordEvent;

typedef struct {
    double start_time;
    double duration;
    int midi_note;
    double volume;
} MelodyNote;

typedef struct {
    double start_time;
    double duration;
    int midi_note;
    double volume;
} BassNote;

/* ============ MAIN ============ */
int main() {
    printf("Blade Runner Musical Composition Generator\n");
    printf("===========================================\n");
    printf("Rendering %.0f seconds at %d Hz...\n", DURATION, SR);

    /* Open WAV file */
    WavWriter wav;
    wav.f = fopen("blade_runner.wav", "wb");
    if (!wav.f) { perror("fopen"); return 1; }
    wav.channels = 2;
    wav.bits = 16;
    wav.sample_rate = SR;
    wav_write_header(&wav, NUM_SAMPLES);

    /* Initialize effects */
    Reverb reverb;
    reverb_init(&reverb);

    Chorus chorus;
    chorus_init(&chorus, 2048, 0.3, 12.0);

    /* Pad oscillators - 6 voices for richness */
    #define NUM_PAD_VOICES 6
    Osc pad_osc[NUM_PAD_VOICES];
    Osc pad_detune[NUM_PAD_VOICES];
    SVFilter pad_filter[NUM_PAD_VOICES];
    ADSR pad_env[NUM_PAD_VOICES];
    double pad_detune_amount[NUM_PAD_VOICES] = {0.0, 0.003, -0.003, 0.006, -0.006, 0.001};
    int pad_prev_chord[NUM_PAD_VOICES];

    for (int i = 0; i < NUM_PAD_VOICES; i++) {
        osc_init(&pad_osc[i], 220.0);
        osc_init(&pad_detune[i], 220.0);
        svf_init(&pad_filter[i], 800.0, 2.0);
        adsr_init(&pad_env[i], 3.0, 2.0, 0.7, 4.0);
        pad_prev_chord[i] = -1;
    }

    /* Melody oscillator */
    Osc mel_osc, mel_vib;
    osc_init(&mel_osc, 440.0);
    osc_init(&mel_vib, 4.5);
    SVFilter mel_filter;
    svf_init(&mel_filter, 2000.0, 1.5);
    ADSR mel_env;
    adsr_init(&mel_env, 0.3, 0.5, 0.6, 1.5);
    int mel_prev_idx = -1;

    /* Bass oscillator */
    Osc bass_osc;
    osc_init(&bass_osc, 73.4);
    SVFilter bass_filter;
    svf_init(&bass_filter, 300.0, 1.0);
    ADSR bass_env;
    adsr_init(&bass_env, 1.0, 1.0, 0.8, 2.0);
    int bass_prev_idx = -1;

    /* Drone oscillators */
    Osc drone_osc, drone_osc2;
    osc_init(&drone_osc, 73.4);
    osc_init(&drone_osc2, 73.7);
    SVFilter drone_filter;
    svf_init(&drone_filter, 200.0, 0.7);

    /* Arpeggio oscillator */
    Osc arp_osc;
    osc_init(&arp_osc, 440.0);
    ADSR arp_env;
    adsr_init(&arp_env, 0.05, 0.3, 0.3, 0.5);
    SVFilter arp_filter;
    svf_init(&arp_filter, 3000.0, 2.0);
    int arp_prev_step = -1;

    /* LFO for filter sweeps */
    Osc global_lfo;
    osc_init(&global_lfo, 0.05);

    /* ============ COMPOSITION STRUCTURE ============ */
    double bpm = 70.0;
    double beat = 60.0 / bpm;
    double bar = beat * 4.0;

    /* Chord events (D minor progression) */
    ChordEvent chords[] = {
        {0.0,     bar*4, {50,53,57,62,65,69}, 6, 200, 1200, 0.35},
        {bar*4,   bar*4, {46,50,53,58,62,65}, 6, 300, 1500, 0.35},
        {bar*8,   bar*4, {41,45,48,53,57,60}, 6, 250, 1400, 0.35},
        {bar*12,  bar*4, {48,52,55,60,64,67}, 6, 350, 1600, 0.35},
        {bar*16,  bar*4, {50,53,57,62,65,69}, 6, 200, 1300, 0.40},
        {bar*20,  bar*4, {45,48,52,57,60,64}, 6, 300, 1500, 0.40},
        {bar*24,  bar*4, {46,50,53,58,62,65}, 6, 250, 1400, 0.40},
        {bar*28,  bar*4, {43,46,50,55,58,62}, 6, 350, 1600, 0.40},
        {bar*32,  bar*4, {50,53,57,62,65,69}, 6, 200, 1800, 0.45},
        {bar*36,  bar*4, {46,50,53,58,62,65}, 6, 300, 1600, 0.40},
        {bar*40,  bar*4, {41,45,48,53,57,60}, 6, 250, 1400, 0.35},
        {bar*44,  bar*4, {48,52,55,60,64,67}, 6, 350, 1200, 0.30},
    };
    int num_chords = sizeof(chords) / sizeof(chords[0]);

    /* Melody - sparse, emotional, D minor scale */
    MelodyNote melody[] = {
        {bar*4+beat*0, beat*3, 74, 0.30},
        {bar*4+beat*3, beat*1, 72, 0.25},
        {bar*5+beat*0, beat*2, 70, 0.28},
        {bar*5+beat*2, beat*2, 69, 0.25},
        {bar*6+beat*0, beat*3, 67, 0.30},
        {bar*6+beat*3, beat*1, 65, 0.22},
        {bar*7+beat*0, beat*4, 62, 0.28},

        {bar*8+beat*0, beat*2, 69, 0.30},
        {bar*8+beat*2, beat*2, 72, 0.28},
        {bar*9+beat*0, beat*3, 74, 0.32},
        {bar*9+beat*3, beat*1, 72, 0.25},
        {bar*10+beat*0, beat*2, 70, 0.30},
        {bar*10+beat*2, beat*2, 69, 0.28},
        {bar*11+beat*0, beat*4, 67, 0.30},

        {bar*16+beat*0, beat*2, 77, 0.35},
        {bar*16+beat*2, beat*2, 74, 0.32},
        {bar*17+beat*0, beat*3, 72, 0.33},
        {bar*17+beat*3, beat*1, 70, 0.28},
        {bar*18+beat*0, beat*2, 69, 0.32},
        {bar*18+beat*2, beat*2, 67, 0.30},
        {bar*19+beat*0, beat*4, 65, 0.30},

        {bar*24+beat*0, beat*2, 79, 0.38},
        {bar*24+beat*2, beat*2, 77, 0.35},
        {bar*25+beat*0, beat*3, 74, 0.35},
        {bar*25+beat*3, beat*1, 72, 0.30},
        {bar*26+beat*0, beat*2, 70, 0.33},
        {bar*26+beat*2, beat*2, 69, 0.30},
        {bar*27+beat*0, beat*4, 67, 0.32},

        {bar*32+beat*0, beat*3, 74, 0.30},
        {bar*32+beat*3, beat*1, 72, 0.25},
        {bar*33+beat*0, beat*4, 70, 0.28},
        {bar*34+beat*0, beat*2, 69, 0.25},
        {bar*34+beat*2, beat*2, 67, 0.22},
        {bar*35+beat*0, beat*4, 65, 0.25},

        {bar*40+beat*0, beat*4, 62, 0.20},
        {bar*44+beat*0, beat*4, 50, 0.15},
    };
    int num_melody = sizeof(melody) / sizeof(melody[0]);

    /* Bass notes */
    BassNote bass[] = {
        {0.0,     bar*4, 38, 0.30},
        {bar*4,   bar*4, 34, 0.28},
        {bar*8,   bar*4, 29, 0.28},
        {bar*12,  bar*4, 36, 0.28},
        {bar*16,  bar*4, 38, 0.32},
        {bar*20,  bar*4, 33, 0.30},
        {bar*24,  bar*4, 34, 0.30},
        {bar*28,  bar*4, 31, 0.30},
        {bar*32,  bar*4, 38, 0.32},
        {bar*36,  bar*4, 34, 0.28},
        {bar*40,  bar*4, 29, 0.25},
        {bar*44,  bar*4, 36, 0.22},
    };
    int num_bass = sizeof(bass) / sizeof(bass[0]);

    /* Arpeggio patterns */
    int arp_patterns[4][4] = {
        {62, 65, 69, 74},  /* Dm */
        {58, 62, 65, 70},  /* Bb */
        {53, 57, 60, 65},  /* F  */
        {60, 64, 67, 72},  /* C  */
    };

    /* ============ RENDER LOOP ============ */
    float *stereo_buf = (float*)malloc((size_t)NUM_SAMPLES * 2 * sizeof(float));
    memset(stereo_buf, 0, (size_t)NUM_SAMPLES * 2 * sizeof(float));

    for (int i = 0; i < NUM_SAMPLES; i++) {
        double t = (double)i / SR;

        /* Global LFO */
        double glfo = osc_sine(&global_lfo);

        /* Master volume envelope */
        double master_vol = 1.0;
        if (t < 5.0) master_vol = t / 5.0;
        if (t > DURATION - 8.0) master_vol = (DURATION - t) / 8.0;
        if (master_vol < 0) master_vol = 0;

        /* === PAD === */
        double pad_out = 0;
        int active_chord = -1;
        for (int c = 0; c < num_chords; c++) {
            if (t >= chords[c].start_time && t < chords[c].start_time + chords[c].duration) {
                active_chord = c;
                break;
            }
        }

        for (int v = 0; v < NUM_PAD_VOICES; v++) {
            if (active_chord >= 0) {
                int note_idx = v % chords[active_chord].num_notes;
                double freq = note_freq(chords[active_chord].notes[note_idx]);
                double detune = pad_detune_amount[v];
                double actual_freq = freq * (1.0 + detune);

                pad_osc[v].freq = actual_freq;
                pad_detune[v].freq = actual_freq * 1.002;

                /* Gate on chord change */
                if (active_chord != pad_prev_chord[v]) {
                    adsr_gate(&pad_env[v], 0); /* release old */
                    adsr_gate(&pad_env[v], 1); /* attack new */
                    pad_prev_chord[v] = active_chord;
                }
            } else {
                adsr_gate(&pad_env[v], 0);
                pad_prev_chord[v] = -1;
            }

            double env = adsr_tick(&pad_env[v]);

            double s1 = osc_saw(&pad_osc[v]) * 0.4;
            double s2 = osc_saw(&pad_detune[v]) * 0.3;
            double s3 = osc_sine(&pad_osc[v]) * 0.3;
            double raw = (s1 + s2 + s3) * env;

            if (active_chord >= 0) {
                double filter_progress = (t - chords[active_chord].start_time) / chords[active_chord].duration;
                if (filter_progress < 0) filter_progress = 0;
                if (filter_progress > 1) filter_progress = 1;
                double ftarget = chords[active_chord].filter_start +
                                (chords[active_chord].filter_end - chords[active_chord].filter_start) * filter_progress;
                ftarget += glfo * 200.0;
                svf_set_freq(&pad_filter[v], ftarget);
                double filtered = svf_process(&pad_filter[v], raw);
                pad_out += filtered * chords[active_chord].volume;
            }
        }
        pad_out /= NUM_PAD_VOICES;

        /* === MELODY === */
        double mel_out = 0;
        int active_mel = -1;
        for (int m = 0; m < num_melody; m++) {
            if (t >= melody[m].start_time && t < melody[m].start_time + melody[m].duration) {
                active_mel = m;
                break;
            }
        }

        if (active_mel >= 0) {
            if (active_mel != mel_prev_idx) {
                adsr_gate(&mel_env, 0);
                adsr_gate(&mel_env, 1);
                mel_prev_idx = active_mel;
            }
            double freq = note_freq(melody[active_mel].midi_note);
            double vib = osc_sine(&mel_vib) * 0.004 * freq;
            mel_osc.freq = freq + vib;

            double env = adsr_tick(&mel_env);
            double saw = osc_saw(&mel_osc) * 0.5;
            double sine = osc_sine(&mel_osc) * 0.5;
            double raw_mel = (saw + sine) * env * melody[active_mel].volume;

            svf_set_freq(&mel_filter, 1500 + glfo * 500);
            mel_out = svf_process(&mel_filter, raw_mel);
        } else {
            adsr_gate(&mel_env, 0);
            adsr_tick(&mel_env);
            mel_prev_idx = -1;
        }

        /* === BASS === */
        double bass_out = 0;
        int active_bass = -1;
        for (int b = 0; b < num_bass; b++) {
            if (t >= bass[b].start_time && t < bass[b].start_time + bass[b].duration) {
                active_bass = b;
                break;
            }
        }

        if (active_bass >= 0) {
            if (active_bass != bass_prev_idx) {
                adsr_gate(&bass_env, 0);
                adsr_gate(&bass_env, 1);
                bass_prev_idx = active_bass;
            }
            double freq = note_freq(bass[active_bass].midi_note);
            bass_osc.freq = freq;
            double env = adsr_tick(&bass_env);
            double raw_bass = (osc_saw(&bass_osc) * 0.3 + osc_sine(&bass_osc) * 0.7) * env * bass[active_bass].volume;
            svf_set_freq(&bass_filter, 250 + glfo * 50);
            bass_out = svf_process(&bass_filter, raw_bass);
        } else {
            adsr_gate(&bass_env, 0);
            adsr_tick(&bass_env);
            bass_prev_idx = -1;
        }

        /* === DRONE === */
        double drone = (osc_sine(&drone_osc) * 0.5 + osc_sine(&drone_osc2) * 0.5) * 0.08;
        svf_set_freq(&drone_filter, 150 + glfo * 30);
        drone = svf_process(&drone_filter, drone);

        /* === ARPEGGIO (bars 8-32) === */
        double arp_out = 0;
        if (t >= bar*8 && t < bar*32) {
            double arp_t = t - bar*8;
            double arp_beat = beat * 0.5;
            int arp_step = (int)(arp_t / arp_beat) % 4;
            int chord_idx = ((int)(arp_t / (bar*4.0))) % 4;
            int arp_note = arp_patterns[chord_idx][arp_step];

            if (arp_step != arp_prev_step) {
                adsr_gate(&arp_env, 0);
                adsr_gate(&arp_env, 1);
                arp_prev_step = arp_step;
            }

            arp_osc.freq = note_freq(arp_note);
            double arp_env_val = adsr_tick(&arp_env);
            double raw_arp = osc_saw(&arp_osc) * 0.3 * arp_env_val * 0.15;
            svf_set_freq(&arp_filter, 2500 + glfo * 800);
            arp_out = svf_process(&arp_filter, raw_arp);
        } else {
            adsr_gate(&arp_env, 0);
            adsr_tick(&arp_env);
            arp_prev_step = -1;
        }

        /* === MIX === */
        double chorused_pad = chorus_process(&chorus, (float)pad_out);
        double dry = (pad_out * 0.6 + chorused_pad * 0.4) + mel_out + bass_out + drone + arp_out;

        double wet = reverb_process(&reverb, (float)(dry * 0.5));

        double left = dry * 0.6 + wet * 0.7;
        double right = dry * 0.6 + wet * 0.7;
        left += (bass_out - mel_out) * 0.1;
        right += (mel_out - bass_out) * 0.1;

        left *= master_vol;
        right *= master_vol;

        stereo_buf[i * 2] = (float)left;
        stereo_buf[i * 2 + 1] = (float)right;

        if (i % (SR * 5) == 0) {
            printf("  Rendering: %d / %d seconds (%.0f%%)\n",
                   (int)t, (int)DURATION, 100.0 * t / DURATION);
        }
    }

    /* Write samples */
    printf("Writing WAV file...\n");
    wav_write_samples(&wav, stereo_buf, NUM_SAMPLES * 2);
    fclose(wav.f);
    free(stereo_buf);

    /* Cleanup */
    for (int i = 0; i < NUM_COMBS; i++) free(reverb.combs[i].buffer);
    for (int i = 0; i < NUM_ALLPASS; i++) free(reverb.allpasses[i].buffer);
    free(chorus.buffer);

    printf("Done! Output: blade_runner.wav\n");
    printf("Duration: %.1f seconds, Sample rate: %d Hz, Channels: 2, Bits: 16\n",
           DURATION, SR);

    return 0;
}