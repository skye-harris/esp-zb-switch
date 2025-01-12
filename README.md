### Zigbee Switch ###

A simple Zigbee Switch, put together as an example project for Reddit

Functionality is incomplete:
- Does not store state between reboots
- No logic in place to control a relay, although the attribute handler all exists for this to be slotted in
- Always inits zigbee with the state set to false, but does not immediately report this to the controller
