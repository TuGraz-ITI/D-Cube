# D-Cube Design #

D-Cube is published as open source hardware under [CC-BY-SA](license.md) ([online](https://creativecommons.org/licenses/by-sa/4.0/deed.en)).
D-Cube was designed for the EWSN 2016 and updated for EWSN 2017. It serves as a distributed energy measurement setup for sensor nodes. It is also capable of measuring the timeliness of a system by measuring the end-to-end latency.

## D-Cube Hardware ##
The design files in this repository where used to fabricate the current iteration of D-Cube. The GPS module used is a Navspark-GL. For the complementary MOSFET a NTJD4105CT2G was chosen in the final design.

### The current iteration of the hardware (1.0) ###
![d-cube hardware revision 1.0](img/d-cube.jpg)

### The prototype used during EWSN 2016 ###
![d-cube hardware during ewsn2016](img/d-cube_ewsn.jpg)

## D-Cube Software ##
The Software consists of two parts
* A task reading the ADC into a FIFO (on top of a real-time Linux kernel)
* A task reading the FIFO and writing it to the database (InfluxDB)

D-Cube contains a power switch circuit which controls the power before the DCDC isolator. A high signal on GPIO23 is required for the ADC and the target to be supplied with power. The gpio.sh file contains an example for this.

### Real-Time Linux ###
We used https://github.com/emlid/linux-rt-rpi but others should work fine

### Visualisation ###
InfluxDB has many frontends available, we used grafana (http://grafana.org/)
