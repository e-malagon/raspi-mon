**Raspberry PI 4 server monitoring (raspi-mon)**

This application was created to monitor a Linux server mounted on a Raspberry PI 4, it was developed as a single file source code, only one dependency is required, the libgpiod library to handle the gpio pin status. This application was tested on Raspberry Pi OS version 12 (Bookworm).

This application executes some monitoring tasks to get the server time, CPU average load of last minute, RAM usage, CPU temperature, Linux up time, two filesystems size and usage, and two network interfaces name, IP address, and bandwidth usage. This information is displayed on a ST7789 320x240 TFT screen. The screen is connected through the SPI0 device, by default it uses pin 27 as reset, pin 25 as data pin, and pin 18 as backlight without PWM for dimming, only on/off status is available. All the code to handle the screen is included as part of the main program, there is no need for a 3rd party library. Also, a button connected by default to pin 20 is used to wake up the monitoring process, there is no reason to keep it running and updating all the time, so it runs normally for an hour and if the button is not pressed the monitoring application goes into a standby status and turns the screen backlight off, until the button is pressed again.

To request the data only ‘/proc’ files or system calls are used, to avoid the overhead of 3rd party commands execution, like top, free, or df. Maybe it is possible to retrieve more accurate information using those commands, but the intention is to have a lightweight application. To keep the CPU usage minimal as possible, at the beginning a set of fixed data is displayed on the screen, this data includes information that normally doesn’t change over time like filesystem size or an IP address, if this information changes, to refresh it, a process restart is mandatory. Only the dynamic information like CPU load or RAM usage is updated every second, only small chunks of data are sent to the screen to avoid any overhead. This approach allows the application to consume near to 0.0 of CPU over the time, making it a great option to keep it running all the time.

It is possible to change some setting using a config file, a base template for this config file is included using the default settings, the are some comments on this file to document how to change it to customize, the items that can be customized are the spi bus, the interface pins, network devices, filesystem mounting point of monitored disks, number of seconds before check the used space on disks again, color schema, and second before go to sleep mode and turn off the screen.

There is no make file, but you can compile this program with the following command line:

    gcc -O3 raspi-mon.c -o raspi-mon -lgpiod -lpthread

These dependencies are requited:

	apt-get install libgpiod-dev libgpiod-doc

To execute the program only pass as parameter the path of the config file.
