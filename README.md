# nabu-tftp
TFTP Gateway for NABU-PC using a Raspberry Pi Pico W


### Compiling
You'll need the [pico-sdk](https://github.com/raspberrypi/pico-sdk/) and toolchain setup according to the instructions from there.

After cloning this repository, initialize the cmake:

```
$ cd nabu-tftp
$ mkdir build
$ cd build
$ PICO_SDK_PATH=<path where you cloned pico-sdk> cmake .. -DWIFI_SSID="your wifi name" -DWIFI_PASSWORD="your wifi key" -DTFTP_SERVER_IP="IP of your TFTP server"
```

This will take a few minutes...

You can now run make in the build folder to generate the nabutftp.uf2 file for flashing your Pico-W

```
$ make
```
