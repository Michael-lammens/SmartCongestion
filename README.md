# SmartCongestion
Many to one client to server simulation. Dynamic feedback analysis to converge all clients at an optimal throughput to RTT ratio. 

Rely only on local analysis and projection of network conditions, without dependence on flags or feedback from the server.

Current idea:

Slowstart sends packets at an initial window size of 2.
  - Observe the average RTT relative to throughput for these packets. Track this in a slowstart struct or BestRatio. WS=2, AvgRTT = 1, Avg throughput = 60
  - Double window size. Do the same thing again. Observe the ratio of RTT to Throughput. If the ratio worsens, start convergence at the best ratio'd window size.
  - During convergence, adjust window size granularly based on relative changes in network conditions
  - If rtt or throughput changes drastically(Another sender stops OR another connection is established), restart slow start process.
  - If a process enters when others have converged, our slow start should trigger their slow starts
  - Ideally all CCA's connected should enter their convergence at the same time as to not trigger another slow start and start process over again


Other idea: Simple design, try to converge
