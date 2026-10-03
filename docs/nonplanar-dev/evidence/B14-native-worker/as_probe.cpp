#include <sys/resource.h>
#include <sys/mman.h>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
int main(){rlimit r{64*1024*1024,64*1024*1024};int set=setrlimit(RLIMIT_AS,&r);void *p=mmap(nullptr,128*1024*1024,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);std::printf("setrlimit=%d mmap_failed=%d errno=%d\n",set,p==MAP_FAILED,errno);if(p!=MAP_FAILED)munmap(p,128*1024*1024);}
