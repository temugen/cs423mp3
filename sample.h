#ifndef _SAMPLE_H_
#define _SAMPLE_H_

struct sample
{
    unsigned long timestamp,
                  major_faults,
                  minor_faults,
                  utilization;
};

#endif
