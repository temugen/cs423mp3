#ifndef __SAMPLE_H__
#define __SAMPLE_H__

struct sample
{
    unsigned long timestamp,
                  major_faults,
                  minor_faults,
                  utilization;
};

#endif
