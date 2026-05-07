#include "kernel/types.h"
#include "kernel/memlayout.h"
#include "user/user.h"

// Test 1: simple COW — allocate lots of memory, fork, child exits
void
simpletest()
{
  uint64 phys_size = PHYSTOP - KERNBASE;
  int sz = (phys_size / 3) * 2;

  printf("simple: ");

  char *p = sbrk(sz);
  if(p == (char*)0xffffffffffffffffL){
    printf("sbrk(%d) failed\n", sz);
    exit(-1);
  }

  for(char *q = p; q < p+sz; q += 4096){
    *(int*)q = getpid();
  }

  int pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(-1);
  }
  if(pid == 0)
    exit(0);

  if(wait(0) < 0){
    printf("wait() failed\n");
    exit(-1);
  }

  if(sbrk(-sz) == (char*)0xffffffffffffffffL){
    printf("sbrk(-%d) failed\n", sz);
    exit(-1);
  }

  printf("ok\n");
}

// Test 2: three processes all write COW memory — checks ref counts
void
threetest()
{
  uint64 phys_size = PHYSTOP - KERNBASE;
  int sz = phys_size / 4;
  int pid1, pid2;

  printf("three: ");

  char *p = sbrk(sz);
  if(p == (char*)0xffffffffffffffffL){
    printf("sbrk(%d) failed\n", sz);
    exit(-1);
  }

  pid1 = fork();
  if(pid1 < 0){
    printf("fork failed\n");
    exit(-1);
  }
  if(pid1 == 0){
    pid2 = fork();
    if(pid2 < 0){
      printf("fork failed\n");
      exit(-1);
    }
    if(pid2 == 0){
      for(char *q = p; q < p+sz; q += 4096)
        *(int*)q = getpid();
      for(char *q = p; q < p+sz; q += 4096){
        if(*(int*)q != getpid()){
          printf("wrong content\n");
          exit(-1);
        }
      }
      exit(0);
    }
    for(char *q = p; q < p+sz; q += 4096)
      *(int*)q = getpid();
    for(char *q = p; q < p+sz; q += 4096){
      if(*(int*)q != getpid()){
        printf("wrong content\n");
        exit(-1);
      }
    }
    wait(0);
    exit(0);
  }

  for(char *q = p; q < p+sz; q += 4096)
    *(int*)q = getpid();
  for(char *q = p; q < p+sz; q += 4096){
    if(*(int*)q != getpid()){
      printf("wrong content\n");
      exit(-1);
    }
  }
  wait(0);

  if(sbrk(-sz) == (char*)0xffffffffffffffffL){
    printf("sbrk(-%d) failed\n", sz);
    exit(-1);
  }

  printf("ok\n");
}

// Test 3: COW + pipe (tests copyout with COW pages)
char junk1[4096];
int fds[2];
char junk2[4096];
char buf[4096];
char junk3[4096];

void
filetest()
{
  printf("file: ");

  buf[0] = 0;
  if(pipe(fds) != 0){
    printf("pipe() failed\n");
    exit(-1);
  }
  int pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(-1);
  }
  if(pid == 0){
    close(fds[0]);
    char buf2[sizeof(buf)];
    memset(buf2, 'x', sizeof(buf2));
    if(write(fds[1], buf2, sizeof(buf2)) != sizeof(buf2)){
      printf("write failed\n");
      exit(-1);
    }
    close(fds[1]);
    exit(0);
  }
  close(fds[1]);
  int n = read(fds[0], buf, sizeof(buf));
  if(n != sizeof(buf)){
    printf("read failed %d\n", n);
    exit(-1);
  }
  close(fds[0]);
  for(int i = 0; i < (int)sizeof(buf); i++){
    if(buf[i] != 'x'){
      printf("wrong content\n");
      exit(-1);
    }
  }
  if(wait(0) < 0){
    printf("wait() failed\n");
    exit(-1);
  }
  printf("ok\n");
}

int
main(int argc, char *argv[])
{
  simpletest();
  threetest();
  filetest();
  printf("ALL COW TESTS PASSED\n");
  exit(0);
}