# AVR USB clock

Simple clock from ~2011 with 4x7seg display (Atmega AT2313 + CC56). I added an ability to display any numeric data periodically and used it to show the outside temperature.

There is a simple UART-based (CP2102 is used as UART-USB bridge) protocol for this, which is also used to set current time. For some reason (internal RC instead of the external quartz?) time gets out of sync quite fast, so it's get corrected frequently. Don't remember any details on the protocol, but is was very simple and might be obvious from the sources.

This clock was connected to my Wi-Fi router (with some custom FW, cannot remember is OpenWRT of Padavan at that time). Bash script was executed via cron to download main page of www.e1.ru and extract the temperature value from it.

Demo of work - https://www.youtube.com/watch?v=8cy1LKB_-fQ