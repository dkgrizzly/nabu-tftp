# nabu-tftp
TFTP Gateway for NABU-PC using a Raspberry Pi Pico W


### Release builds
Binary releases are available at the right for ease of use.


### Usage
NABU TFTP Gateway now supports .nabu segment files
and will packetize them on the fly.
Pre-packetized .pak files are suppored as well.

The first time you connect the Pico-W after flashing
it will do a WiFi scan and step through the configuration.
If you don't see any output in your terminal program,
press return to re-scan for WiFi.

After configuration the gateway will be listening for
communication from the NABU PC.

If you need to reboot or change the configuration you
can use the USB-Serial interface to do this.

Press C to erase the config and reboot, restarting the wizard.
Press R to reboot the Pico-W.
Press X to test your UART configuration (sends 0x10 0xE4).

Multiple UART configs are now supported.

You can connect a RS422 transceiver to UART0 or UART1 on the Pico
and select the pins when configuring.

RS422 transceivers are also supported on any two Pico pins using the PIO.
You can select this with UART Type 3 in the configuration.

You can also use a Schmartboard Inc. RS-232 Module or similar board
which has two RS232 level translator pairs with UART Type 2.


### Compiling
You'll need the [pico-sdk](https://github.com/raspberrypi/pico-sdk/) and toolchain setup according to the instructions from there.

After cloning this repository, initialize the cmake:

```
$ cd nabu-tftp
$ git submodule update --init
$ PICO_SDK_PATH=<path where you cloned pico-sdk> cmake -S . -B build
```

This will take a few minutes...

You can now generate the nabutftp.uf2 file for flashing your Pico-W

```
$ cmake --build build
```


### Packetizing .nabu files
A program is provided in the tools folder which will packetize
.nabu files, though it is not strictly needed anymore.

