float trimmed_mean(float* samples, int count, float trim_ratio) {
    int trim = count * trim_ratio;
    if (trim * 2 >= count) return 0.0;  // prevent invalid trimming

    // Simple bubble sort (replace with quicksort if you're scaling up)
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (samples[j] < samples[i]) {
                float tmp = samples[i];
                samples[i] = samples[j];
                samples[j] = tmp;
            }
        }
    }

    float sum = 0;
    for (int i = trim; i < count - trim; i++) {
        sum += samples[i];
    }
    return sum / (count - 2 * trim);
}
