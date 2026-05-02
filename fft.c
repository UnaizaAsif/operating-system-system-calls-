#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void fft(double complex *x, int n) {
    if (n <= 1) return;
    int half = n / 2;
    double complex *even = malloc(half * sizeof(double complex));
    double complex *odd  = malloc(half * sizeof(double complex));
    if (!even || !odd) { free(even); free(odd); return; }
    for (int i = 0; i < half; i++) {
        even[i] = x[2*i];
        odd[i]  = x[2*i + 1];
    }
    fft(even, half);
    fft(odd,  half);
    for (int k = 0; k < half; k++) {
        double complex t = cexp(-2.0 * I * M_PI * k / n) * odd[k];
        x[k]        = even[k] + t;
        x[k + half] = even[k] - t;
    }
    free(even); free(odd);
}
