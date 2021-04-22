# PrintSpider for Arduino

This repository contains source code for the [PrintSpider project](https://hackaday.io/project/176931-hp-printer-cartridge-control-module) of HP printer cartridge control module for Arduino Uno board. Project is about creating an Arduino compatible control module for HP cartridge in easy to connect format, witch parts almost all is printable with 3D printer.

## Project stucture

Source code organised as [PlatformIO](https://platformio.org/) project, so you can read full documentation about it in it's [documentation](https://docs.platformio.org/en/latest/).
Most important parts consists of:

- **./main.c**: source code of the simple program for jetting with cartridge.
- **./lib/PrintSpider/printspider.h** and **./lib/PrintSpider/printspider.h**: header and implementation of the library of generator of output signals sequences for control module.

## Development

To debug this code locally without real Arduino board you could clone this project with command:

```bash
git clone https://github.com/lichnost/PrintSpider_Arduino.git
```

Open project with PlatformIO.
Then you can run it with PlatformIO [debug](https://docs.platformio.org/en/latest/plus/debugging.html) feature. **./platformio.ini** has configured property to debug builded firmware with [simavr](https://github.com/buserror/simavr) AVR simulator.

Additionally you can run simulation with otputing logic data to VCD(value change dump) file for viewing in logic analisys GUI like [GTKWave](http://gtkwave.sourceforge.net/) or [PulseView](https://sigrok.org/wiki/PulseView).
Previously you should build and install latest version of *simvar* from source code as follows:

```bash
git clone https://github.com/buserror/simavr.git
cd simavr/
make install RELEASE=1
```

Then you can start simulation:

```bash
simavr ./.pio/build/uno/firmware.elf -m atmega328p -f 16000000L --output output.vcd --add-trace D1=trace@0x002B/0x01 --add-trace D2=trace@0x002B/0x02 --add-trace D3=trace@0x002B/0x04 --add-trace CSYNC=trace@0x002B/0x08 --add-trace S1=trace@0x002B/0x10 --add-trace S2=trace@0x002B/0x20 --add-trace S3=trace@0x002B/0x40 --add-trace S4=trace@0x002B/0x80 --add-trace S5=trace@0x0025/0x01 --add-trace DCLK=trace@0x0025/0x02 --add-trace F3=trace@0x0025/0x04 --add-trace F5=trace@0x0025/0x08
```

Output VCD file will be named **./output.vcd**.
Screenshot from PulseView:
![](./docs/pulseview.png)