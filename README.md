# STM32 FreeRTOS Audio Player

A FreeRTOS-based embedded audio player for the STM32F407ZG / RT-Spark platform. The project combines binary song selection, synthesized PCM audio through the ES8388 codec, a 240 x 240 LCD, potentiometer-based volume control, RGB status indicators, UART instructions, and pause/resume control.

> **Repository build note:** the current `platformio.ini` compiles `AudioPlayer/Core/Src/main.c`. The repository also contains `AudioPlayer/Core/Src/main.cpp`, which implements a newer graphical music-player UI, but that file is not selected by the current PlatformIO `build_src_filter`. The documentation below describes the active PlatformIO build unless stated otherwise.

## Features

- STM32F407ZG running FreeRTOS
- Three application tasks: display, button polling, and volume control
- Eight selectable songs using a 3-bit button combination
- Five-second song confirmation window using a FreeRTOS software timer
- 40 ms button debounce
- Synthesized 16-bit stereo PCM audio
- ES8388 codec control over I2C2
- I2S3 audio output with DMA1 Stream7 in circular mode
- TIM7 1 kHz note ticker for melody timing
- 240 x 240 LCD output through FSMC
- 12-bit ADC potentiometer volume control
- RGB playback/selection status LEDs
- Built-in UP button for pause/resume
- UART1 instructions at 115200 baud
- FreeRTOS idle-hook light sleep using `WFI`

## Hardware and Pin Mapping

| Function | STM32 Pin / Peripheral | Notes |
| --- | --- | --- |
| Selection button B1 | PE2 | Capture / confirm |
| Selection button B2 | PE3 | Song bit 0 |
| Selection button B3 | PE4 | Song bit 1 |
| Selection button B4 | PE5 | Song bit 2 |
| Built-in UP button | PC5 | Active-low pause/resume |
| Potentiometer wiper | PA0 / ADC1 CH0 | 12-bit volume input |
| Red LED | PF5 | Ready / not playing |
| Green LED | PF6 | Selection active |
| Blue LED | PF7 | Playing |
| ES8388 SDA | PF0 / I2C2 | Codec control |
| ES8388 SCL | PF1 / I2C2 | Codec control |
| I2S3 WS | PA15 | Left/right channel clock |
| I2S3 CK | PB3 | Bit clock |
| I2S3 SD | PB5 | Audio data |
| I2S3 MCLK | PC7 | Codec master clock |
| UART1 TX / RX | PA9 / PA10 | 115200 baud |

The external B1-B4 inputs are configured on PE2-PE5 and are expected to use external 10 kOhm pull-ups. The built-in UP button on PC5 uses the STM32 internal pull-up.

## Song Selection

B2-B4 form a 3-bit song number while B1 is used to capture and confirm the choice.

| B4 B3 B2 | Song | Source label |
| --- | ---: | --- |
| `000` | 1 | Fur Elise - Beethoven |
| `001` | 2 | Canon In D - Pachebelbel |
| `010` | 3 | Minuet in G major - Bach |
| `011` | 4 | Turkish March - Mozart |
| `100` | 5 | Nocturne in E flat - Chopin |
| `101` | 6 | Waltz No. 2 - Shostakovich |
| `110` | 7 | Nocturne in C sharp - Chopin |
| `111` | 8 | Symphony No. 40 - Mozart |

The song source table contains ten melodies in total, but the 3-bit selector exposes the first eight.

### Selection procedure

1. Hold B2-B4 in the desired binary combination.
2. Press B1 once to capture the selection.
3. Release the buttons.
4. Press B1 again within five seconds to confirm.
5. The display task starts the confirmed song.

If the second B1 press does not occur within five seconds, the pending selection is cancelled.

## FreeRTOS Architecture

The active firmware creates three application tasks at priority 1:

| Task | Stack | Responsibility |
| --- | ---: | --- |
| `DisplayTask` / `StartDisplayTask` | 512 words | LCD status, RGB LEDs, UART instructions, and starting confirmed songs |
| `ButtonTask` / `StartButtonTask` | 256 words | Debounced B1-B4 selection and built-in UP pause/resume |
| `VolumeTask` / `StartVolumeTask` | 256 words | ADC sampling, filtering, percentage mapping, and audio volume updates |

A FreeRTOS mutex (`lcdMutexHandle`) protects LCD access. A one-shot software timer (`SelectionTimeout`) implements the five-second confirmation window.

Audio sample generation is interrupt/DMA driven rather than implemented as a fourth task.

## Audio Pipeline

```text
Song note/beat definitions
        |
        v
SparkSongs_Get()
        |
        v
SparkAudio_PlaySong()
        |
        v
Triangle-wave PCM generation
        |
        v
I2S3 @ 8 kHz, 16-bit stereo
        |
        v
DMA1 Stream7 (circular)
        |
        v
ES8388 audio codec
        |
        v
Headphones / audio output
```

TIM7 runs at 1 kHz and advances melody note timing. DMA half-transfer and transfer-complete callbacks refill the two halves of the audio buffer. Short attack/release envelopes are applied to reduce clicks between notes.

This project synthesizes melodies from note and beat data; it does **not** decode `.mp3` files.

## Volume Control

The potentiometer is connected to PA0 and sampled by ADC1 channel 0 using 12-bit conversion (`0-4095`). The volume task applies exponential smoothing:

```text
filtered = (filtered * 7 + raw + 4) / 8
```

The filtered value is mapped to 0-100%. Changes smaller than 2 percentage points are ignored, except at the 0% and 100% endpoints. The accepted percentage updates the PCM amplitude through `SparkAudio_SetVolume()`.

## Playback Indicators

| State | LED |
| --- | --- |
| Playing | Blue (PF7) |
| Selecting / confirming | Green (PF6) |
| Ready, paused, or otherwise not playing | Red (PF5) |

The built-in UP button on PC5 toggles pause/resume when a song is active.

## LCD and UART

The active `main.c` build presents a text-based status interface on the onboard LCD, including codec detection, current song, playback status, volume, and pending choice.

At startup, UART1 prints the operating instructions:

```text
Hold B2-B4 for binary song 1-8, then press B1.
Release all buttons; press B1 within 5 seconds to confirm.
Built-in UP button: pause/resume.
Potentiometer controls playback volume.
```

The repository also contains a newer graphical UI in `AudioPlayer/Core/Src/main.cpp` with a progress bar, animated waveform, modern player card, volume bar, and confirmation countdown. That source is currently outside the active PlatformIO filter.

## Project Structure

```text
stm32-freertos-audio-player/
|-- AudioPlayer/
|   |-- Core/
|   |   |-- Inc/
|   |   |   |-- FreeRTOSConfig.h
|   |   |   |-- main.h
|   |   |   |-- spark_audio.h
|   |   |   |-- spark_lcd.h
|   |   |   `-- spark_songs.h
|   |   `-- Src/
|   |       |-- main.c          # Active PlatformIO application
|   |       |-- main.cpp        # Alternate newer graphical UI
|   |       |-- spark_audio.c
|   |       |-- spark_lcd.c
|   |       `-- spark_songs.cpp
|   |-- Drivers/
|   |-- Middlewares/Third_Party/FreeRTOS/
|   `-- AudioPlayer.ioc
|-- reference/
|   |-- song.h
|   `-- song_def.h
|-- platformio.ini
`-- test/
```

## Build and Upload

### Requirements

- Visual Studio Code
- PlatformIO
- ST-Link programmer/debugger
- STM32F407ZG / compatible RT-Spark hardware

### Build

```bash
pio run -e black_f407zg
```

### Upload

```bash
pio run -e black_f407zg -t upload
```

### Serial Monitor

```bash
pio device monitor -b 115200
```

`platformio.ini` currently specifies `COM9` as the monitor port. Change or remove `monitor_port` if the board appears on a different COM port.

## Verification Checklist

The repository does not currently contain automated application test cases; the `test/` directory only contains PlatformIO's placeholder README. Before submission, verify the following on the physical board and capture evidence for the laboratory report:

- All eight binary selections map to the intended songs
- Second B1 press confirms only within the five-second window
- Timeout cancels a pending selection
- Built-in UP pauses and resumes active playback
- Potentiometer changes the volume smoothly
- Blue, green, and red LED states match playback/selection state
- LCD updates the song, status, choice, and volume correctly
- UART startup instructions appear at 115200 baud
- Audio continues without DMA/I2S faults

## Documentation Contribution

This project was completed as a pair activity.

| Contributor | Contribution |
| --- | --- |
| `@msyennna` / [Partner's full name] | Firmware and hardware implementation |
| Shanice Reih H. Tanque | README documentation and laboratory report |

## Repository

Original project repository: <https://github.com/msyennna/stm32-freertos-audio-player>
