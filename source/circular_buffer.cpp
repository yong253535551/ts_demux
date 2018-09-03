#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <ctype.h>  // for isprint

#include <sys/types.h>
#include <time.h>        // Sleeping and timing

#include <unistd.h>
#include <sys/time.h>    // gettimeofday
#include <sys/mman.h>    // memory mapping
#include <sys/wait.h>
#include <sys/socket.h>  // send

#include <stdint.h>
#include <sys/mman.h>

#include "circular_buffer.h"

static int global_parent_debug = FALSE;
static int global_show_all_times = FALSE; // extra info for the above
static int global_child_debug = FALSE;

// Should we try to simulate network choppiness by randomly perturbing
// the child process's idea of time? If `global_perturb_range` is non-zero,
// then yes, we should (admittedly with rather a blunt hammer).
// In which case, we then specify a seed to use for our random perturbations,
// and a range for the time in milliseconds that we should use as a range
// (we'll generate values for that range on either side of zero).
static unsigned global_perturb_seed;
static unsigned global_perturb_range = 0;
static int      global_perturb_verbose = FALSE;

extern int map_circular_buffer(circular_buffer_p  *circular,
                               int                 circ_buf_size,
                               int                 TS_in_packet,
                               int                 maxnowait,
                               int                 waitfor,
                               const tswrite_pkt_hdr_type_t hdr_type)
{
  // Rather than map a file, we'll map anonymous memory
  // BSD supports the MAP_ANON flag as is,
  // Linux (bless it) deprecates MAP_ANON and would prefer us to use
  // the more verbose MAP_ANONYMOUS (but MAP_ANON is still around, so
  // we'll stick with that while we can)

  // The shared memory starts with the circular buffer "header". This ends with
  // an array of `circular_buffer_item` structures, of length `circ_buf_size`.
  //
  // Each circular buffer item needs enough space to store (up to)
  // `TS_in_packet` TS entries (so, `TS_in_packet`*188 bytes). Since that size
  // is not fixed, we can't just allocate it "inside" the buffer items (it
  // wouldn't be nice to allocate the *maximum* possible space we might want!).
  // Instead, we'll put it as a byte array after the rest of our data.
  //
  // Space may be left to add an RTP header before each items data
  //
  // So:
  const int hdr_size = (hdr_type == PKT_HDR_TYPE_RTP) ? 12 : 0;
  int base_size = SIZEOF_CIRCULAR_BUFFER +
                  (circ_buf_size * SIZEOF_CIRCULAR_BUFFER_ITEM);
  int data_size = circ_buf_size * (TS_in_packet * TS_PACKET_SIZE + hdr_size);
  int total_size = base_size + data_size;
  circular_buffer_p cb;

  *circular = NULL;

  cb = (circular_buffer_p)mmap(NULL,total_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
  if (cb == MAP_FAILED)
  {
    printf("### Error mapping circular buffer as shared memory: %s\n",
               strerror(errno));
    return 1;
  }

  if(access("yong.ts", F_OK) == 0){
    remove("yong.ts");
    printf("remove yong.ts...\n");
  }
  cb->filp = fopen("yong.ts","wb");
  if (cb->filp == NULL){
      printf("### Unable to open output file: %s\n", strerror(errno));
      return 1;
  }
#ifdef WRITE_TS_FILE
  if(access("yong1.ts", F_OK) == 0){
    remove("yong1.ts");
    printf("remove yong1.ts...\n");
  }
  cb->filp1 = fopen("yong1.ts","wb");
  if (cb->filp1 == NULL){
      printf("### Unable to open output file: %s\n", strerror(errno));
      return 1;
  }
#endif

  cb->start = 0;
  cb->end = 0;
  cb->pending = 0;
  cb->write_started = 0;
  cb->read_started = 0;
  cb->which = 0;
  cb->eos = FALSE;
  cb->size = circ_buf_size;
  cb->TS_in_item = TS_in_packet;//7
  cb->item_size = TS_in_packet * TS_PACKET_SIZE + hdr_size;
  cb->hdr_size = hdr_size;
  cb->hdr_type = hdr_type;
  if (hdr_type == PKT_HDR_TYPE_RTP)
  {
    struct timeval now;
    gettimeofday(&now, NULL);

    cb->hdr.rtp.seq = 0;
    cb->hdr.rtp.ssrc = (uint32_t)(now.tv_sec ^ now.tv_usec << 12);  // A somewhat random number
  }
  cb->maxnowait = maxnowait;
  cb->waitfor = waitfor;
  cb->item_data = (byte *) cb + base_size + hdr_size;
  *circular = cb;
  return 0;
}

extern int unmap_circular_buffer(circular_buffer_p  circular)
{
  int base_size = SIZEOF_CIRCULAR_BUFFER +
                  (circular->size * SIZEOF_CIRCULAR_BUFFER_ITEM);
  int data_size = circular->size * circular->item_size;
  int total_size = base_size + data_size;
  int err = munmap(circular,total_size);
  if (err)
  {
    printf("### Error unmapping circular buffer from shared memory: %s\n",
               strerror(errno));
    return 1;
  }
  return 0;
}

/*
 * Is the buffer empty?
 */
extern int circular_buffer_empty(circular_buffer_p  circular)
{
  return (circular->start % circular->size == circular->end);
}

/*
 * Is the buffer full?
 */
extern int circular_buffer_full(circular_buffer_p  circular)
{
  return ((circular->end + 1) % circular->size == circular->start);
}

/*
 * If the circular buffer is empty, wait until it gains some data.
 *
 * Returns 0 if all goes well, 1 if something goes wrong.
 */
extern int wait_if_buffer_empty(circular_buffer_p  circular)
{
  int count = 0;
  int    err;

  while (circular_buffer_empty(circular))
  {
    count ++;

    usleep(1000);

    if (count > 5)
    {
      printf("### empty: giving up (parent not responding 500ms)\n");
      return 1;
    }
  }

  count = 0;
  return circular_buffer_empty(circular);  // If empty then EOS so return 1
}

/*
 * Wait for the circular buffer to fill up
 *
 * Returns 0 if all goes well, 1 if something goes wrong.
 */
extern int wait_for_buffer_to_fill(circular_buffer_p  circular)
{
  static int count = 0;
  struct timespec   time = {0,global_child_wait*ONE_MS_AS_NANOSECONDS};
  int    err;

  while (!circular_buffer_full(circular) && !circular->eos)
  {
    count ++;

    usleep(1000);

    // If we wait for a *very* long time, maybe our parent has crashed
    if (count > 50)
    {
      printf("### Child: giving up (parent not responding 50ms)\n");
      return 1;
    }
  }

  return 0;
}

/*
 * If the circular buffer is full, wait until it gains some room.
 *
 * Returns 0 if all goes well, 1 if something goes wrong.
 */
extern int wait_if_buffer_full(circular_buffer_p  circular)
{
  int  count = 0;
  int    err;

  while (circular_buffer_full(circular))
  {
    count ++;

    usleep(1000);

    if (count > 5)
    {
      printf("### full: giving up (child not responding 500ms)\n");
      return 1;
    }
  }
  
  return 0;
}

/*
 * Print out the buffer contents, prefixed by a prefix string
 */
extern void print_circular_buffer(char              *prefix,
                                  circular_buffer_p  circular)
{
  int ii;
  if (prefix != NULL)
    printf("%s ",prefix);
  for (ii = 0; ii < circular->size; ii++)
  {
    byte* offset = circular->item_data + (ii * circular->item_size);
    printf("%s",(circular->start == ii ? "[":" "));
    if (*offset == 0)
      printf("..");
    else
      printf("%02x",*offset);
    printf("%s ",(circular->end == ii ? "]":" "));
  }
  printf("\n");
}

static int num_packet = 0;
static int total_len = 0;
static int write_file_data(circular_buffer_p  circular,
                           byte         data[],
                           int          data_len)
{
  size_t  written = 0;
  errno = 0;

  if(num_packet > READ_TS_NUM){
    printf("num_packet = %d data_len = %d total_len = %d...\n", num_packet, data_len, total_len);
    fclose(circular->filp);
    return -1;
  }
  num_packet++;
  total_len = total_len + data_len;

  written = fwrite(data,1,data_len, circular->filp);
  if (written != data_len)
  {
    printf("### Error writing out TS packet data: %s\n",strerror(errno));
    return 1;
  }
  return 0;
}

static int num_packet1 = 0;
static int total_len1 = 0;
static int write_file1_data(circular_buffer_p  circular,
                           byte         data[],
                           int          data_len)
{
  size_t  written = 0;
  errno = 0;

  if(num_packet1 > READ_TS_NUM){
    printf("num_packet1 = %d data_len = %d total_len1 = %d...\n", num_packet1, data_len, total_len1);
    fclose(circular->filp1);
    return 0;
  }
  num_packet1++;
  total_len1 = total_len1 + data_len;

  written = fwrite(data,1,data_len, circular->filp1);
  if (written != data_len)
  {
    printf("### Error writing out TS packet data: %s\n",strerror(errno));
    return 1;
  }
  return 0;
}

extern int write_circular_buffer(circular_buffer_p  circular, byte *packet, int count)
{
  byte  *data = NULL;
  int   *length = 0;
  int ret = -1;

#ifdef WRITE_TS_FILE
  ret = write_file1_data(circular, packet, count);
  if(ret){
    printf("fail to write to ts file...\n");

    return -1;
  }
#endif

  ret = wait_if_buffer_full(circular);
  if(ret){
    printf("fail to wait_if_buffer_full circular->end = %d...\n", circular->end);
    return -1;
  }else{
    //printf("success to wait_if_buffer_full...\n");
  }

  data     =   circular->item_data + circular->end*circular->item_size;
  length   = &(circular->item[circular->end].length);
 
  (*length)  = 0;
  memset(data, circular->item_size, 0);

  memcpy(&(data[*length]), packet, count);
  (*length) = count;

  if(circular->item_data[0] != 0x47){
	printf("circular->item_data[0] != 0x47 write...\n");
  } 

  //printf("write:circular->start = %d circular->end = %d item_size = %d circular->size = %d count = %d\n",  circular->start, circular->end, circular->item_size, circular->size, count);

  circular->end   = (circular->end + 1) % circular->size;
 
  return 0;
}


extern int read_circular_buffer(circular_buffer_p  circular, byte *rbuf, int *len)
{
  int ret = -1;
  byte   *buffer  = NULL;
  int     length  = 0;

  ret = wait_if_buffer_empty(circular);
  if(ret){
    *len = 0;
    //printf("fail to wait_if_buffer_empty circular->start = %d circular->end = %d...\n", circular->start, circular->end);
    return 1;
  }

  buffer  = circular->item_data + circular->start*circular->item_size;
  length  = circular->item[circular->start].length;
  
  if(circular->item_data[0] != 0x47){
	printf("circular->item_data[0] != 0x47 read...\n");
  } 
 
  //printf("read:circular->start = %d circular->end = %d circular->item_size = %d length = %d\n", circular->start, circular->end,circular->item_size, length);

  *len = length;
  memcpy(rbuf, buffer, length);

  ret = write_file_data(circular, buffer, length);
  if(ret){
    printf("fail to write to ts file...\n");

    return -1;
  }

  circular->start = (circular->start + 1) % circular->size;
     
  return 0;
}
