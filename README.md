# RGB-LED-Controller
RGB-LED-Controller based on an Atmel ATmega8 with UART serial control interface
The controller is designed to be controlled by a human through a serial interface

# communication settings
baud rate   19.200
data bits   8
parity      none
stop bits   1

# available commands
r<value> - sets the red pwm value (0...255)
g<value> - sets the green pwm value (0...255)
b<value> - sets the blue pwm value (0...255)
p<1/0> - switches the power on or off
a<1/0> - switches auto-on after powerloss on or off
s<y> - stores the current configuration
l<y> - loads the stored configuration as current configuration