# REAL-TIME-BOILER-CONTROL
Boiler control project, created in the STR (Real Time Software) class. The program controls the boiler level and temperature through sensor reading and actuator control. The software allows the user to add disturbances to the system and set reference values ​​for temperature and level.


# SETUP
This was made to run in Linux environment, therefore is highly recommended to run on a Unix based system.

You need to have java SDK installed to run the Boiler simulator.

To compile all C files, just run the command:

```
$ sudo make
```

# RUN

To run the Boiler simulator run the command:
```
java -jar ./Aquecedor2021.jar 1234
```
OBS: 1234 is the port which the processes will communicate, if left empty the port will assume the value 4545.

To run the software that will controll the boiler run the command:
```
$ ./caldeira_control 1234
```
OBS: The port need be the same as boiler simulator