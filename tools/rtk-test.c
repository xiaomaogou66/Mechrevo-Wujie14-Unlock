#define _DEFAULT_SOURCE
#include <unistd.h>
/* Raw USB probe for Realtek MOC fingerprint sensor 3274:9011 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#define VID 0x3274
#define PID 0x9011
#define EP_OUT 0x01
#define EP_IN  0x82

static void hexdump(const unsigned char *b, int n, const char *tag) {
  printf("  %s (%d): ", tag, n);
  int m = n < 32 ? n : 32;
  for (int i = 0; i < m; i++) printf("%02x ", b[i]);
  if (n > 32) printf("...");
  printf("\n");
}

/* cmd: 12-byte header.  returns data phase (if READ) and status phase */
static void run_cmd(libusb_device_handle *h, const char *name, unsigned char *hdr, int do_data_read) {
  int r, t;
  unsigned char buf[2048];
  int cmdtype = (hdr[0] & 0xC0) >> 6; /* 0=only 1=read 2=write */
  int trans_len = (hdr[11] << 8) | hdr[10];

  r = libusb_bulk_transfer(h, EP_OUT, hdr, 12, &t, 1000);
  if (r != 0) { printf("== %s: SEND fail r=%d\n", name, r); return; }

  if (cmdtype == 1) { /* BULK_READ: data phase */
    r = libusb_bulk_transfer(h, EP_IN, buf, 2048, &t, 3000);
    if (r != 0) printf("== %s: DATA r=%d\n", name, r);
    else hexdump(buf, t, "data");
  }

  /* status phase */
  r = libusb_bulk_transfer(h, EP_IN, buf, 2048, &t, 3000);
  if (r != 0) printf("== %s: STATUS r=%d\n", name, r);
  else hexdump(buf, t, "stat");
  printf("== %s done (type=%d trans_len=%d)\n", name, cmdtype, trans_len);
}

static void cmd_hdr(unsigned char *h, int b0, int b1,
                    int p0, int p1, int p2, int p3,
                    int a0, int a1, int a2, int a3,
                    int dlen) {
  memset(h, 0, 12);
  h[0] = b0; h[1] = b1;
  h[2] = p0; h[3] = p1; h[4] = p2; h[5] = p3;
  h[6] = a0; h[7] = a1; h[8] = a2; h[9] = a3;
  h[10] = dlen & 0xff; h[11] = (dlen >> 8) & 0xff;
}

int main(void) {
  libusb_context *ctx = NULL;
  libusb_device_handle *h = NULL;
  int r;

  r = libusb_init(&ctx);
  if (r < 0) { printf("libusb_init failed: %d\n", r); return 1; }

  h = libusb_open_device_with_vid_pid(ctx, VID, PID);
  if (!h) { printf("device not found\n"); return 1; }
  printf("device opened\n");

  if (libusb_kernel_driver_active(h, 0) == 1)
    libusb_detach_kernel_driver(h, 0);
  r = libusb_claim_interface(h, 0);
  if (r < 0) { printf("claim failed: %d\n", r); return 1; }
  printf("interface claimed\n\n");

  unsigned char hdr[12];

  /* 1) SELECT_OS with different params, then GET_ENROLL_NUM */
  for (int os = 0; os <= 3; os++) {
    printf("--- SELECT_OS param=%d ---\n", os);
    cmd_hdr(hdr, 0x05, 0x13, os, 0, 0, 0, 0, 0, 0, 0, 0);
    run_cmd(h, "select_os", hdr, 0);
    cmd_hdr(hdr, 0x45, 0x0d, 0, 0, 0, 0, 0, 0, 0, 0, 2);
    run_cmd(h, "get_enroll_num", hdr, 1);
    usleep(200000);
  }

  /* 2) GET_TEMPLATE with various data lengths */
  int lens[] = {0, 35, 350, 700};
  for (int li = 0; li < 4; li++) { int dl = lens[li];
    printf("--- GET_TEMPLATE data_len=%d ---\n", dl);
    cmd_hdr(hdr, 0x45, 0x0e, 0, 0, 0, 0, 0, 0, 0, 0, dl);
    run_cmd(h, "get_template", hdr, 1);
    usleep(200000);
  }

  /* 3) basic sensor commands */
  cmd_hdr(hdr, 0x05, 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0);  /* enroll begin */
  run_cmd(h, "enroll_begin", hdr, 0);
  cmd_hdr(hdr, 0x05, 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0);  /* start capture */
  run_cmd(h, "start_capture", hdr, 0);

  libusb_release_interface(h, 0);
  libusb_close(h);
  libusb_exit(ctx);
  printf("\ndone\n");
  return 0;
}
