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
event_blacklist = RWT,RMT,DMO
enabled = 1
```

Each `[channelN]` section (0–6) maps to one NWR frequency and routes alert audio to a specific USRP destination.

- `fips` — Comma-separated FIPS codes to match (P-digit stripped for comparison)
- `event_blacklist` — Comma-separated 3-letter EAS event codes to suppress (e.g. RWT, RMT, DMO). If omitted or empty, all events pass through.

## Usage

```sh
./weather-usrp [-c config.ini] [-v]
```

Without `-c`, searches for config in order:
1. `./config.ini` (current directory)
2. `/etc/weather-usrp/config.ini` (system-wide)

## AllStarLink Integration

Configure `chan_usrp` in your `rpt.conf`:

```
rxchannel = USRP/127.0.0.1:34001:32001
```

Where `34001` is the port weather-usrp sends audio to, and `32001` is the return port (unused by weather-usrp but required by chan_usrp).

## TCP Control Interface

Connect with `nc` or `telnet` to the control port (default 5555):

```
s                   - JSON status of all channels
p N 1               - Enable passthrough on channel N
p N 0               - Disable passthrough on channel N
q                   - Disconnect
```

Long-form commands also accepted: `STATUS`, `PASSTHROUGH N ON|OFF`, `QUIT`.

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
  │     │     │     └─ SAME Parser → FIPS Match → Event Blacklist → Gate ALERT
  │     │     │
  │     │     └─ Decimator (48k → 8k) → Gate → USRP UDP
  │     │
  │     └─ (per channel)
  │
  └─ TCP Control Server
```

## USRP Protocol

Each audio frame is 352 bytes: 32-byte header + 320 bytes of signed 16-bit LE audio (160 samples at 8 kHz). The header contains the "USRP" magic, sequence number, keyup flag (PTT), and type field. Type 0 = voice, keyup 1 = transmit.

## SAME Event Codes

The following event codes are used in the SAME/EAS system. Use the `event_blacklist` config field to suppress unwanted codes (e.g. `RWT,RMT,DMO` for tests and demos).

### Warnings

| Code | Description |
|------|-------------|
| AVW  | Avalanche Warning |
| BLU  | Blue Alert |
| BZW  | Blizzard Warning |
| CDW  | Civil Danger Warning |
| CEM  | Civil Emergency Message |
| CFW  | Coastal Flood Warning |
| DSW  | Dust Storm Warning |
| EAN  | National Emergency Message |
| EQW  | Earthquake Warning |
| EVI  | Evacuation Immediate |
| EWW  | Extreme Wind Warning |
| FFW  | Flash Flood Warning |
| FLW  | Flood Warning |
| FRW  | Fire Warning |
| FSW  | Flash Freeze Warning |
| FZW  | Freeze Warning |
| HMW  | Hazardous Materials Warning |
| HUW  | Hurricane Warning |
| HWW  | High Wind Warning |
| LEW  | Law Enforcement Warning |
| NUW  | Nuclear Power Plant Warning |
| RHW  | Radiological Hazard Warning |
| SMW  | Special Marine Warning |
| SPW  | Shelter In-Place Warning |
| SQW  | Snow Squall Warning |
| SSW  | Storm Surge Warning |
| SVR  | Severe Thunderstorm Warning |
| TOR  | Tornado Warning |
| TRW  | Tropical Storm Warning |
| TSW  | Tsunami Warning |
| VOW  | Volcano Warning |
| WSW  | Winter Storm Warning |

### Watches

| Code | Description |
|------|-------------|
| AVA  | Avalanche Watch |
| CFA  | Coastal Flood Watch |
| FFA  | Flash Flood Watch |
| FLA  | Flood Watch |
| HUA  | Hurricane Watch |
| HWA  | High Wind Watch |
| SSA  | Storm Surge Watch |
| SVA  | Severe Thunderstorm Watch |
| TOA  | Tornado Watch |
| TRA  | Tropical Storm Watch |
| TSA  | Tsunami Watch |
| WSA  | Winter Storm Watch |

### Advisories / Statements

| Code | Description |
|------|-------------|
| ADR  | Administrative Message |
| CAE  | Child Abduction Emergency |
| EAT  | Emergency Action Termination |
| FFS  | Flash Flood Statement |
| FLS  | Flood Statement |
| HLS  | Hurricane Local Statement |
| LAE  | Local Area Emergency |
| MEP  | Missing and Endangered Persons |
| NIC  | National Information Center |
| NMN  | Network Notification Message |
| SPS  | Special Weather Statement |
| SVS  | Severe Weather Statement |
| TOE  | 911 Telephone Outage Emergency |

### Tests

| Code | Description |
|------|-------------|
| DMO  | Practice/Demo Warning |
| NAT  | National Audible Test |
| NPT  | Nationwide EAS Test |
| NST  | National Silent Test |
| RMT  | Required Monthly Test |
| RWT  | Required Weekly Test |

### Internal Use Only

| Code | Description |
|------|-------------|
| TXB  | Transmitter Backup On |
| TXF  | Transmitter Carrier Off |
| TXO  | Transmitter Carrier On |
| TXP  | Transmitter Primary On |

### Future Implementation

| Code | Description |
|------|-------------|
| BHW  | Biological Hazard Warning |
| BWW  | Boil Water Warning |
| CHW  | Chemical Hazard Warning |
| CWW  | Contaminated Water Warning |
| DBA  | Dam Watch |
| DBW  | Dam Break Warning |
| DEW  | Contagious Disease Warning |
| EVA  | Evacuation Watch |
| FCW  | Food Contamination Warning |
| IBW  | Iceberg Warning |
| IFW  | Industrial Fire Warning |
| LSW  | Landslide Warning |
| POS  | Power Outage Advisory |
| WFA  | Wild Fire Watch |
| WFW  | Wild Fire Warning |

## License

GPL v2
