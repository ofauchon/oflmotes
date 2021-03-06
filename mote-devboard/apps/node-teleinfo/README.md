
Menu
----

- [Features](#features)
- [Hardware](#install)
- [Building](#running-k6)
- [Running](#overview)
- [Need help or want to contribute?](#need-help-or-want-to-contribute)

Features
--------

Teleinfo mote is a circuit and a program aimed at collecting power meters informations.
Power meters sends information over a proprietary interface (Teleinformation or Teleinfo port)
Signal adapter are required to convert signal to regular 1200 baud, 7bit, 1 stop UART signals.
A single UART from uController is shared over the two signal adapters to collect the two power
meters datas. 
It can easily extended to connect to more power meters (One GPIO output is required
for each driven signal adapter


Hardware
--------

Build dependancies
------------------

# Packages to build
pacman -S arm-none-eabi-gcc arm-none-eabi-newlib arm-none-eabi-gdb openocd

# Packages to debug
pacman -S arm-none-eabi-gdb

# RIOT-OS (Kinetis power mode branch from gebart's RIOT-OS clone
cd ~/dev/
git clone https://github.com/gebart/RIOT.git RIOT.gebart
git checkout pr/kinetis-pm

Build
-----

Just run 'make'


Running
-------



Need help or want to contribute?
--------------------------------

Feel free to contact me if you have questions concerning this project, or if you want to contribute.

