<img width="1408" height="768" alt="Weather-usrp2" src="https://github.com/user-attachments/assets/4142969a-09b8-4590-818b-fc89ea568f37" />



# weather-usrp

NOAA Weather Radio EAS/SAME alert gate for AllStarLink.

Monitors all 7 NOAA Weather Radio frequencies simultaneously using a single RTL-SDR dongle, decodes EAS/SAME alerts, filters by FIPS code and event type, and forwards matching alert audio to AllStarLink nodes via the USRP protocol (chan_usrp).

## Features

- Simultaneous monitoring of all 7 NWR channels (162.400–162.550 MHz) with one RTL-SDR
- Real-time EAS/SAME decoding with configurable burst voting (1-of-3 through 3-of-3)
- FIPS code filtering — only alerts for your area trigger audio forwarding
- Event blacklist — suppress test codes (RWT, RMT, DMO) or any unwanted event types
- Per-channel USRP output — route each NWR channel to a different AllStarLink node
- Passthrough mode — stream continuous audio from any channel on demand
- Automatic PTT control — keys and unkeys based on alert state
- TCP control interface for runtime status and commands
- 10-minute alert timeout safety net

## Requirements

### Hardware

- RTL-SDR dongle (RTL-SDR Blog V3/V4 or Nooelec SMArTee v2 recommended)
- Antenna suitable for 162 MHz (scanner antenna, discone, or dedicated NWR antenna)

### Software

- Linux (Debian/Ubuntu-based recommended)
- librtlsdr-dev
- libusb-1.0-0-dev
- build-essential (gcc, make)

```sh
sudo apt install librtlsdr-dev libusb-1.0-0-dev build-essential
```

## Building

```sh
make
```

Run tests:

```sh
make test
```

## Configuration

Edit `config.ini`:

```ini
[sdr]
device_index = 0
gain = -1               ; -1 for automatic gain control
ppm = 0                 ; frequency correction in PPM
audio_gain = 4.0        ; audio output level multiplier
eas_min_bursts = 2      ; 1=fire on first burst, 2=require 2-of-3 match (recommended)
; center_freq = 162482500  ; override SDR center frequency (Hz)

[control]
host = 127.0.0.1
port = 5555

[channel0]
frequency = 162400000
usrp_host = 127.0.0.1
usrp_port = 34001
fips = 048453,048491
event_blacklist = RWT,RMT,DMO
enabled = 1
```

### SDR Settings

| Key | Default | Description |
|-----|---------|-------------|
| `device_index` | 0 | RTL-SDR device number (if multiple dongles) |
| `gain` | -1 | Tuner gain in tenths of dB, or -1 for automatic |
| `ppm` | 0 | Frequency correction (most TCXO dongles need 0) |
| `audio_gain` | 1.0 | Audio output multiplier (4.0–15.0 typical for NBFM) |
| `eas_min_bursts` | 2 | Bursts required before alert fires (1–3) |
| `center_freq` | 162482500 | SDR center frequency in Hz (advanced) |

### Channel Settings

Each `[channelN]` section (0–6) maps to one NWR frequency.

| Key | Description |
|-----|-------------|
| `frequency` | Channel frequency in Hz |
| `usrp_host` | Destination IP for USRP audio packets |
| `usrp_port` | Destination UDP port for USRP audio packets |
| `fips` | Comma-separated FIPS codes to match (leave empty to match all) |
| `event_blacklist` | Comma-separated EAS event codes to suppress |
| `enabled` | 1 to enable, 0 to disable |

### FIPS Codes

FIPS codes are 6-digit location identifiers in the format PSSCCC (P=part, SS=state, CCC=county). The P-digit is stripped during comparison, so `048453` matches any sub-area of county 48453.

National alerts (FIPS 000000) always match regardless of configuration.

If `fips` is left empty, all alerts match (useful for testing).

Find your FIPS codes at: https://www.weather.gov/nwr/counties

## Usage

```sh
./weather-usrp [-c config.ini] [-v|-vv|-vvv]
```

| Flag | Description |
|------|-------------|
| `-c path` | Specify config file path |
| `-v` | Show info messages (alerts, state changes) |
| `-vv` | Show debug messages (EAS decode progress, signal diagnostics) |
| `-vvv` | Show trace messages (bit-level decode, correlator details) |

Without `-c`, searches for config in order:
1. `./config.ini` (current directory)
2. `/etc/weather-usrp/config.ini` (system-wide)

## AllStarLink Integration

Add to your node's `rpt.conf`:

```
rxchannel = USRP/127.0.0.1:34001:32001
```

- `34001` — port weather-usrp sends audio to (must match `usrp_port` in config)
- `32001` — return port (required by chan_usrp, not used by weather-usrp)

For multiple channels on separate nodes, configure each channel with a different `usrp_port` and add a corresponding `rxchannel` line per node.

## TCP Control Interface

Connect with `nc` or `telnet` to the control port (default 5555):

```
s                   - JSON status of all channels
p N 1               - Enable passthrough on channel N
p N 0               - Disable passthrough on channel N
q                   - Disconnect
```

Long-form commands also accepted: `STATUS`, `PASSTHROUGH N ON|OFF`, `QUIT`.

### Passthrough Mode

Passthrough streams all audio from a channel continuously, regardless of EAS alert state. Useful for monitoring a channel or relaying weather radio audio to a repeater full-time.

While in passthrough, EAS alerts on that channel are ignored (audio is already flowing).

## How It Works

```
RTL-SDR (2.4 MS/s)
  │
  ├── FIR Channelizer (×7 channels, 20 kHz bandwidth each)
  │     │
  │     ├── FM Demodulator (with DC-blocking filter)
  │     │     │
  │     │     ├── EAS/SAME Decoder (AFSK → bytes → burst voting)
  │     │     │     │
  │     │     │     └── FIPS Match → Event Filter → Gate ALERT → USRP
  │     │     │
  │     │     └── Audio Decimator (48 kHz → 8 kHz) → Gate → USRP UDP
  │     │
  │     └── (per channel)
  │
  └── TCP Control Server
```

## Signal Diagnostics

At `-vv`, signal diagnostics print every 5 seconds per channel:

```
sig: ch6 162.550MHz: pwr avg=-1.4dB peak=1.4dB | FM dev avg=0.197 peak=1.065 (25562Hz pk)
```

- **pwr avg/peak** — channel power (higher = stronger signal)
- **FM dev avg/peak** — FM deviation (NOAA uses ±5 kHz; values near 1.0 indicate clipping)

ADC clipping is also reported:

```
adc: clip: 6189895/24117248 samples (25.67%)
```

Clipping above 5% indicates the signal is too strong — reduce gain or add attenuation.

## EAS Event Codes

Use the `event_blacklist` config field to suppress unwanted codes. Common test suppression: `RWT,RMT,DMO`.

### Warnings

| Code | Description |
|------|-------------|
| AVW | Avalanche Warning |
| BLU | Blue Alert |
| BZW | Blizzard Warning |
| CDW | Civil Danger Warning |
| CEM | Civil Emergency Message |
| CFW | Coastal Flood Warning |
| DSW | Dust Storm Warning |
| EAN | National Emergency Message |
| EQW | Earthquake Warning |
| EVI | Evacuation Immediate |
| EWW | Extreme Wind Warning |
| FFW | Flash Flood Warning |
| FLW | Flood Warning |
| FRW | Fire Warning |
| FSW | Flash Freeze Warning |
| FZW | Freeze Warning |
| HMW | Hazardous Materials Warning |
| HUW | Hurricane Warning |
| HWW | High Wind Warning |
| LEW | Law Enforcement Warning |
| NUW | Nuclear Power Plant Warning |
| RHW | Radiological Hazard Warning |
| SMW | Special Marine Warning |
| SPW | Shelter In-Place Warning |
| SQW | Snow Squall Warning |
| SSW | Storm Surge Warning |
| SVR | Severe Thunderstorm Warning |
| TOR | Tornado Warning |
| TRW | Tropical Storm Warning |
| TSW | Tsunami Warning |
| VOW | Volcano Warning |
| WSW | Winter Storm Warning |

### Watches

| Code | Description |
|------|-------------|
| AVA | Avalanche Watch |
| CFA | Coastal Flood Watch |
| FFA | Flash Flood Watch |
| FLA | Flood Watch |
| HUA | Hurricane Watch |
| HWA | High Wind Watch |
| SSA | Storm Surge Watch |
| SVA | Severe Thunderstorm Watch |
| TOA | Tornado Watch |
| TRA | Tropical Storm Watch |
| TSA | Tsunami Watch |
| WSA | Winter Storm Watch |

### Advisories / Statements

| Code | Description |
|------|-------------|
| ADR | Administrative Message |
| CAE | Child Abduction Emergency |
| EAT | Emergency Action Termination |
| FFS | Flash Flood Statement |
| FLS | Flood Statement |
| HLS | Hurricane Local Statement |
| LAE | Local Area Emergency |
| MEP | Missing and Endangered Persons |
| NIC | National Information Center |
| NMN | Network Notification Message |
| SPS | Special Weather Statement |
| SVS | Severe Weather Statement |
| TOE | 911 Telephone Outage Emergency |

### Tests

| Code | Description |
|------|-------------|
| DMO | Practice/Demo Warning |
| NAT | National Audible Test |
| NPT | Nationwide EAS Test |
| NST | National Silent Test |
| RMT | Required Monthly Test |
| RWT | Required Weekly Test |

## Troubleshooting

### No signal / PLL not locked

```
[R82XX] PLL not locked!
```

The RTL-SDR tuner cannot lock to the requested frequency. This indicates a hardware problem — the dongle's VCO may not cover 162 MHz. Try a different RTL-SDR dongle (Blog V3/V4 recommended).

### No EAS decode

- Check signal diagnostics at `-vv` — you need visible power above the noise floor


### Audio too quiet

Increase `audio_gain` in config. Start at 4.0, increase up to 15.0. NBFM speech typically uses only 20-30% of max deviation, so amplification is needed.

### ADC clipping

Reduce `gain` from -1 (auto) to a fixed value, or add an attenuator between antenna and dongle. Clipping distorts all channels.

## License

GPL v3
