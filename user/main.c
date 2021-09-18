#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>

#include "testcases.h"

#define PAGE_OFFSET		0xffff880000000000ULL
#define SWAP_MASK 1ULL << 62
#define PAGE_SIZE		(4096)
#define THP_SIZE		(2097152)
#define SWAP_LIMIT 4096
#define THP_MASK 1ULL << 22

#define GET_PFN(pfn) ((pfn) & (~(0x1ffULL << 55)))
#define IN_SWAP(pfn) (((pfn) & SWAP_MASK) != 0)
#define IS_IDLE(bitmap, pfn) (((bitmap) & (1ULL << (pfn) % 64)) != 0)
#define IS_THP(pfn) (((pfn) & THP_MASK) != 0)

#define BALLOONING 442
#define BSWAP 443
#define SIGBALLOON 60

void *buff;
unsigned long nr_signals = 0;
unsigned long long *pfn_list;
unsigned long n_reclaim = 0;
static struct timeval t1, t2;

//The code of idle page tracking is inspired by
//https://github.com/brendangregg/wss/blob/master/wss-v1.c
//and https://github.com/sjp38/idle_page_tracking/blob/master/pageidle.c

int checkthp(unsigned long long pfn)
{
  char *kflagspath = "/proc/kpageflags";
  int kflagsfd;
  unsigned long long kflags;

  if((kflagsfd = open(kflagspath, O_RDWR)) < 0)
  {
    printf("kpageflags bitmap open error\n");
    goto exit;
  }

  if(pread(kflagsfd, &kflags, sizeof(pfn), pfn*8) != sizeof(pfn))
  {
    printf("kpageflags read error\n");
    goto exit;
  }

  if(IS_THP(kflags))
    return 1;
exit:
  close(kflagsfd);
  return -1;
}
int setidleall(unsigned long long mapstart, unsigned long long mapend)
{
	char *idlepath = "/sys/kernel/mm/page_idle/bitmap";
  char *pagepath =  "/proc/self/pagemap";
  int pagefd, idlefd;
  unsigned long long page, poffset, idleoffset, pfn, setbits;

  if((pagefd = open(pagepath, O_RDONLY)) < 0)
  {
    printf("pagemap open error\n");
    return -1;
  }

  if((idlefd = open(idlepath, O_RDWR)) < 0)
  {
    printf("idle bitmap open error\n");
    return -1;
  }

  for(page=mapstart; page<mapend; page+=PAGE_SIZE)
  {
    poffset = page/PAGE_SIZE*8;
    if(pread(pagefd, &pfn, sizeof(pfn), poffset) != sizeof(pfn))
    {
      printf("pagemap read error\n");
      goto exit;
    }
        
    pfn = GET_PFN(pfn);
    if(pfn == 0)
      continue;

    idleoffset = pfn/64*8;
    setbits = ~(0ULL);
    int err = pwrite(idlefd, &setbits, sizeof(setbits), idleoffset);

  }
exit:
  close(pagefd);
  close(idlefd);
  return 0;
}

int getidle(unsigned long long mapstart, unsigned long long mapend, int huge)
{
	char *idlepath = "/sys/kernel/mm/page_idle/bitmap";
  char *pagepath =  "/proc/self/pagemap";
  int pagefd, idlefd;
  unsigned long long page, poffset, idleoffset, pfn, setbits, bitmap;

  if((pagefd = open(pagepath, O_RDONLY)) < 0)
  {
    printf("pagemap open error\n");
    return -1;
  }

  if((idlefd = open(idlepath, O_RDONLY)) < 0)
  {
    printf("idle bitmap open error\n");
    return -1;
  }

  for(page=mapstart; page<mapend; page += PAGE_SIZE)
  {
    poffset = (page/PAGE_SIZE)*8;
    if(pread(pagefd, &pfn, sizeof(pfn), poffset) != sizeof(pfn))
    {
      printf("pagemap read error\n");
      goto exit;
    }
        
    pfn = GET_PFN(pfn);

    if(pfn == 0 || IN_SWAP(pfn))
      continue;

    idleoffset = (pfn/64)*8;
    if(pread(idlefd, &bitmap, sizeof(bitmap), idleoffset) != sizeof(bitmap))
    {
      printf("bitmap read error\n");
      goto exit;
    }

    if(huge && checkthp(pfn) == 1)
      page+=THP_SIZE-PAGE_SIZE;

    if(IS_IDLE(bitmap, pfn))
    {
      pfn_list[n_reclaim] = pfn;
      n_reclaim++;
    }
    if(n_reclaim > SWAP_LIMIT)
      goto exit;
  }
exit:
  close(pagefd);
  close(idlefd);
  return 0;
}

int walkmaps(int flag, int huge)
{
  char *mapspath = "/proc/self/maps";
  FILE *mapf;
  unsigned long long mapstart, mapend;
  char line[128];
  n_reclaim = 0;

  if((mapf = fopen(mapspath, "r")) == NULL)
  {
    printf("maps read error\n");
    return -1;
  }

  while(fgets(line, sizeof(line), mapf) != NULL)
  {
		sscanf(line, "%llx-%llx", &mapstart, &mapend);
		if (mapstart > PAGE_OFFSET)
      continue;
    if(flag)
    {
      if(getidle(mapstart, mapend, huge) < 0)
        goto exit;
    }
    else
      setidleall(mapstart, mapend);
  }
exit:
  fclose(mapf);
  return 0;
}

int page_reclaim()
{
  unsigned long nr_reclaimed = 0;
  int huge = 1;
  walkmaps(1, 1);
  nr_reclaimed = syscall(BSWAP, pfn_list, n_reclaim);
  if(nr_reclaimed < 0)
    perror("bswap syscall failed\n");
  printf("Page reclaimed %ld\n", nr_reclaimed);

  return 0;
}

void handler(int sig, siginfo_t *info, void *ucontext)
{
  nr_signals++;
  page_reclaim();
  //set all idle bits
  walkmaps(0, 0);
}

int main(int argc, char *argv[])
{
  int *ptr, nr_pages;
  struct sigaction act;
  long activity;

  ptr = mmap(NULL, TOTAL_MEMORY_SIZE, PROT_READ | PROT_WRITE,
  MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

  if (ptr == MAP_FAILED) {
  printf("mmap failed\n");
  exit(1);
  }
  buff = ptr;
  memset(buff, 0, TOTAL_MEMORY_SIZE);

  pfn_list = (unsigned long long*)malloc(SWAP_LIMIT*sizeof(unsigned long long));

  // Intializing signal
  act.sa_sigaction = handler;
  act.sa_flags = SA_SIGINFO;
  if(sigaction(SIGBALLOON, &act, NULL) == -1)
  {
    perror("sigaction");
  }

  //Set idle bit for all pages;
  walkmaps(0, 0);

  // Calling BALLOONING syscall
  activity = syscall(BALLOONING);
  if(activity < 0)
    perror("syscall ballooning failed");

  /* test-case */
  test_case_main(buff, TOTAL_MEMORY_SIZE);

  munmap(ptr, TOTAL_MEMORY_SIZE);
  free(pfn_list);
  printf("I received SIGBALLOON %lu times\n", nr_signals);
}
