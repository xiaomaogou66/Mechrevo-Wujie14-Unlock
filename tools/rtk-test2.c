/* Replicate driver's exact init+identify sequence with NO delays */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define VID 0x3274
#define PID 0x9011
#define EP_OUT 0x01
#define EP_IN  0x82

static void hexdump(const unsigned char *b, int n, const char *tag) {
  printf("  %s (%d): ", tag, n);
  int m = n < 32 ? n : 32;
  for (int i = 0; i < m; i++) printf("%02x ", b[i]);
  printf("\n");
}

/* returns 0 if status byte (last read) == 0 */
static int run_cmd(libusb_device_handle *h, const char *name, unsigned char *hdr) {
  int r, t;
  unsigned char buf[2048];
  int cmdtype = (hdr[0] & 0xC0) >> 6;
  int trans_len = (hdr[11] << 8) | hdr[10];

  r = libusb_bulk_transfer(h, EP_OUT, hdr, 12, &t, 1000);
  if (r != 0) { printf("== %s: SEND fail r=%d\n", name, r); return -1; }

  if (cmdtype == 1) {
    r = libusb_bulk_transfer(h, EP_IN, buf, 2048, &t, 2000);
    if (r != 0) printf("== %s: DATA r=%d\n", name, r);
    else hexdump(buf, t, "data");
  }

  r = libusb_bulk_transfer(h, EP_IN, buf, 2048, &t, 2000);
  if (r != 0) { printf("== %s: STATUS r=%d\n", name, r); return -1; }
  hexdump(buf, t, "stat");
  printf("== %s (type=%d tlen=%d) status=0x%02x\n", name, cmdtype, trans_len, buf[0]);
  return buf[0];
}

static void cmd_hdr(unsigned char *h, int b0, int b1, int p0, int dl) {
  memset(h, 0, 12);
  h[0] = b0; h[1] = b1; h[2] = p0;
  h[10] = dl & 0xff; h[11] = (dl >> 8) & 0xff;
}

int main(void) {
  libusb_context *ctx = NULL;
  libusb_device_handle *h = NULL;
  int r;
  unsigned char hdr[12], buf[8];

  libusb_init(&ctx);
  h = libusb_open_device_with_vid_pid(ctx, VID, PID);
  if (!h) { printf("device not found\n"); return 1; }
  if (libusb_kernel_driver_active(h, 0) == 1) libusb_detach_kernel_driver(h, 0);
  libusb_claim_interface(h, 0);
  printf("== sequence: NO delays between commands ==\n\n");

  /* 1) GET_DEVICE_INFO: ctrl transfer 0x07 value=0x000D index=0 len=8 (like driver) */
  r = libusb_control_transfer(h, 0x80 | 0x40, 0x07, 0x000D, 0x0000, buf, 8, 1000);
  printf("get_device_info ctrl: r=%d data:", r);
  for (int i = 0; i < (r > 0 ? r : 0); i++) printf(" %02x", buf[i]);
  printf("\n");

  /* 2) SELECT_OS param=1 (driver does param[0]=0x01) */
  cmd_hdr(hdr, 0x05, 0x13, 0x01, 0);
  run_cmd(h, "select_os", hdr);

  /* 3) GET_ENROLL_NUM */
  cmd_hdr(hdr, 0x45, 0x0d, 0, 2);
  run_cmd(h, "get_enroll_num", hdr);

  /* 4) GET_TEMPLATE 350 (driver: 35*10) -- immediately */
  cmd_hdr(hdr, 0x45, 0x0e, 0, 350);
  run_cmd(h, "get_template", hdr);

  /* 5) repeat GET_TEMPLATE immediately again */
  run_cmd(h, "get_template2", hdr);

  /* 6) now with small delay, retry */
  usleep(300000);
  run_cmd(h, "get_template3(after delay)", hdr);

  libusb_release_interface(h, 0);
  libusb_close(h);
  libusb_exit(ctx);
  printf("\ndone\n");
  return 0;
}
