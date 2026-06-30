# Future Work: Concurrency, Security & Optimization

**Status:** DEFERRED — do not implement yet.
**Trigger to start:** after the current design is validated against real RTL-SDR hardware
and confirmed to decode/forward live SAME/EAS alerts correctly.

This document records findings from a code review of the current tree
(`git 1a0e3ae`). The goal is to lock in *working* behavior on hardware first, then
apply these changes without re-deriving the analysis. Nothing here is a known crash
in normal single-client, single-threaded-load operation; these are correctness-under-
concurrency, hardening, and throughput improvements.

---

## 0. Verified-safe (do NOT re-investigate)

These were checked during review and are correctly bounded. Listed so future work
doesn't chase false positives:

- **SAME parser** (`same.c`): every `memcpy` (lines 25, 31, 39) is guarded by a
  preceding `strlen(p) >= N` check; input `raw` is always NUL-terminated by the
  caller. Safe.
- **EAS message buffer** (`eas.c`): `msg_buf[EAS_NUM_BURSTS][EAS_MAX_MSG_LEN + 1]`
  (= `[3][269]`). The write guard `msg_pos < EAS_MAX_MSG_LEN` plus the `+1` row size
  means the terminator write at `msg_buf[..][msg_pos]` (line 123) is in-bounds. Safe.
- **`same.c:10`** `strncpy(msg->raw, raw, EAS_MAX_MSG_LEN)` — preceded by `memset(msg,0)`
  and source is `<= 268` chars, so the result is always NUL-terminated. Safe.
- **Control `arg[8]`** with `sscanf(" %d %7s")` — max 7 chars + NUL. Safe.
- **`gate.c:39`** `frame_buf[frame_pos++]` — bounded by the `frame_pos >= USRP_SAMPLES`
  reset on line 40. Safe *single-threaded* (see C1 for the concurrent case).
- Channel indices from both the TCP command and the config section name are
  range-checked before use (`control.c:46`, `config.c:107`).

General hygiene is good: `snprintf` everywhere (no `sprintf`/`strcpy`/`strcat`),
`+1`-sized string buffers, length guards before copies.

---

## 1. Concurrency (highest priority)

Three threads share state: **capture** (RTL-SDR async callback), **control** (TCP),
**main** (signal handler + 10 Hz `gate_tick`).

### C1 — `control->lock` does not actually protect the gate  [HIGH]

The mutex is held in only 3 spots (`gate_set_passthrough`, `gate_get_state`,
`control_broadcast`), but the real mutators of `gate_t` never take it:

| Function | Thread(s) | Mutates | Locked? |
|---|---|---|---|
| `gate_alert` (gate.c:47) | capture | `state`, `frame_pos`, `current_alert` | no |
| `gate_process_audio` (gate.c:33) | capture | `state`, `frame_buf`, `frame_pos` | no |
| `gate_eom` (gate.c:66) | capture + main | `state`, `frame_pos`, usrp `seq` | no |
| `gate_tick` (gate.c:127) | main | `state` | no |
| `gate_set_passthrough` (gate.c:92) | control | `state`, `frame_pos` | yes (races the rest) |

Consequences: a `PASSTHROUGH` command can reset `frame_pos` and call
`usrp_send_audio` while the capture thread is mid-frame on the same gate; both paths
race the non-atomic `conn->seq++` (`usrp.c:41`), producing duplicate/skipped USRP
sequence numbers and a corrupted state machine. This controls transmitter PTT, so
state corruption is the worst-case bug.

**Fix options:**
- (Pragmatic) Add `pthread_mutex_t lock;` to `gate_t`. Take it in *every* accessor.
  In the hot path, lock once per 160-sample USRP frame (around the send block in
  `gate_process_audio`), **not** per sample.
- (Cleaner) Make the gate single-writer: have `control` push commands into a queue
  that the capture thread drains, so only the capture thread mutates the gate.

### C2 — `client_fds[]` / `num_clients` data race  [HIGH]

`control_broadcast` (control.c:264-267) iterates the client array *under* the lock,
but `control_thread` adds (line 147) and swap-removes (lines 163, 180, 196) clients
*without* it. `control_broadcast` runs on the capture and main threads, concurrent
with the control thread compacting the array → `write()` to a closed/garbage fd, or
skipped/double-handled clients.

**Fix:** take `ctl->lock` in `control_thread` around the `accept` insert and every
swap-remove. Keep broadcast's critical section minimal (snapshot fds under lock,
then write).

### C3 — interleaved writes to the same client socket  [MEDIUM]

Even after C2, command *responses* (control thread) and async *events*
(`control_broadcast` from capture/main) `write()` to the same client fd
(control.c:18). Per-call TCP writes can interleave → corrupted protocol lines.

**Fix:** single-writer output (enqueue events for the control thread to flush), or a
per-client write lock.

### C4 — non-atomic shutdown flags  [LOW]

`cap->running` (capture.c:12) and `ctl->running` (control.c:121) are plain `int`,
written by one thread and spin-read by another; the read may be hoisted by the
optimizer.

**Fix:** `volatile sig_atomic_t` or `<stdatomic.h>` `atomic_int`. (Global `running`
in main.c:27 is already `volatile`, which is adequate for the signal flag.)

---

## 2. Security

### S1 — control port has no authentication  [MEDIUM]

Anyone who can reach the control port can issue `PASSTHROUGH N ON`, keying the
transmitter and forwarding received audio to AllStarLink (control.c:71 → gate.c:92).
Default bind is `127.0.0.1` (good), but nothing prevents a config from binding
`0.0.0.0`, and there is no token/ACL.

**Fix:** document loopback-only prominently in `README`/`config.ini`; reject non-
loopback binds unless an explicit opt-in flag is set; optionally add a shared-secret
token handshake.

### S2 — radio bytes flow unescaped into JSON  [LOW]

`process_bit` stores arbitrary demodulated bytes (`eas.c:66,80`) into `msg_buf`,
which becomes `msg->raw`, emitted as `"raw":"%s"` (same.c:152) to control clients. A
garbage decode containing `"` or `\` injects into the JSON event stream.

**Fix:** JSON-escape `raw`/`callsign`, or validate the SAME charset `[A-Z0-9+\-]`
before formatting.

### S3 — `trim()` undefined behavior on empty value  [LOW]

`config.c:15` computes `s + strlen(s) - 1`; for an empty value (e.g.
`event_blacklist =`) this forms a pointer before the buffer (UB, harmless in
practice).

**Fix:** `if (*s) { ... }` guard before the trailing-trim.

### S4 — no range validation on config integers  [LOW]

`atoi` → `(uint16_t)` casts (config.c:138,149) silently wrap ports; `atoi` returns 0
on garbage. Trusted local file, so low risk.

**Fix:** `strtol` with explicit range/`errno` checks.

---

## 3. Optimization

Hot path is `capture_callback` → 7× `fir_chan_process` at **2.4 MS/s**
(~16.8M calls/s).

### O1 — power-of-two modulo on signed ints  [HIGH value, trivial]

`FIR_TAPS == 128` and all ring buffers are 128, but the index modulo runs on
**signed** `int`, so the compiler cannot lower `% 128` to a bitmask (negative
handling) and emits a real division — in the hottest loops:
`dsp.c:62`, **`dsp.c:76`**, `dsp.c:120`, `dsp.c:129`, `eas.c:33`, `eas.c:178`.

**Fix:** use `& (FIR_TAPS - 1)` / `& 127`, or make the indices `unsigned`.

### O2 — remove the inner-loop modulo entirely  [MEDIUM]

The FIR MAC (`dsp.c:75-84`) recomputes `k = (idx+i) % FIR_TAPS` every tap. Split into
two contiguous runs (`idx..N-1`, then `0..idx-1`); add `restrict` to the pointers so
the compiler can vectorize the complex MAC.

### O3 — Makefile build flags  [MEDIUM]

Currently `-O2` (Makefile:2). For float DSP add `-O3 -funroll-loops`, and (if reduced
strictness is acceptable for the discriminator/correlator math) `-ffast-math`. Offer
`-march=native` behind an opt-in variable (it pins the binary to the build CPU).

### O4 — replace per-sample `cosf`/`sinf` in EAS  [LOW–MEDIUM]

`eas.c:166-169` calls 4 transcendentals per audio sample (~1.3M/s). Replace the
reference oscillators with a complex-rotation recurrence (4 mults + periodic
renormalize). Lower priority — this is post-channelizer at 48 kHz, not 2.4 MS/s.

### O5 — hoist the `enabled` check  [LOW]

`main.c:86` branches on `channels[ch].config->enabled` every sample; snapshot an
enabled-mask once per buffer.

> Intentionally **not** recommended: FFT/polyphase channelizer. The README mandates
> "no FFTW," and the current channelizer already early-returns before the MAC on
> 49/50 samples, so it is reasonable as-is.

---

## 4. Phased implementation plan (post-hardware-validation)

Each phase is independently shippable. Run `make test` after every phase; add
ThreadSanitizer (`-fsanitize=thread`) for Phase 1.

**Phase 0 — guardrails (do first)**
- [ ] Add a concurrency stress test: drive `PASSTHROUGH`/`STATUS` from a TCP client
      while alerts are injected, under ThreadSanitizer.
- [ ] Capture a baseline DSP throughput number (samples/s the capture callback
      sustains) so O1–O3 gains are measurable.

**Phase 1 — concurrency correctness (C1, C2)**
- [ ] Add `pthread_mutex_t` to `gate_t`; lock all accessors (per-frame in the hot
      path, not per-sample).
- [ ] Lock the client-array mutations in `control_thread`; snapshot under lock in
      `control_broadcast`.
- [ ] Verify clean under ThreadSanitizer.

**Phase 2 — DSP throughput (O1, then O2, O3)**
- [ ] `% 128` → `& 127` (or unsigned indices) in the listed loops. Re-measure.
- [ ] Split the FIR MAC into contiguous runs + `restrict`. Re-measure.
- [ ] Bump Makefile flags; gate `-march=native` behind a variable. Re-measure.

**Phase 3 — control-channel hardening (C3, C4, S1)**
- [ ] Single-writer control output (event queue) — supersedes the C3 write race.
- [ ] Atomic `running` flags.
- [ ] Enforce/document loopback-only control port; optional token.

**Phase 4 — polish (S2, S3, S4, O4, O5)**
- [ ] JSON-escape radio-derived fields.
- [ ] `trim()` empty-string guard; `strtol` range checks.
- [ ] EAS oscillator recurrence; hoist `enabled` mask.

---

## Priority summary

1. **C1 + C2** — gate mutex + client-array locking (PTT-control correctness).
2. **O1** — modulo→bitmask in DSP/correlator (free throughput).
3. **S1** — enforce/document loopback-only control port.
4. **C3, C4, O2, O3** — single-writer output, atomic flags, build flags.
5. **S2–S4, O4, O5** — hardening + polish.
