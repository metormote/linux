.. SPDX-License-Identifier: GPL-2.0

Kernel driver stpddc60
======================

Supported chips:

  * ST STPDDC60

    Prefix: 'stpddc60', 'bmr481'

    Addresses scanned: -

    Datasheet: https://flexpowermodules.com/documents/fpm-techspec-bmr481

Author: Erik Rosen <erik.rosen@metormote.com>


Description
-----------

This driver supports hardware monitoring for ST STPDDC60 controller chip and
compatible modules.

The driver is a client driver to the core PMBus driver. Please see
Documentation/hwmon/pmbus.rst and Documentation.hwmon/pmbus-core for details
on PMBus client drivers.


Usage Notes
-----------

This driver does not auto-detect devices. You will have to instantiate the
devices explicitly. Please see Documentation/i2c/instantiating-devices.rst for
details.


Platform data support
---------------------

The driver supports standard PMBus driver platform data.


Sysfs entries
-------------

The following attributes are supported. Limits and alarms are read-write; all other
attributes are read-only.

======================= ========================================================
in1_label		"vin"
in1_input		Measured input voltage.
in1_lcrit		Critical minimum input voltage.
in1_crit		Critical maximum input voltage.
in1_lcrit_alarm		Input voltage critical low alarm.
in1_crit_alarm		Input voltage critical high alarm.

in2_label		"vout1"
in2_input		Measured output voltage.
in2_lcrit		Critical minimum output Voltage.
in2_crit		Critical maximum output voltage.
in2_lcrit_alarm		Critical output voltage critical low alarm.
in2_crit_alarm		Critical output voltage critical high alarm.

curr1_label		"iout1"
curr1_input		Measured output current.
curr1_max		Maximum output current.
curr1_max_alarm		Output current high alarm.
curr1_crit		Critical maximum output current.
curr1_crit_alarm	Output current critical high alarm.

temp1_input		Measured maximum temperature of all phases.
temp1_max		Maximum temperature limit.
temp1_max_alarm		High temperature alarm.
temp1_crit		Critical maximum temperature limit.
temp1_crit_alarm	Critical maximum temperature alarm.
======================= ========================================================
