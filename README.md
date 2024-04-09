# SmartCongestion
Independent / Isolated client based congestion control. Each client CCA uses only it's acquired knowledge since inception to predict congestion. We are trying to build isloated CCA based clients that can naturally adapt to network changes, solely on the analysis of its own available data, without any communication with the server or other CCA's. CCA's use bayesian inference methods with the goal of collectively converging at an optimal throughput to RTT ratio and in a relatively timely matter. 

Our server will be as simple as possible. 

- 1 single bottleneck area Queue
- Concurrent data-link area where concurrent packet processing is capped at the servers max bytes per second
- Minimum RTT, non congestive.
- Non deterministic delay D, non congestive.

Current idea:

Slowstart sends packets at an initial window size of 2.
  - Observe the average RTT relative to throughput for these packets. Track this in a slowstart struct or BestRatio. WS=2, AvgRTT = 1, Avg throughput = 60
  - Double window size. Do the same thing again. Observe the ratio of RTT to Throughput. If the ratio begins to worsen, start convergence process at the best ratio'd window size
    with the hopes of converging.
  - During convergence, adjust window size granularly based on relative changes in network conditions
  - If rtt or throughput changes drastically(Another sender stops OR another connection is established), restart slow start process at some reduced factor relative to the scale of the network changes detected. New processes with little data stored would not know what optimal? - What to do here
  - If a process enters when others have converged, our slow start should trigger their slow starts at an optimal starting point such that the impact on all CCAs is minimal. new process should be able to interpret the network responses accordingly and find some way of interpreting the data such that it can find its relative share.
  - Ideally all CCA's connected should enter their convergence at the same time as to not trigger another slow start and start process over again.
    - This alone would result in a runaway/domino slowstart loop. However maybe as long as the CCA's can find optimal windowsize reduction factors with respect to their knowledge of the network, the effect would be minimal? 
