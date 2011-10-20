#ifndef __SAMPLE_H__
#define __SAMPLE_H__

struct sample
{
    unsigned long timestamp,
                  major_faults,
                  minor_faults,
                  utilization;
};

#define WORK_HZ 20
#define NUM_SAMPLES (600 * WORK_HZ)
#define BUFFER_SIZE (NUM_SAMPLES * sizeof(struct sample))

#endif
