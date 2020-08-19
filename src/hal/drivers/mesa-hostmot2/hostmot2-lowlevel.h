
//
//    Copyright (C) 2007-2008 Sebastian Kuzminsky
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#ifndef HOSTMOT2_LOWLEVEL_H
#define HOSTMOT2_LOWLEVEL_H

#include RTAPI_INC_DEVICE_H
#include RTAPI_INC_FIRMWARE_H

#include "rtapi.h"
#include "hal.h"

#include "bitfile.h"


//
// Note: LL_PRINT(), LL_PRINT_IF(), and THIS_PRINT() all use rtapi_print()
//       The others all use rtapi_print_msg()
//

#define LL_PRINT(fmt, args...)    rtapi_print(HM2_LLIO_NAME ": " fmt, ## args);
#define THIS_PRINT(fmt, args...)  rtapi_print("%s: " fmt, this->name, ## args);

#define LL_PRINT_IF(enable, fmt, args...)  if (enable) { rtapi_print(HM2_LLIO_NAME ": " fmt, ## args); }

#define LL_ERR(fmt, args...)   rtapi_print_msg(RTAPI_MSG_ERR,  HM2_LLIO_NAME ": " fmt, ## args);
#define LL_WARN(fmt, args...)  rtapi_print_msg(RTAPI_MSG_WARN, HM2_LLIO_NAME ": " fmt, ## args);
#define LL_INFO(fmt, args...)  rtapi_print_msg(RTAPI_MSG_INFO, HM2_LLIO_NAME ": " fmt, ## args);
#define LL_DBG(fmt, args...)   rtapi_print_msg(RTAPI_MSG_DBG,  HM2_LLIO_NAME ": " fmt, ## args);

#define THIS_ERR(fmt, args...)   rtapi_print_msg(RTAPI_MSG_ERR,  "%s: " fmt, this->name, ## args);
#define THIS_WARN(fmt, args...)  rtapi_print_msg(RTAPI_MSG_WARN, "%s: " fmt, this->name, ## args);
#define THIS_INFO(fmt, args...)  rtapi_print_msg(RTAPI_MSG_INFO, "%s: " fmt, this->name, ## args);
#define THIS_DBG(fmt, args...)   rtapi_print_msg(RTAPI_MSG_DBG,  "%s: " fmt, this->name, ## args);


#define ANYIO_MAX_IOPORT_CONNECTORS (8)




//
// this struct holds an abstract "low-level I/O" driver
//

typedef struct hm2_lowlevel_io_struct hm2_lowlevel_io_t;

typedef struct {
    struct {
        hal_bit_t *io_error;
    } pin;
} hm2_low_level_io_global_t;

// FIXME: this is really a lowlevel io *instance*, or maybe a "board"
struct hm2_lowlevel_io_struct {
    char name[HAL_NAME_LEN-10]; // Needs to be shorter than a full HAL name because this is copied into a pin name
    int comp_id;

    // these two are required
    // on success these two return TRUE (not zero)
    // on failure they return FALSE (0) and set *self->io_error (below) to TRUE
    int (*read)(hm2_lowlevel_io_t *self, u32 addr, void *buffer, int size);
    int (*write)(hm2_lowlevel_io_t *self, u32 addr, const void *buffer, int size);

    // these are optional
    int (*program_fpga)(hm2_lowlevel_io_t *self,
      const bitfile_t *bitfile,
      const struct firmware *fw);
    int (*reset)(hm2_lowlevel_io_t *self);

    // use this if the firmware is not Xilinx bitfile format.
    // if NULL, uses legacy behavior (bitfile_parse_and_verify()
    int (*verify_firmware)(hm2_lowlevel_io_t *self, const struct firmware *fw);

    // optional
    // resets the communication method to the fpga - can be called by the
    // watchdog recovery code.  primary motivation was to add this for hm2_eth.c use
    // where it needs to close/re-open a socket due to some intermittent ethernet
    // error that the user has resolved and they don't want to restart the entire world.
    // if the io_reset fails, set the io_error parameter to true
    void (*io_reset)(hm2_lowlevel_io_t *self);

    // for devices with a lot of inherent latency (ethernet and spi are two
    // examples), it is useful to divide the work of the bulk reads which occur
    // every servo cycle into up to three groups:
    //   * queuing the reads -- the buffer must point to storage which is stable
    //     thrugh the eventual receive_queued_reads call
    //   * actually requesting the queued reads
    //   * actually receiving the read result and storing it in the buffer given in the
    //     queue_read call
    //
    // these routines are optional: the llio may provide any of these subsets:
    //   * none, in which case a dummy implementation of ->queue_read delegates to
    //     ->read
    //   * queue_read and send_queued_reads, in which case send_queued_reads must also
    //     receive and process the reads
    //   * all three
    //
    //  Return codes?????  Undocumented.  Assuming they should behave consistent
    //  with the read and write above which are 0 on failure and !0 on success.
    int (*queue_read)(hm2_lowlevel_io_t *self, u32 addr, void *buffer, int size);
    int (*send_queued_reads)(hm2_lowlevel_io_t *self);
    int (*receive_queued_reads)(hm2_lowlevel_io_t *self);

    // similarly, it is useful to divide the work of bulk writes into two groups
    //   * queueing the writes
    //   * actually performing the writes
    // these routines are optional; the llio may either provide both of them, or neither
    // (in which case a dummy implementation of ->queue_write delegates to ->write)
    //
    //  Return codes?????  Undocumented.  Assuming they should behave consistent
    //  with the read and write above which are 0 on failure and !0 on success.
    int (*queue_write)(hm2_lowlevel_io_t *self, u32 addr, const void *buffer, int size);
    int (*send_queued_writes)(hm2_lowlevel_io_t *self);

//
    // This is a HAL parameter allocated and added to HAL by hostmot2.
    //
    // * The llio driver sets it whenever it detects an I/O error.
    //
    // * The hostmot2 driver checks it and stops calling the llio driver if
    //   it's true.
    //
    // * Users can clear it (by poking the HAL parameter), which makes the
    //   hostmot2 driver call into llio to reset the hardware and start
    //   driving it again.
    //
    hm2_low_level_io_global_t *hal;

    // this gets set to TRUE by .read-request and cleared by .read, in order
    // to amortize latency on multiple ethernet devices
    int read_requested;

    // the period (in ns) of the last read-request invocation
    unsigned long period;

    // the time (in ns) that the last read-request was issued
    unsigned long long read_time;

    // TRUE if it is useful to split reads into a request and response part
    int split_read;

    // this gets set to TRUE when the llio driver detects an io_error, and
    // by the hm2 watchdog (if present) when it detects a watchdog bite
    // this requires user intervention to clear the io_error and then
    // the needs_reset flag will be reacted to in the next
    // hm2_watchdog_write() call.
    int needs_reset;

    // !0 when recoverable errors have occurred (minor packet loss in the case of hm2_eth llio driver is an example).
    int needs_soft_reset;

    // the pin-count and names of the io port connectors on this board
    int num_ioport_connectors;
    int pins_per_connector;
    const char *ioport_connector_name[ANYIO_MAX_IOPORT_CONNECTORS];

    // llio enumeration is the easiest place to count the leds
    int num_leds;

    // the part number of the FPGA on this board
    // optional, enhances firmware sanity checking if present
    const char *fpga_part_number;

    // the llio can set this to TRUE (non-zero) or to FALSE (zero)
    // if FALSE, the hostmot2 driver will export only global read() and write() functions (and pet_watchdog())
    // if TRUE, the hostmot2 driver will export those three functions and also read_gpio() and write_gpio()
    int threadsafe;

    char *firmware; // Name of the firmware file
    int host_wants_irq; // flag passed in to request llio to setup IRQ
    int irq_fd;     // The uio file descriptor used for irq blocking

    void *private;  // for the low-level driver to hang their struct on

    void *fwid_msg; // the lowlevel driver may override any built-in fwid message
    int   fwid_len; // in the FPGA image
};




//
// HostMot2 functions for the low-level I/O drivers to call
//

int hm2_register(hm2_lowlevel_io_t *llio, char *config);
void hm2_unregister(hm2_lowlevel_io_t *llio);


#endif //  HOSTMOT2_LOWLEVEL_H

