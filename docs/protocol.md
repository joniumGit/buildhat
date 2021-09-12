# BuildHAT firmware serial protocol

## Introduction

The BuildHAT is a board that provides an interface between a
Raspberry Pi host and up to four LEGO LPF2 (LEGO Power Functions
version 2) devices. Supported
LPF2 devices include a wide range of actuators and sensors.
Firmware running on the HAT deals with the hard real-time
requirements of the LPF2 devices, including monitoring for
connection and disconnection events and interrogating devices
to determine their capabilities and properties.

The HAT communicates with the Raspberry Pi host over the 'command port',
a 115200 baud serial interface with eight bits per character, no parity
and one stop bit ('8N1'). There is no flow control. The command port
protocol is entirely in plain text, and it is perfectly possible to
simply run a terminal emulator on the host and interact manually with
the HAT. When experimenting the HAT in this way it is convenient to enable echo
mode so that you can see what you are typing: see the description of the
``echo`` command below. See also the ``plimit`` command, which must
be sent before many operations will work correctly.

This document describes the commands available over that interface.
Note that a Python library is provided that provides a higher-level
interface to the functions of the HAT: in most cases it will
be preferable to use that library rather than the lower-level
commands described here.

## Port and device basics

The firmware numbers the ports from 0 to 3. There is the notion
of the 'current port', set using the ``port`` command. Many
commands implcitly address the current port.

Each port may have a device connected to it. A device may be
'active', which means that it communicates with the HAT using
a serial interface, or it may be 'passive'. Passive devices
include some lights and motors, although most types of motor
are active devices. Active devices can offer some feedback to
the HAT: for example, an active motor might contain position
or speed sensors to allow it to be controlled precisely.

An active device has up to sixteen 'modes'. A mode can be
thought of as a small memory buffer in the device, much like
the concept of a 'characteristic' in Bluetooth terminology. Some
modes are intended to be written to, to control the device; some
are intended to be read from, to extract sensor readings
for example.

When a device is plugged in to the HAT, the HAT emits a 'connected'
message followed by the information it has about the device. For
a passive device this will just be an identifying code number. For an
active device this will be an identifying code number followed
by other information including the connection baud rate, software
and hardware version numbers and a list of the available modes. See
the ``list`` command below for more detail.

When a device is unplugged from the HAT, a 'disconnected' message
is emitted.

## Device power

The LPF2 connector supplies power to a connected device in two
ways. The first of these is a normal digital logic power supply
at +3.3V which is always present. If a device attempts to draw
too much current from this supply then a 'Port power fault'
message is emitted for as long as the fault persists.

The second power source is at about +7.2V and intended for
driving motors and other relatively high-power devices. This
is supplied by a separate dedicated driver IC for each port.
If a fault is detected on this supply then a 'Motor power fault'
message is emitted repeatedly. This fault condition is latched
in hardware and must be cleared explicitly after the cause
of the fault has been resolved: see the ``clear_faults``
command below.

The motor driver ICs can output PWM waveforms of either polarity
to allow speed and direction control of motors.

## On-board controllers

The firmware implements an independent
controller for the motor power output on each port of the HAT.
The controller can be in one of two modes: 'direct PWM' (the default)
and 'PID'. In addition, each controller has an associated setpoint, which can
be constant or varying: see the ``set`` command below.

In direct PWM mode the setpoint, which must be in the range from
–1 to +1, directly controls the power output. This is useful in
simple motor applications, for driving lights, and for certain
devices that use 'motor power' for other purposes. Such devices
usually need to have power enabled very shortly after connection
is established, and usually need to be powered in reverse: i.e.,
``set –1``.

In PID mode a proportional-integral-differential controller reads
a value from a sensor: typically this will be a speed or angle sensor
on a motor being controlled. This value is called the 'process
variable'. The PID controller adjusts the output power to attempt to
have the process variable track the setpoint closely. Using this
you can, for example, attempt to run a motor at constant speed under
varying load, or move a motor to a given position. See the ``pid``
command below for more details.

## Command summary

Any command can be abbreviated to its shortest unique prefix. Multiple
commands can be given on one line separated by semicolons.

### ``help``, ``?``

Prints a synopsis of the available commands.

### ``echo <0|1>``

Disables (default) or enables echo of characters received over the command port.

### ``version``

Prints the version string for the currently-running firmware.

### ``port <port>``

Sets the current port, used implicitly by many other commands.

### ``vin``

Prints the voltage present on the input power jack.

### ``ledmode <ledmode>``

Sets the behaviour of the HAT's LEDs.

| ``ledmode`` | Effect |
| :---: | --- |
| –1 | LEDs lit depend on the voltage present on the input power jack |
|  0 | LEDs off |
|  1 | orange |
|  2 | green |
|  3 | orange and green together |

### ``list``

Prints a list of all the information known about the LPF2 devices connected to the HAT.

### ``clear_faults``

Clears any latched motor power fault.

### ``coast``

Switches the motor driver on the current port to 'coast' mode, that is, with both outputs floating.

### ``pwm``

Switches the controller on the current port to direct PWM mode.

### ``off``

Same as ``pwm; set 0``.

### ``on``

Same as ``pwm; set 1``.

### ``pid <pidparams>``

Switches the controller on the current port to PID mode. The ``pidparams`` specify from
where the process variable is to be fetched and the gain coefficients for the controller
itself. They are, in order, as follows.

| Name | Meaning |
| --- | --- |
| ``pvport``   | port to fetch process variable from |
| ``pvmode``   | mode to fetch process variable from |
| ``pvoffset`` | process variable byte offset into mode |
| ``pvformat`` | process variable format: ``u1``=unsigned byte, ``s1``=signed byte, ``u2``=unsigned short, ``s2``=signed short, ``u4``=unsigned int, ``s4``=signed int, ``f4``=float |
| ``pvscale``  | process variable multiplicative scale factor |
| ``pvunwrap`` | 0=no unwrapping; otherwise modulo for process variable phase unwrap |
| ``Kp``       | proportional gain |
| ``Ki``       | integral gain |
| ``Kd``       | differential gain |
| ``windup``   | integral windup limit |

The PID controller fetches the process variable from the mode specified
by the ``pvport``, ``pvmode``, ``pvoffset``, ``pvformat`` parameters. Note
that a suitable ``select`` command is required to ensure that this mode's
data are available.

It then multiplies the value from the mode by ``pvscale``.

The ``pvunwrap`` parameter allows
'phase unwrapping' of the process variable. The commonest use of this
is with an angular sensor that outputs a value from –180° to +179°
depending on its absolute position. When the sensor is turned continuously,
the reading will jump from +179° to –180° once per revolution. Setting
the ``pvunwrap`` parameter to 360 will cause the unwrapper to add or subtract 360° at each
discontinuity to make its output continuous (and in principle infinite
in range). More precisely, the output of the unwrapper is guaranteed equal to
its input modulo the ``pvunwrap`` parameter, with the smallest possible
change between one sample and the next.

``Kp``, ``Ki`` and ``Kd`` are the standard PID controller parameters.
The implied unit of time in the integrator and differentiator is one
second.

The output of the error integrator is clamped in absolute value to
the ``windup`` parameter.

### ``set <setpoint>``

Configures the setpoint for the controller on the current port. The
setpoint can be a (floating-point) constant or a waveform specification.

A waveform specification is one of ``square``, ``sine``, ``triangle``,
``pulse`` and ``ramp``, followed by four floating-point parameters.

For square, sine and triangle waveforms these parameters are the minimum
value, the maximum value, the period in seconds, and the initial phase
(from 0 to 1). For a pulse waveform the first three parameters are the
setpoint value during the pulse, the setpoint value after the pulse,
and the duration of the pulse; the fourth parameter is ignored. For a
ramp waveform the first three parameters are the setpoint value at the
start of the ramp, the setpoint value at the end of the ramp, and the
duration of the ramp; the fourth parameter is again ignored.

When a pulse or ramp is completed, a message ``pulse done`` or ``ramp done``
is emitted.

### ``bias <bias>``

Sets a bias value for the current port which is added to positive motor
drive values and subtracted from negative motor drive values. This can
be used to compensate for the fact that most DC motors require a certain
amount of drive before they will turn at all.

### ``plimit <limit>``

Sets a global limit to the motor drive power on all ports. For safety
when experimenting the default value is 0.1; this will usually need to be increased.

### ``select``

Deselects any previously-selected mode on the current port.

### ``select <selmode>``

Selects the specified mode on the current port and repeatedly outputs that mode's data as raw hexadecimal.

### ``select <selmode> <offset> <format>``

Selects the specified mode on the current port. Repeatedly extracts a value
from that mode starting at the specified offset, interpreting it according
to the specified format (``u1``=unsigned byte, ``s1``=signed byte, ``u2``=unsigned short, ``s2``=signed short, ``u4``=unsigned int, ``s4``=signed int, ``f4``=float)
and outputs that value.

### ``selonce``

As ``select`` but outputs data once rather than repeatedly.

### ``combi <index>``

Deconfigures any previously-configured combi mode on the current port at the specified index.

### ``combi <index> <clist>``

Configures a combi mode on the current port at the specified index.
``clist`` is a list of pairs of numbers, each pair giving a mode
and an offset into that mode. Note that the offset is a 'dataset'
offset, i.e., is multiplied by the size of the data elements in that
mode (as given by the ``list`` command) to obtain a byte offset.

### ``write1 <hexbyte>*``

Writes the given hexadecimal bytes to the current port, the first byte
being a header byte. The message is padded if necessary, and length and
checksum fields are automatically populated.

### ``write2 <hexbyte>*``

Writes the given hexadecimal bytes to the current port, the first two bytes
being header bytes. The message is padded if necessary, and length and 
checksum fields are automatically populated.

### ``debug <debugcode>``

Not required for normal use. Note that some debug modes can generate
output faster than the serial port can handle at its standard speed.

### ``signature``

Not required for normal use.

## Examples

The following are some simple examples to illustrate how to use the above commands.

### Using a motor from the SPIKE Prime set

Plug the motor into port 0. Then send

``port 0 ; plimit 1 ; set triangle 0 1 10 0``

The motor will accelerate to full speed and back to zero continuously with a period of ten seconds.

``port 0 ; combi 0 1 0 2 0 3 0 ; select 0 ; plimit 1 ; bias .4 ; pid 0 0 5 s2 0.0027777778 1 5 0 .1 3 ; set square 0 1 3 0``

The motor will alternately rotate one revolution clockwise and one revolution
anticlockwise with a period of three seconds.

## TODO

- expand list command details
- list passive and active ID codes?
- give all reverse-engineered info about how to use devices?
- examples?
