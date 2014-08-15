id4-weather
===========

Serial interface for Heath ID4001-5. Includes builtin web service.

$ make release
$ sudo cp ./bin/Release/id4-pi /usr/local/bin

id4-pi Control and reporting for Heath ID4001 V1.0

id4-pi [options]

 options:
   -s name     Serial device suffix (default: USB0)
   -W          Show current time/weather data and exit
   -M          Show lastest min/max data and exit
   -H          Show weather history (31 days) and exit
   -V          Show ID4001-5 firmware version and exit
   -T          Set ID4001 time from system
   -C          Clear current weather data memory
   -B          Run in background (daemonize)
   -Z          Turn off weather logging
   -n          Open serial port in non-block mode
   -r          Record serial comms to file: weather.log
