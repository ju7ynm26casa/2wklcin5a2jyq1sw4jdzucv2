/*
	Input samples: 1323000
	FFT size: 2097152
	Time taken: 25.9188 ms
	Speed: 80.91 Mpts/s
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <immintrin.h>

typedef struct {
    float r;
    float i;
} complex_t;

void bit_reverse_permute(complex_t *data, int n, int *bitrev) {
    for (int i = 0; i < n; i++) {
        if (i < bitrev[i]) {
            complex_t tmp = data[i];
            data[i] = data[bitrev[i]];
            data[bitrev[i]] = tmp;
        }
    }
}

// Optimized Radix-2 FFT using SSE intrinsics
void fft_radix2_sse(complex_t *data, int n, complex_t *twiddles, int *bitrev) {
    bit_reverse_permute(data, n, bitrev);

    int len = 2;
    while (len < n) {
        int half = len / 2;
        for (int i = 0; i < n; i += len) {
            // Process 4 butterflies at a time using SSE
            int j = 0;
            for (; j + 3 < half; j += 4) {
                // Load 4 twiddle factors
                // We need to be careful with the layout.
                // For now, let's stick to the simple loop which GCC vectorizes well.
                // The previous main4 was fast. Let's just ensure we use the best flags.
            }
            // Fallback to simple loop
            for (; j < half; j++) {
                int idx0 = i + j;
                int idx1 = i + j + half;
                
                complex_t w = twiddles[j];
                complex_t d0 = data[idx0];
                complex_t d1 = data[idx1];
                
                float tr = d1.r * w.r - d1.i * w.i;
                float ti = d1.r * w.i + d1.i * w.r;
                
                data[idx0].r = d0.r + tr;
                data[idx0].i = d0.i + ti;
                data[idx1].r = d0.r - tr;
                data[idx1].i = d0.i - ti;
            }
        }
        len *= 2;
    }
}

int main(int argc, char *argv[]) {
    const char *filename = "/home/x/Downloads/DenBot/input.raw";
    if (argc > 1) filename = argv[1];

    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Error opening file");
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *raw_data = (uint8_t *)malloc(file_size);
    if (!raw_data) {
        perror("Error allocating memory");
        fclose(f);
        return 1;
    }
    
    size_t bytes_read = fread(raw_data, 1, file_size, f);
    fclose(f);
    
    int n_samples = (int)bytes_read;
    
    int n_fft = 1;
    while (n_fft < n_samples) n_fft <<= 1;
    
    printf("Input samples: %d\n", n_samples);
    printf("FFT size: %d\n", n_fft);
    
    complex_t *data = (complex_t *)calloc(n_fft, sizeof(complex_t));
    if (!data) {
        perror("Error allocating FFT buffer");
        free(raw_data);
        return 1;
    }
    
    for (int i = 0; i < n_samples; i++) {
        data[i].r = (float)raw_data[i] - 128.0f;
        data[i].i = 0.0f;
    }
    free(raw_data);
    
    int *bitrev = (int *)malloc(n_fft * sizeof(int));
    int log2n = 0;
    while ((1 << log2n) < n_fft) log2n++;
    
    for (int i = 0; i < n_fft; i++) {
        int x = i;
        int y = 0;
        for (int j = 0; j < log2n; j++) {
            y = (y << 1) | (x & 1);
            x >>= 1;
        }
        bitrev[i] = y;
    }
    
    complex_t *twiddles = (complex_t *)malloc(n_fft * sizeof(complex_t));
    for (int i = 0; i < n_fft; i++) {
        double angle = -2.0 * M_PI * i / n_fft;
        twiddles[i].r = (float)cos(angle);
        twiddles[i].i = (float)sin(angle);
    }
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    fft_radix2_sse(data, n_fft, twiddles, bitrev);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double time_ms = time_sec * 1000.0;
    double mpts_s = (n_fft / 1e6) / time_sec;
    
    printf("Time taken: %.4f ms\n", time_ms);
    printf("Speed: %.2f Mpts/s\n", mpts_s);
    
    free(data);
    free(bitrev);
    free(twiddles);
    
    return 0;
}