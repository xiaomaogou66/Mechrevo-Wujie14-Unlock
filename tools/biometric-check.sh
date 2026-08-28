#!/usr/bin/env bash
# greetd login biometric check — fingerprint PRIMARY, face as backup.
# Runs as root via pam_exec. PAM_USER is exported by pam_exec.
#
# Flow:
#   1. start fprintd-verify (fingerprint)
#   2. give it ~1.5s head start; if it matches -> unlock
#   3. otherwise ALSO start howdy (face) and wait for either

USER="${PAM_USER:-}"
if [ -z "$USER" ]; then
  exit 1
fi

# Only for users with an enrolled face model (skips greeter-session setup)
if [ ! -f "/usr/lib/security/howdy/models/$USER.dat" ]; then
  exit 1
fi

# --- fingerprint first (primary) ---
fprintd-verify "$USER" >/dev/null 2>&1 &
FP_PID=$!

# give fingerprint a head start (~1.5s)
for _ in $(seq 1 30); do
  if ! kill -0 "$FP_PID" 2>/dev/null; then
    wait "$FP_PID"
    rc=$?
    if [ "$rc" -eq 0 ]; then
      exit 0
    fi
    FP_PID=0
    break
  fi
  sleep 0.05
done

# --- fingerprint didn't win; start face too ---
if [ "$FP_PID" -ne 0 ] || [ "$FP_PID" = "0" ]; then
  :
fi

python3 /usr/lib/security/howdy/compare.py "$USER" >/dev/null 2>&1 &
FC_PID=$!

while [ "$FP_PID" != "0" ] || kill -0 "$FC_PID" 2>/dev/null; do
  if [ "$FP_PID" != "0" ] && ! kill -0 "$FP_PID" 2>/dev/null; then
    wait "$FP_PID"
    rc=$?
    if [ "$rc" -eq 0 ]; then
      kill "$FC_PID" 2>/dev/null
      exit 0
    fi
    FP_PID=0
  fi
  if ! kill -0 "$FC_PID" 2>/dev/null; then
    wait "$FC_PID"
    rc=$?
    if [ "$rc" -eq 0 ]; then
      kill "$FP_PID" 2>/dev/null
      exit 0
    fi
    break
  fi
  sleep 0.05
done

exit 1
