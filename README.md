# SmartCongestion
Independent / Isolated client based congestion control. Each client CCA uses only it's acquired knowledge since inception to predict congestion. We are trying to build CCA based clients that can naturally adapt to network changes, solely on the analysis of its own available data, without any communication with the server or other CCA's. CCA's use inference methods with the goal of collectively converging at an optimal throughput to RTT ratio and in a relatively timely matter. 

My server will be as simple as possible. 

- 1 single bottleneck area Queue
- Data-link area where concurrency is capped at the servers max bytes per second
- Minimum RTT, non congestive.
- Non-deterministic delay D, non congestive.

Current idea:

Slowstart sends packets at an initial window size of 2.
  - Observe the average RTT relative to throughput for these packets. Track this in a slowstart struct or BestRatio. WS=2, AvgRTT = 1, Avg throughput = 60
  - Double window size. Repeat. Observe the ratio of RTT to Throughput. If the ratio begins to worsen, start convergence process at the best ratio'd window size
    with the hopes of converging.
  - During convergence, adjust window size granularly based on relative changes in network conditions
  - If rtt or throughput changes drastically(Another sender stops OR another connection is established), restart slow start process at some reduced factor relative to the scale of the network changes detected. New processes with little data stored would not know what optimal? - What to do here
  - If a process enters when others have converged, the slow start should trigger other slow starts at an optimal starting point such that the impact on all CCAs is minimal. new process should be able to interpret the network responses accordingly and find some way of interpreting the data such that it can find its relative share.
  - Ideally all CCA's connected should enter their convergence at the same time as to not trigger another slow start and start process over again.
    - This alone would result in a runaway/domino slowstart loop. However maybe as long as the CCA's can find optimal windowsize reduction factors with respect to their knowledge of the network, the effect would be minimal?

  - As long as we can detect variations in network performance, we could use granular slow starts accross all connected CCA's, however if all CCA's are doing this at the same time, ignorant of changes in other flows, theres no real way to tell whos changes are making a difference.
      
  - We could implement a lightweight broadcast from the server at some server time interval to signal CCA's they must slowstart. Signal could be placed in the ACK flags, BUUT some flows may have a larger backlog in the queue creating a possibly large variation in receiving time. 
    -  We could have a fixed universal RTT/TP ratio for what the server can handle, processes above some threshhold of this ratio would be considered to be neglected and we'd need to find a way for all other processes to give this processes a piece of their util until all processes are at the (nearly)same ratio. Could use an external or server based table to track what flow is neglected?
    (Both would also take away from the isolation, server as a blackbox approach)
### CCA's mutually, syncronously sharing util to their optimal ratio is the best approach, how can this be done in isolation?
   
      
  - As long as we can detect variations in network performance, we could use granular slow starts accross all connected CCA's, however if all CCA's are doing this at the same time, ignorant of changes in other flows, theres no real way to tell whos changes are making a difference.



I visualize the performance and results through my own visualizer because why not: https://github.com/Michael-lammens/NetworkSimulation
