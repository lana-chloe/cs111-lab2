# You Spin Me Round Robin

C implementation for round robin scheduling for a given workload and quantum length.

## Building

Run make in the lab-02 directory to create a rr executable.

    make 

## Running

Run the following instructions:

    ./rr processes.txt n

processes.txt is the .txt file containing the workload for round robin scheduling. the first line is the total number of processes. each line following the first line should follow the format: process pid, arrival time, burst time. n is the quantum length of the time each process gets to run on the CPU.

## Cleaning up

Run make clean to remove all the binary files

    make clean
