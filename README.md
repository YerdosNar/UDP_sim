# UDP Simulation

A simple protocol over UDP trying to mimic reliable transfer using **STOP-and-WAIT**. Very inefficient.
Retransmission on dropped packets.
```bash
# To compile the './bin/peer', and './bin/server'
make

# To compile drop testing
make test-drop
```

Later improvements **GOBACK-N** using sliding window.
