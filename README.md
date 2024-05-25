# Bonjour Server

This module implements the Bonjour/DNS-SD and mDNS protocols. Only the server side is implemented.

Preliminary version.

### Current limitations

* Multicast only, no unicast.
* Unsolicited announcements are sent only once (when the service instance is created).
* No announcement when a service instance is unregistered or the server is stopped.
* No probing, no conflict resolution.
* Known answers are added at the end of the last segment. No extra segment is generate and the known answers that do not fit there are discarded. The TC flag is never set.
* No congestion avoidance.
* Only the `local` domain is supported.