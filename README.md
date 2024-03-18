# HomeAutomation
This project uses esp32 to communicate with local mqtt broker. Then informations flow through UART to stm32f103 with rs485 menager software, that distribute data between the other stm32 with subdevice software.

In the folder pictures there are pictures and videos that present how it works.

## MQTT commands
To communicate with the hardware you use I2C like commands. 
- "00001111 00001100" - first "byte" and then after the space there is a command that device get. 
- "11111111 00000000" - special command that request for state of every device. 
- "hello" - Checks if esp32 is connected to mqtt topic.

## Subdevice functionality
Subdevice in current project just turn on and turn off 4 leds, but it could be modified to be a led dimmer or something more complex.

## TODO
- Access application
- Real life application - room lights, led strips etc.

