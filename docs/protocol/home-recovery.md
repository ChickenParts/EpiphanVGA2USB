# Original HOME short-frame adaptation

Static audit, 2026-09-28. Vendor ELF 3.33.0.17 SHA256
`536b9be25b310d57a77d2888fe83cd2e499417dbb963129edc90357cf9736dbb`.
All addresses are section relative. Repository comparison was against
this repository at commit
`13964b0dbb9753305ae15fd10fbc17b28ac00b6a`, specifically
`src/capture_mode.c` and `src/home_frame.c`.
No USB requests, target changes, production edits, builds, resets, or module
loads were performed. The reported first hardware capture returned 1536 bytes
for a 720×400 request whose full wire size is 614304 bytes.

## Result and next defensible parameter change

The current first B4 payload matches the proven fresh original HOME defaults.
There is a concrete vendor adaptation missing from a single-capture path:
after **three failing frames**, raise vertical phase count V from 2 to the
next divisor of the full active height. For height400, 3 does not divide the
height, so V becomes4, lines per phase become100, and byte25 becomes4.
The changes to the 31-byte B4 payload are exactly:

| Zero-based wire offsets | Initial | Next adapted request |
|---|---|---|
| 18..19, BE16 vertical phases | 00 02 | 00 04 |
| 20..21, BE16 lines per phase | 00 c8 | 00 64 |
| 25, phase count byte | 02 | 04 |

For measured status SS=2, the next payload is:

```text
03 84 02 00 08 14 1f 70 70 70 20 20 20 03 02 d0 01 90 00 04 00 64 00 9e 24 04 01 80 80 00 00
```

This preserves width720, active height400, P=1, all analog controls and measured
status. Vendor sender remains OUT40/B4/value0/index0/length31. It changes the
capture ordering/scheduling, not the amount of full-frame data. Expected wire
size remains614304 bytes and decoded output remains576000 bytes. No evidence
supports treating1536 bytes as a full frame or reducing expected height to1.

The exact adaptation is helper `.text+0x221e0`; call from completed-frame
statistics is 0x22a56–0x22a5c. The first two failing calls do not invoke it.
The third failing call updates the parameters but returns its original error;
the changed parameters apply to the **next** frame request, not an immediate
extra transfer inside the failed call.

## Correct original dispatch

The original channel `.rodata+0x11d1c0` selects `grab_protocol_home` at+0x40.
That constructor is `.text+0x23370`, and it passes ops `.rodata+0x1a3e60` at
0x233af–0x233d5. These ops select:

| Ops offset | Function |
|---|---|
| +0x28 | .text+0x20d10, frame-size calculation |
| +0x30 | .text+0x211a0 |
| +0x38 | .text+0x22030, full wire-size calculation |
| +0x40 | .text+0x23100, original HOME capture/crop wrapper |
| +0x50 | .text+0x220a0, HOME reset/default phases |

The nearby `.rodata+0x1a3f20` table and functions0x241c0/0x24da0/0x24fe0 are
another protocol path. Their decompression reset/retry flag0x40000000 is not
the original HOME adaptation and should not be copied onto original5555:1110
because anonymous functions have nearest-symbol labels containing HOME.

The full-crop wrapper0x23100 recognizes crop x=y=0,width=activewidth,
height=activeheight at0x23141–0x2314b and0x23200–0x2320e, then calls0x22290
at0x2321e. For a requested crop it temporarily captures the whole active frame
and crops in software; it does not reduce original wire height to ROI height.

## Why1536 bytes is an error

Actual original read/deorder entry is0x22290. It calls
`grab_io_read_image_lines(vgrabber, gp, 0x400, NULL, 0, &linesbuf)` at
0x222b7–0x222d8. A negative receive result follows the common statistics path
via0x22922. A nonnegative receive result computes full expected wire bytes at
0x222e5–0x22344, then calls`lines_buf_total` at0x2234c.

Let W=gp+0x0e, V=gp+0x12, L=gp+0x14, P=gp+0x1a, and
Q=P*(1+((gp[2]&0x10)!=0)). For the current valid progressive request:

```text
segments = V * L * Q             # zero product falls back to1 before Q
sample_width = floor(W / Q)
stride = 512 * ceil(2 * sample_width / 512)
expected = (segments - 1) * stride + 2 * sample_width
```

W720,V2,L200,P1,gp2=2 gives segments400,stride1536,last-useful1440,
expected614304. The original length check permits only:

```text
expected - 512 < actual_total <= expected
```

At0x22355–0x22358, actual above expected rejects. At0x2235e–0x22366, actual
plus512 less than or equal to expected rejects. Both branches lead to
0x22a4b, setting return`-32`. Thus1536 is rejected before pixel conversion.
No loop around the image read accumulates another frame after this check.
Increasing V while preserving V*L leaves the byte expectation unchanged.

## Exact phase search and upper limit

Helper0x221e0:

```text
candidate = gp[25] + 1
if candidate > 254: return0
while height % candidate != 0:
    candidate += 1
    if candidate == 255: return0
gp[25] = candidate
gp.u16_at18 = candidate
gp.u16_at20 = height / candidate
reset HOME error statistics
set HOME direction = +1
set HOME deadline = current_time + adjustment_interval
return1
```

Evidence: start/count-limit0x221e5–0x221f2; divisor search0x22207–0x2221a;
three writes0x2221c–0x22229. There is no partial gp write on exhaustion.
The search uses **active full height** atgp+0x10, not prior lines per phase or
crop height. It never selects255 or greater.

For height400, increasing values are:

```text
2,4,5,8,10,16,20,25,40,50,80,100,200
```

After200 the search exhausts without changing gp; height400 itself is outside
the permitted limit. For comparison, height480 starts2 then3,4,5,6,8,...,240.
The binary does not contain a global maximum number of capture attempts.
Once phase search is exhausted, another failed call still returns an error;
there is no evidence for an indefinite user-facing retry loop. A tool may
stop after bounded attempts and report exhaustion, preserving each requested
V and actual byte count. Current task only establishes the first V4 change;
hardware success for any value remains unverified.

## Failure-rate state and timing

The protocol constructor zeroes its384-byte instance at0x233a5–0x233cf.
It initializes the qword at+0x68 to`0x138800000000` at0x23405–0x2340f,
meaning direction0 at+0x68 and adjustment interval5000 at+0x6c.
HOME reset0x220a0 sets the same interval/direction, clears+0x70/+0x74/+0x78,
and sets deadline+0x60=current_time+interval. It then sets V=2 for progressive
or4 for flags0x20, L=height/V, andgp25=V (0x220e4–0x22119).

Completed capture bookkeeping at0x2296f–0x22a3b tracks a256-entry ring:
index+0x70, sample count+0x74, failing count+0x78, outcome bytes+0x7c.
Byte1 means return0 (success); byte0 means nonzero return (failure).
For more than2 samples:

* All samples failing raises V immediately (0x229ad–0x229b3).
* Once samples>20, calculate integer`failure_percent=100*errors/samples`.
  Original ordinary thresholds are low3/high6; high-rate threshold is
  strict`failure_percent > 6` (0x229be–0x229c6), which raises V.
* Special board IDs0x1120,0x3344,0x3383,0x3337,0x4004 use low6/high12 instead
  (0x2292f–0x2296f). Original0x1110 is not in this special list. The all-failed
  three-sample trigger does not depend on which thresholds are selected.

Raising V clears statistics at0x22231–0x22239. If the previous direction
was-1, it doubles interval but caps it at60000 (0x2222d–0x22254), then
sets direction+1 and a new deadline. There is no deadline condition before
raising after three all-failed frames.

The low-error branch after more than20 samples additionally requires
`failure_percent < low_threshold`, gp+0x80 bit0 clear, and deadline passed
(0x229cc–0x229ec,0x22dfc–0x22e04). It searches downward fromgp25-1 for the
next divisor of height, never below progressive2/interlaced4
(0x22e0a–0x22e5c). It synchronizes the same three gp fields, clears statistics,
sets direction-1, and advances the deadline (0x22e62–0x22e98). Repeated
downward adjustment sets the interval back to5000. If already at the minimum,
it replaces the deadline withINT64_MAX (0x230bd).

These timing values are stated in`os_get_time` units; the bounded static audit
does not depend on asserting a new unit conversion. They are internal adaptive
deadlines, not proposed process timeouts.

## ADC create/reset audit: no missing AD9884A reset found

Original model selects`adc_AD9884A_create` via relocation at
`.rodata+0x11d218`. Constructor0x11480 passes ops`.rodata+0x1a3620` to
`adc_default_create` at0x11489–0x11493. The default constructor only allocates
48 bytes and initializes instance fields; writes at0xeaa3/0xeaab clear the
qwords at+0x20/+0x28, including calibration word+0x24. It sends no I2C request.

Original AD9884A ops has only mode callback relocation+0x18->0x11160;
the callback slot+0x20 is zero. Raw ops+0x10 (ADC register-image length) is
zero and+0x30 (image generation) is null. The USB ADC I/O ops
`.rodata+0x17cca0` initialization callback+0x10 is also null. `adc_init`0xf320
first calls that I/O init only when nonnull (0xf350–0xf361), and its EEPROM
calibration path requires nonzero instance word+0x24 (0xf366–0xf36d), which
is zero here. Consequently no additional original ADC reset/init write is
established by this create/init flow.

The tempting nearby function0x114a0 writes I2C register0x12=0 then1 and
sleeps250 (0x114ea–0x11544). Its wrapper0x11560 is selected by **different**
ADC ops`.rodata+0x1a36a0`+0x20 atrelocation0x1a36c0. The original ops table
does not point to it. Applying those writes based on nearest-symbol output
would be unsupported.

`grab_protocol_init`0x1bd30 stores callbacks/vgrabber and creates a software
spinlock; it contains no device request. HOME create/reset is likewise a
software state/phase initializer. No evidence in this bounded audit requires
an extra B0, an ADC reset, or a format change before trying the proven phase
adaptation. Firmware-side B4 handler analysis and preserving the1536 received
bytes remain useful separate leads if the adapted request is still short.

## Offline payload reproduction

`home-retry-plan.rb` accepts exactly31 hex bytes and emits the next vendor
divisor-based payload. It never opens a USB device. Example:

```sh
ruby home-retry-plan.rb '0384020008141f7070702020200302d00190000200c8009e24020180800000'
```

It verifies matching BE16/count fields and leaves all other bytes unchanged.
For default720×400 it emits the V4 payload above; phase200 reports exhaustion.
This is a proposed request plan, not execution or a claim that capture is fixed.
