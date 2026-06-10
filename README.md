# STANAG 4285 Standalone Modem
**Claude AI was used in this project!**

A C program that transmits and receives STANAG 4285 HF serial-tone modem
signals using live audio (microphone/speaker) or WAV files.

Built from C.H. Brain G4GUO's original source, with the OSS `/dev/dsp`
and IPC shared-memory layer replaced by [miniaudio](https://miniaud.io)
(single-header, cross-platform audio) and plain stdin/stdout I/O.

---

## Build

```
make
```

Requires GCC, libm, libpthread.  `miniaudio.h` must be present in the
same directory (already included).

Run the built-in loopback tests:

```
make test
```

---

## Usage

### Transmit

```
stanag4285 -tx [-mode <MODE>] [-wav <outfile.wav>]
```

Reads raw bytes from **stdin**, HDLC-frames them, modulates to a STANAG
4285 waveform centred at 1800 Hz, and either:
- plays the audio through the **default speaker** (live mode), or
- writes a **WAV file** (mono, 16-bit, 9600 Hz) when `-wav` is given.

### Receive

```
stanag4285 -rx [-mode <MODE>] [-wav <infile.wav>]
```

Reads audio from the **default microphone** (or a WAV file with `-wav`),
demodulates, HDLC-deframes, and writes the recovered bytes to **stdout**.

---

## Modes

| Mode string | Rate (bps) | Interleaver | Modulation |
|-------------|-----------|-------------|-----------|
| `75n`       | 75        | None        | BPSK      |
| `75s`       | 75        | Short       | BPSK      |
| `75l`       | 75        | Long        | BPSK      |
| `150n/s/l`  | 150       | None/S/L    | BPSK      |
| `300n/s/l`  | 300       | None/S/L    | BPSK      |
| `600n/s/l`  | 600       | None/S/L    | QPSK      |
| `1200n/s/l` | 1200      | None/S/L    | QPSK      |
| `1200u`     | 1200      | None        | 8-PSK uncoded |
| `2400n/s/l` | 2400      | None/S/L    | 8-PSK     |
| `2400u`     | 2400      | None        | 8-PSK uncoded |
| `3600u`     | 3600      | None        | 8-PSK uncoded |

Default: **600s** (600 bps, short interleaver, QPSK with FEC).

---

## Examples

### WAV loopback (offline test)

```bash
echo "CQ CQ DE W1ABC" | ./stanag4285 -tx -mode 1200l -wav out.wav
./stanag4285 -rx -mode 1200l -wav out.wav
```

### Live over-the-air

```bash
# Terminal 1 — transmit text typed on stdin
./stanag4285 -tx -mode 600s

# Terminal 2 — receive and print to stdout
./stanag4285 -rx -mode 600s
```

The modem output is centred at **1800 Hz**, suitable for feeding into an
SSB transceiver's mic/line input and treating the receiver audio output as
the modem input.

### Pipe from a file

```bash
cat message.txt | ./stanag4285 -tx -mode 2400l -wav tx.wav
sox tx.wav -r 48000 tx_48k.wav   # resample for your soundcard if needed
```

---

## Signal path notes

**TX:** stdin → HDLC frame (CRC-32, bit stuffing) → convolutional FEC
(rate 1/2, K=7) → interleaver → 8-PSK/QPSK/BPSK scrambled symbols →
RRC pulse shaping → 1800 Hz carrier → 16-bit PCM at 9600 Hz.

**RX:** 16-bit PCM at 9600 Hz → three-channel frequency search (1765 /
1800 / 1835 Hz) → preamble correlator → Doppler correction → RRC matched
filter → Kalman DFE equalizer → BPSK/QPSK/8-PSK demodulator → soft-output
Viterbi decoder → de-interleaver → HDLC deframe → stdout.

---

## Audio device selection

miniaudio uses the OS default audio device.  To choose a specific device,
set the environment variable `MA_DEFAULT_DEVICE` or pass the device name
in your system's audio routing (PulseAudio/ALSA sink/source name, etc.).

---

## Notes

- The sample rate is **9600 Hz**.  Most modern soundcards support this
  natively; miniaudio will negotiate the closest supported rate otherwise.
- The long interleavers (`l` suffix) require significantly more data to
  fill the interleaver buffer before the decoder can start producing
  output.  This is normal STANAG 4285 behaviour.
- There is no 8N1 or any external serial framing — the modem's own
  preamble and HDLC structure handle synchronisation. (set decoder software framing to "Synchronous")
- Original modem code by C.H. Brain G4GUO (circa 2000).
