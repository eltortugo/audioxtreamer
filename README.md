# AudioXtreamer
A simple multichannel USB FIFO  to FPGA I2S audio interface

Initially started as a simple way to add usb connectivity to the Yamaha 01x firewire based digital mixer & control surface, this project attempts to provide a simple low latency ASIO interface to a usb connected fpga that handles the i2s I/O.

Inspired by [Koon](https://sites.google.com/site/koonaudioprojects/usb-to-multi-channel-usb2-0) and his 24ch out usb interface, it builds further by providing 32in/32out on a simple usb2.0 interface and bypassing any vendor specific driver to communicate with the lowest possible latency with the fpga.

The proof of concept was built and written around the ft232h usb fifo in combination with a zynq fpga but it rapidly became clear that from a practical point of view, a more flexible Cypress FX2LP and a Xilinx Spartan 6 would suffice the needs of the interface. Although the code is now fully adapted to the 16bit data bus of the Cypress and the CoreGenerator fifos of ISE, it is easily portable to the original 8 bit FTDI and the new Vivado IP generator.

The AudioXtreamer can be seen as 3 different parts:
  - A generic C++ ASIO (TortugASIO) multi-thread 32/64 bit driver that bridges the audio client/DAW to the USB backend
  - USB backend (USBDk) to communicate with the FX2LP
  - VHDL fpga code for the Spartan 6 which handles the usb fifos, sample buffer and generation and decoding of the I2S I/O
  
The sources are written in such a way that the user can configure how many inputs and outputs can be handled in software and hardware.
 
To build the whole project you will need basic knowledge of C/C++ and Visual Studio, VHDL and ISE plus basic digital electronics skills.
 
The hardware implementation is based on the [ZTEX 2.01](https://www.ztex.de/usb-fpga-2/usb-fpga-2.01.e.html) but simple Aliexpress fx2lp / startan6 parts should also work with minimal effort.
