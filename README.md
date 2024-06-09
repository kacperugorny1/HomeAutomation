# HomeAutomation
This project uses esp32 to communicate with local mqtt broker. Then informations flow through UART to stm32f103 with rs485 menager software, that distribute data between the other stm32 with subdevice software.
The esp32 dosen't manage the rs485, because it could be replaced with offline solution - eg. Raspberry Pi with touch screen and application, that directly sends data to stm32 rs485 menager. 
It has been modified recently to post state of devices on mqtt topic once per second. It is needed to do so for the new project of Access app that is an android application. 
<br>
[Video presenting how it work](https://www.youtube.com/watch?v=BQn5Pm7Jpvo)
<br>
[Controlling led strips](https://www.youtube.com/watch?v=tAvtpi8G1t0)
<br>
New functionality - at start rs485 menager scans the bus for the devices - no hardcoded addresses.

## MQTT commands
To communicate with the hardware you use I2C like commands. 
- "00001111 00001100" - first "byte" is address and then after the space there is a command that device get. 
- "11111111 11111111" - command that request for state of every device.
- "11111111 11111110" - scan bus for devices
- "11111111 ADDRESS" - command that request for state of particural device. 
- "11111110 ADDRESS" - add address manually. 
- "hello" - Checks if esp32 is connected to mqtt topic.

## Subdevice functionality
Subdevice in current project just turn on and turn off 4 leds, but it could be modified to be a led dimmer or something more complex.

## TODO
- Access application
- Wider bytes for communication - 4 or 8 bytes to have more control and commands be more straight foward - first byte says if its set, get state or scan the bus - now the commands limits the other possibilities - less addresses in bus 
- Queue of operations on esp
- manual adding addresses
