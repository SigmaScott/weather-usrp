# weather-usrp

NOAA Weather Radio SAME/EAS alert gate for AllStarLink.

Monitors all 7 NOAA Weather Radio frequencies simultaneously using a single RTL-SDR dongle, decodes SAME/EAS alerts, filters by FIPS code, and forwards matching alert audio over the USRP protocol (chan_usrp) via UDP.

## Features

- Single RTL-SDR dongle captures all 7 NWR channels (162.400–162.550 MHz)
- FIR channelizer with frequency-shifted taps (no FFTW dependency)
- FM demodulation and decimation per channel
- AFSK correlator-based EAS/SAME decoder with 2-of-3 burst voting
- FIPS code filtering with P-digit stripping
- Per-channel USRP output to separate AllStarLink destinations
- Gate state machine (IDLE / ALERT / PASSTHROUGH) with automatic PTT control
- TCP control interface for runtime status and passthrough commands

## Building

### Dependencies

- librtlsdr-dev
- libusb-1.0-0-dev
- build-essential (gcc, make)

```sh
sudo apt install librtlsdr-dev libusb-1.0-0-dev build-essential
```

### Compile

```sh
make
```

### Run Tests

```sh
make test
```

## Configuration

Edit `config.ini`:

```ini
[sdr]
device_index = 0
gain = -1          ; -1 for automatic gain
ppm = 0

[control]
host = 127.0.0.1
port = 5555

[channel0]
frequency = 162400000
usrp_host = 127.0.0.1
usrp_port = 34001
fips = 048453,048491
enabled = 1
```

Each `[channelN]` section (0–6) maps to one NWR frequency and routes alert audio to a specific USRP destination. The `fips` field is a comma-separated list of 5 or 6-digit FIPS codes to match (P-digit is stripped for comparison).

## Usage

```sh
./weather-usrp config.ini
```

## AllStarLink Integration

Configure `chan_usrp` in your `rpt.conf`:

```
rxchannel = USRP/127.0.0.1:34001:32001
```

Where `34001` is the port weather-usrp sends audio to, and `32001` is the return port (unused by weather-usrp but required by chan_usrp).

## TCP Control Interface

Connect with `nc` or `telnet` to the control port:

```
STATUS              - JSON status of all channels
PASSTHROUGH N ON    - Enable passthrough on channel N (audio forwarded without SAME filtering)
PASSTHROUGH N OFF   - Disable passthrough on channel N
QUIT                - Disconnect
```

## Architecture

```
RTL-SDR (2.4 MS/s @ 162.482 MHz)
  │
  ├─ FIR Channelizer (×7, freq-shifted taps, decimate to 48 kHz IQ)
  │     │
  │     ├─ FM Discriminator → 48 kHz audio
  │     │     │
  │     │     ├─ EAS Decoder (AFSK correlator → byte framer → 2-of-3 voting)
  │     │     │     │
  │     │     │     └─ SAME Parser → FIPS Match → Gate ALERT
  │     │     │
  │     │     └─ Decimator (48k → 8k) → Gate → USRP UDP
  │     │
  │     └─ (per channel)
  │
  └─ TCP Control Server
```

## USRP Protocol

Each audio frame is 352 bytes: 32-byte header + 320 bytes of signed 16-bit LE audio (160 samples at 8 kHz). The header contains the "USRP" magic, sequence number, keyup flag (PTT), and type field. Type 0 = voice, keyup 1 = transmit.

## License

GPL v2
