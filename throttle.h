
// CPU Frequency Range
static constexpr int CPU_MAX_FREQ = 240;   // Max performance
static constexpr int CPU_MIN_FREQ = 80;    // Minimum power-saving mode

int throttleCPU(float temperature = -100., float cpu_temp_min = 70., float cpu_temp_max = 90);