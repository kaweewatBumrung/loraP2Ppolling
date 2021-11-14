# Changelog

All notable changes to this project will be documented in this file.

**Latest version 1.1.0**

## [1.1.0] - 2021-11-14

### Added

 - Added duration `T_waitforJoin` to use in between master create network and start polling. it will be default of duration for node to do joining across all channel. or you can set it up yourself with at least the duration for node to wait for join accept `CH_Sweep_timeOut`.

- Added way to do module reset by pull `LORA_RES_PIN` low at when module fail to initialize. and if module still not respond it will call `ESP.restart();` restart the ESP32 and try to initialize the lora module again.

### Changed

 - Changed channel and number of channel to the range of 920MHz - 923MHz. start at 920.2MHz with channel bandwidth of 200kHz and 14 channel total. [920.2MHz, 920.4MHz, 920.6MHz, 920.8MHz, 921.0MHz, 921.2MHz, 921.4MHz, 921.6MHz, 921.8MHz, 922.0MHz, 922.2MHz, 922.4MHz, 922.6MHz, 922.8MHz]. i changed this because i want to avoid interference with LoRaWAN in thailand. which commonly use frequency in the range of 923MHz - 925MHz.

- Changed the lora module start sequence. to make it more robust (relate to add module reset).

## [1.0.0] - 2021-11-4

### General

- Initial version