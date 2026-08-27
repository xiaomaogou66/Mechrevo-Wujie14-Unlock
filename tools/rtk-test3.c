/* test3: isolate the get_device_info ctrl + warmup variables */
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
  int m = n < 24 ? n : 24;
  for (int i = 0; i < m; i++) printf("%02x ", b[i]);
  printf("\n");
}

static int run_cmd(libusb_device_handle *h, const char *name, unsigned char *hdr) {
  int r, t;
  unsigned char buf[2048];
  int cmdtype = (hdr[0] & 0xC0) >> 6;
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
  printf("== %s status=0x%02x\n", name, buf[0]);
  return buf[0];
}

static void cmd_hdr(unsigned char *h, int b0, int b1, int p0, int dl) {
  memset(h, 0, 12);
  h[0] = b0; h[1] = b1; h[2] = p0;
  h[10] = dl & 0xff; h[11] = (dl >> 8) & 0xff;
}

static void reset_device(libusb_device_handle *h) {
  libusb_reset_device(h);
  usleep(300000);
  libusb_claim_interface(h, 0);
}

int main(void) {
  libusb_context *ctx = NULL;
  libusb_device_handle *h = NULL;
  unsigned char hdr[12], buf[8];
  int r;

  libusb_init(&ctx);
  h = libusb_open_device_with_vid_pid(ctx, VID, PID);
  if (!h) { printf("device not found\n"); return 1; }

  /* ============ A) NO ctrl info, single select_os ============ */
  printf("\n########## A) no get_device_info, select_os(1) once ##########\n");
  reset_device(h);
  cmd_hdr(hdr, 0x05, 0x13, 0x01, 0);
  run_cmd(h, "select_os", hdr);
  cmd_hdr(hdr, 0x45, 0x0d, 0, 2);
  run_cmd(h, "get_enroll_num", hdr);
  cmd_hdr(hdr, 0x45, 0x0e, 0, 350);
  run_cmd(h, "get_template", hdr);

  /* ============ B) no ctrl info, 4x select_os warmup (like test1) ============ */
  printf("\n########## B) no ctrl info, 4x select_os warmup ##########\n");
  reset_device(h);
  for (int os = 0; os < 4; os++) {
    cmd_hdr(hdr, 0x05, 0x13, os, 0);
    run_cmd(h, "select_os", hdr);
    cmd_hdr(hdr, 0x45, 0x0d, 0, 2);
    run_cmd(h, "get_enroll_num", hdr);
    usleep(200000);
  }
  cmd_hdr(hdr, 0x45, 0x0e, 0, 350);
  run_cmd(h, "get_template", hdr);

  /* ============ C) WITH ctrl info, 4x warmup ============ */
  printf("\n########## C) with ctrl info + 4x warmup ##########\n");
  reset_device(h);
  r = libusb_control_transfer(h, 0x80 | 0x40, 0x07, 0x000D, 0x0000, buf, 8, 1000);
  printf("  get_device_info ctrl r=%d: %02x %02x %02x %02x %02x %02x %02x %02x\n",
         r, buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);
  for (int os = 0; os < 4; os++) {
    cmd_hdr(hdr, 0x05, 0x13, os, 0);
    run_cmd(h, "select_os", hdr);
    cmd_hdr(hdr, 0x45, 0x0d, 0, 2);
    run_cmd(h, "get_enroll_num", hdr);
    usleep(200000);
  }
  cmd_hdr(hdr, 0x45, 0x0e, 0, 350);
  run_cmd(h, "get_template", hdr);

  libusb_release_interface(h, 0);
  libusb_close(h);
  libusb_exit(ctx);
  printf("\ndone\n");
  return 0;
}
