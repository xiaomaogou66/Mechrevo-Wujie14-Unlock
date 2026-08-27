/* Clear all templates from the Realtek MOC sensor */
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
  for (int i = 0; i < (n < 16 ? n : 16); i++) printf("%02x ", b[i]);
  printf("\n");
}

static int run_cmd(libusb_device_handle *h, const char *name, unsigned char *hdr, int quiet) {
  int r, t;
  unsigned char buf[2048];
  int cmdtype = (hdr[0] & 0xC0) >> 6;
  r = libusb_bulk_transfer(h, EP_OUT, hdr, 12, &t, 1000);
  if (r != 0) { printf("== %s: SEND fail r=%d\n", name, r); return -1; }
  if (cmdtype == 1) {
    r = libusb_bulk_transfer(h, EP_IN, buf, 2048, &t, 2000);
    if (r != 0) printf("== %s: DATA r=%d\n", name, r);
    else if (!quiet) hexdump(buf, t, "data");
  }
  r = libusb_bulk_transfer(h, EP_IN, buf, 2048, &t, 2000);
  if (r != 0) { printf("== %s: STATUS r=%d\n", name, r); return -1; }
  if (!quiet) hexdump(buf, t, "stat");
  printf("== %s status=0x%02x\n", name, buf[0]);
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
  unsigned char hdr[12];
  int r;

  libusb_init(&ctx);
  h = libusb_open_device_with_vid_pid(ctx, VID, PID);
  if (!h) { printf("device not found\n"); return 1; }
  r = libusb_reset_device(h);
  if (r == 0 || r == LIBUSB_ERROR_NOT_FOUND) usleep(300000);
  libusb_claim_interface(h, 0);
  printf("device claimed\n");

  /* 4-step select_os handshake (required by 9011 firmware) */
  for (int i = 0; i < 4; i++) {
    cmd_hdr(hdr, 0x05, 0x13, i, 0);
    run_cmd(h, "select_os", hdr, 1);
  }

  /* check before */
  cmd_hdr(hdr, 0x45, 0x0d, 0, 2);
  run_cmd(h, "get_enroll_num BEFORE", hdr, 0);

  /* delete all: delete_record param=0xff */
  cmd_hdr(hdr, 0x05, 0x0f, 0xff, 0);
  run_cmd(h, "delete_record(ff)", hdr, 0);

  /* check after */
  cmd_hdr(hdr, 0x45, 0x0d, 0, 2);
  run_cmd(h, "get_enroll_num AFTER", hdr, 0);

  libusb_release_interface(h, 0);
  libusb_close(h);
  libusb_exit(ctx);
  printf("\ndone\n");
  return 0;
}
