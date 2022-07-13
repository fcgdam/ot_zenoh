# ot_zenoh -> Sample program for zenoh

This is a simple basic program for testing Zenoh connectivity from a zenoh-pico enable node running Zephyr and using Openthread as the network layer.

The target platform is the NRF52840 dongle.

The project builds with PlatformIO.

# Instalation:

- Clone this repository
- Clone the zenoh-pico repository

```
git clone https://github.com/eclipse-zenoh/zenoh-pico.git
```

- Move to the ot_zenoh repository and link the zenoh-pico as a library dependency

```
cd ot_zenoh/lib
ln -s ../../zenoh-pico zenoh-pico
```

- Make sure that there is a soft link on lib for zenoh-pico

# Compilation

- Change on *src/main* on line 275 the OffMesh address of the zenoh router/daemon.
- Compile with *pio run* 
- Press the reset button to turn the NRF52840 into DFU mode (red led breathing)
- Upload the code: *pio run -t upload*

# Configuration

- Connect to the NRF52840 Zephyr console and use the *ot* commands to join to an existing network.
