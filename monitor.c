#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "sample.h"

static int buf_fd = -1;

// This function opens a character device (which is pointed by a file named as fname) and performs the mmap() operation. If the operations are successful, the base address of memory mapped buffer is returned. Otherwise, a NULL pointer is returned.
void *buf_init(char *fname)
{
  char *kadr;

  if(buf_fd == -1){
    if ((buf_fd=open(fname, O_RDWR|O_SYNC))<0){
        printf("file open error. %s\n", fname);
        return NULL;
    }
  }
  kadr = mmap(0, BUFFER_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, buf_fd, 0);
  if (kadr == MAP_FAILED){
      printf("buf file open error.\n");
      return NULL;
  }

  return kadr;
}

// This function closes the opened character device file.
void buf_exit()
{
  if(buf_fd != -1){
    close(buf_fd);
    buf_fd = -1;
  }
}

int main(int argc, char* argv[])
{
  struct sample *buf;
  int i;

  // Open the char device and mmap()
  buf = (struct sample *)buf_init("node");
  if(!buf)
    return -1;

  // Read and print profiled data
  for(i=0; i < NUM_SAMPLES && buf[i].timestamp != -1; i++)
  {
    printf("%lu %lu %lu %lu\n", buf[i].timestamp, buf[i].major_faults, buf[i].minor_faults, buf[i].utilization);
  }

  printf("read %d profiled data\n", i);

  // Close the char device
  buf_exit();
}

