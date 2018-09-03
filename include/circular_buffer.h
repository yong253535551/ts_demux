#ifndef __CIRCULAR_BUFFER_H__
#define __CIRCULAR_BUFFER_H__

//#define WRITE_TS_FILE
#define READ_TS_NUM 100000

typedef uint8_t                 byte;

typedef enum tswrite_pkt_hdr_type_e
{
  PKT_HDR_TYPE_NONE = 0,
  PKT_HDR_TYPE_RTP
} tswrite_pkt_hdr_type_t;

#define TRUE  1
#define FALSE 0

#define DEFAULT_PARENT_WAIT  50
#define DEFAULT_CHILD_WAIT   10

static int global_parent_wait = DEFAULT_PARENT_WAIT;
static int global_child_wait = DEFAULT_CHILD_WAIT;

#define CHILD_GIVE_UP_AFTER 1000

// And a similar quantity for the parent, in case the child dies
#define PARENT_GIVE_UP_AFTER 1000

// If not being quiet, report progress every REPORT_EVERY packets read
#define REPORT_EVERY 10000

#define DISPLAY_BUFFER 1
#if DISPLAY_BUFFER
static int global_show_circular = FALSE;
#endif

#define ONE_MS_AS_NANOSECONDS   1000000

// Transport Stream packets are always the same size
#define TS_PACKET_SIZE 188

// When we are putting data into a TS packet, we need the first four
// bytes for heading information, which means that we will have at most
// 184 bytes for our payload
#define MAX_TS_PAYLOAD_SIZE  (TS_PACKET_SIZE-4)

// ============================================================
// CIRCULAR BUFFER
// ============================================================

// We default to using a "packet" of 7 transport stream packets because 7*188 =
// 1316, but 8*188 = 1504, and we would like to output as much data as we can
// that is guaranteed to fit into a single ethernet packet, size 1500.
#define DEFAULT_TS_PACKETS_IN_ITEM      7

// For simplicity, we'll have a maximum on that (it allows us to have static
// array sizes in some places). This should be a big enough size to more than
// fill a jumbo packet on a gigabit network.
#define MAX_TS_PACKETS_IN_ITEM          100

struct circular_buffer_item
{
  uint32_t time;              // when we would like this data output
  int      discontinuity;     // TRUE if our timeline has "broken"
  int      length;            // number of bytes of data in the array
};
typedef struct circular_buffer_item *circular_buffer_item_p;

#define SIZEOF_CIRCULAR_BUFFER_ITEM sizeof(struct circular_buffer_item)



typedef struct rtp_hdr_info_s
{
  uint16_t seq;
  uint32_t ssrc;
} rtp_hdr_info_t;


struct circular_buffer
{
  volatile int      start;      // start of data "pointer"
  volatile int      end;        // end of completed data "pointer" (you guessed)
  volatile int      pending;    // end of buffered but not ready for xmit
  int      size;       // the actual length of the `item` array

  FILE *filp;
  FILE *filp1;
  int write_started; //TRUE if we've started writing therein
  int read_started; //TRUE if we've started writing therein
  int which;  //Which buffer index we're writing to

  volatile int eos;    // end of stream

  int      TS_in_item; // max number of TS packets in a circular buffer item
  int      item_size;  // and thus the size of said item's data array
  int      hdr_size;
  tswrite_pkt_hdr_type_t hdr_type;
  union {
    rtp_hdr_info_t rtp;
  } hdr;

  int      maxnowait;  // max number consecutive packets to send with no wait
  int      waitfor;    // the number of microseconds to wait thereafter

  // The location of the packet data for the circular buffer items
  byte     *item_data;

  // The "header" data for each circular buffer item
  struct circular_buffer_item item[];
};
typedef struct circular_buffer *circular_buffer_p;

// Note that size doesn't include the final `item`
#define SIZEOF_CIRCULAR_BUFFER sizeof(struct circular_buffer)

// For PCR2 pacing we accumulate the initial packets here so it must be big
// enough to cope with a max bitrate stream
// Say 100Mbits is the fastest we are going to care about
// So we need to buffer 2 PCRs which have a max spacing of .1s (by the std)
// and another .1s before that for having just missed a PCR = .2s = 20Mbits
// = 2.5Mbytes. 2048 * 188 * 7 = 2.7M
#define DEFAULT_CIRCULAR_BUFFER_SIZE  2048              // used to be 100


extern int map_circular_buffer(circular_buffer_p  *circular,
                               int                 circ_buf_size,
                               int                 TS_in_packet,
                               int                 maxnowait,
                               int                 waitfor,
                               const tswrite_pkt_hdr_type_t hdr_type);
extern int unmap_circular_buffer(circular_buffer_p  circular);
extern int circular_buffer_empty(circular_buffer_p  circular);
extern int circular_buffer_full(circular_buffer_p  circular);
extern int circular_buffer_jammed(circular_buffer_p  circular);
extern int wait_if_buffer_empty(circular_buffer_p  circular);
extern int wait_for_buffer_to_fill(circular_buffer_p  circular);
extern int wait_if_buffer_full(circular_buffer_p  circular);
extern void print_circular_buffer(char              *prefix,
                                  circular_buffer_p  circular);

extern int write_circular_buffer(circular_buffer_p  circular, byte *packet, int count);
extern int read_circular_buffer(circular_buffer_p  circular, byte *rbuf, int *len);

#endif
