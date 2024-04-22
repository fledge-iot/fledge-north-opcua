===================
fledge-north-opcua
===================

Fledge North Plugin that acts as an OPC UA server

This plugin does not send data to a destination system but rather acts as an OPC UA Server to which OPC UA clients can connect.
This server's OPC UA Address Space is created from Readings received from the Fledge storage.
It supports retrieval of current data values and data updates through OPC UA Subscriptions.
It does not support historical data retrieval.

For configuration options, see the `documentation page <docs/index.rst>`_.
