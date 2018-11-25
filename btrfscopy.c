
#define _GNU_SOURCE
//#define _LARGEFILE64_SOURCE moikka
#include <stdio.h>
#include <btrfs/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

/* 
const char *devfilename    = "./devfile.du\0";
const char *comparfilename = "./comparfile.du\0";
const char *dstfilename    = "./dstfile.du\0";
 */

char *devfilename, *cmpfilename, *dstfilename;
void pexit(const char *s1, const char *s2)
{
  printf("ERROR_EXIT:\n");
  perror(s1);
  perror(s2);
  exit(-1);
}

int main(int argc, char **argv)
{
  char *inblock1, *inblock2;
  long seekpos;
  int blksize;
  int fd_devfile, fd_cmpfile, fd_dstfile;
  int samefilesystem = (1 == 0);
  blksize = getpagesize() * 64;
  if ((argc < 4) || (argc > 6))
  {
    printf("args: real_source diff_source target_samefilesystem [multiplier] \n");
    printf("where real_source would be for example /dev/sda, diff_source "
           "a previous backup-image "
           "and target would be a new backup-image\n");
    printf("\nPurpose: copy real_source to target in a way that uses btrfs clone "
           "when possible so that diff_source blocks are referred to.\n");
    printf("multiplier = os.blocksize * multiplier=transfersize\n");
#ifdef JADDAJADDA
           "samefs = 'y' also deduplicate against real_source that must reside in same fs as diff_source and target");
#endif
           printf("\nPreconditions: diff_source and target must be on same BTRFS filesystem, unpredictable amount of hard disk space is needed\n");
           printf("\nBlock size is %d (fixed in this version)\n", blksize);
           printf("v2\n");
           exit(1);
  }
  int multiplier = 64;
  if (argc > 4)
  {
    multiplier = atoi(argv[4]);
  }
  blksize = getpagesize() * multiplier;
  if (multiplier < 1)
    pexit("multiplier", "too small");
  printf("multiplier=%d\n", multiplier);

  devfilename = argv[1];
  cmpfilename = argv[2];
  dstfilename = argv[3];
  printf("dev = %s\n", devfilename);
  printf("dst = %s\n", dstfilename);

  inblock1 = malloc(blksize);
  inblock2 = malloc(blksize);
  if ((!inblock1) || (!inblock2))
    pexit("out of mem", "oUt oF mEm");

  printf("blksize=%d\n", blksize);

  fd_devfile = open(devfilename, O_LARGEFILE | O_NOATIME);
  fd_cmpfile = open(cmpfilename, O_LARGEFILE | O_NOATIME);
  fd_dstfile = open(dstfilename, O_CREAT | O_LARGEFILE | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd_devfile < 1)
    pexit("Maybe I failed to open", devfilename);
  if (fd_cmpfile < 1)
    pexit("Maybe I failed to open", cmpfilename);
  if (fd_dstfile < 1)
    pexit("Maybe I failed to open", dstfilename);
  printf("open %s ok\n", devfilename);
  printf("open %s ok\n", cmpfilename);
  printf("open %s ok\n", dstfilename);

  int sz_1, sz_2, i;
  int compsize;
  long seekpos_pre_read;
  int dupestatus;
  struct btrfs_ioctl_clone_range_args duparg;
  int ccount = 0;
  char c;

  compsize = blksize;
  duparg.src_fd = fd_cmpfile;
  duparg.src_length = compsize;

  long dev_seekpos_pre_read = -2;
  long cmp_seekpos_pre_read = -2;
  long dst_seekpos_pre_read = -2;

  //seekpos_pre_read = blksize * 15260000L;
  seekpos_pre_read = 0;

  dev_seekpos_pre_read = lseek64(fd_devfile, seekpos_pre_read, SEEK_SET); 
  cmp_seekpos_pre_read = lseek64(fd_cmpfile, seekpos_pre_read, SEEK_SET);
  dst_seekpos_pre_read = lseek64(fd_dstfile, seekpos_pre_read, SEEK_SET);
  printf("seekpos_pre_read devfile = %li\n", dev_seekpos_pre_read);
  printf("seekpos_pre_read cmpfile = %li\n", cmp_seekpos_pre_read);  
  printf("seekpos_pre_read dstfile = %li\n", dst_seekpos_pre_read);
  
  while (1) {
    seekpos_pre_read = lseek64(fd_devfile, 0, SEEK_CUR);
    // if(seekpos_pre_read > 4L*1024*1024*1024) break;

    sz_1 = read(fd_devfile, inblock1, blksize);
    sz_2 = read(fd_cmpfile, inblock2, blksize);
    if (sz_1 != sz_2)
    {
      printf("sz_1=%d sz_2=%d\n", sz_1, sz_2);
      printf("errno = %d\n", errno);
      pexit("read sizes differ, signal?", " maybe?");
    }
    compsize = sz_1;
    if (sz_1 == 0) pexit("Done.", "Done.");   
    if (sz_1 < blksize)
    {
      printf("sz_1 < blksize, viimeinen kierros\n");
      printf("seekpos_pre_read = %li\n", seekpos_pre_read);
      printf("blksize = %d\nsz_1 = %d\nsz2 = %d\n", blksize, sz_1, sz_2);
    }

    if (0 != memcmp(inblock1, inblock2, compsize))
    {
      if (write(fd_dstfile, inblock1, sz_1) != sz_1)
      {
        printf("Can not handle this. Didn't write everything");
        pexit("write failed.", "ump ump");
      };
      c = 'w';
    }
    else
    {
      duparg.dest_offset = seekpos_pre_read;
      duparg.src_offset = seekpos_pre_read;
      duparg.src_fd = fd_cmpfile;
      duparg.src_length = sz_1;

      //printf("src_offset %lu dst_offset %lu len %d\n", (long unsigned int)duparg.src_offset, (long unsigned int)duparg.dest_offset, sz_1 );
      long int seekpos_target = lseek64(fd_dstfile, 0, SEEK_CUR);
      //printf("target seekpos = %lu\n", seekpos_target);

      dupestatus = ioctl(fd_dstfile, BTRFS_IOC_CLONE_RANGE, &duparg);
      lseek64(fd_dstfile, sz_1, SEEK_CUR);
      if (dupestatus != 0)
        pexit("ioctl err", "ioctl err");

      c = 'd';
    }
    if (ccount++ > (10240 / multiplier) * 3)
    {
      putchar(c);
      fflush(stdout);
      ccount = 0;
    }
  }
 printf("Out\n");
}
