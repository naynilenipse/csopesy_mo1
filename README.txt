CSOPESY Major Output: Process Scheduler and CLI
Section: S01
Members: Espineli, Nyan | Guarin, Raine | Montano, Rovin | Tolentino, Winelle

ENTRY POINT: main.cpp  (main function)

REQUIREMENTS
    A C++17 compiler (g++ 9+ or MinGW-w64 on Windows) with pthread support.

BUILD
    make
  or, without make:
    g++ -std=c++17 -O2 -pthread main.cpp Console.cpp Scheduler.cpp Process.cpp Program.cpp Config.cpp BatchGenerator.cpp Report.cpp -o csopesy

RUN
    Put config.txt in the folder you run from, then:
        ./csopesy            (Mac / Linux / WSL)
        .\csopesy.exe        (Windows; use .\csopesy if built with -o csopesy)
    Edit config.txt only; no recompiling is needed to change parameters.

COMMANDS
    initialize                 read config.txt and start the scheduler (must be first)
    scheduler-start / -stop    generate a dummy process (p01, p02, ...) every batch-process-freq ticks
    screen -s <name>           create a process and open its screen
    screen -r <name>           reopen a running process's screen ("Process <name> not found." if finished)
    screen -ls                 CPU utilization + running/finished processes
    report-util                same as screen -ls, saved to csopesy-log.txt
    exit

  Inside a process screen: process-smi, exit.

CONFIG (config.txt: one "<parameter> <value>" per line; the scheduler value may be quoted)
    num-cpu             [1, 128]     number of CPU cores
    scheduler           "fcfs"/"rr"  scheduling algorithm
    quantum-cycles      [1, 2^32]    time slice for rr (no effect on fcfs)
    batch-process-freq  [1, 2^32]    a new process is generated every N CPU ticks
    min-ins             [1, 2^32]    minimum instructions per process
    max-ins             [1, 2^32]    maximum instructions per process (must be >= min-ins)
    delay-per-exec      [0, 2^32]    busy-wait ticks between instructions

  Sample:
    num-cpu 4
    scheduler "rr"
    quantum-cycles 5
    batch-process-freq 1
    min-ins 1000
    max-ins 2000
    delay-per-exec 0

  If config.txt is missing, malformed, or out of range, "initialize" prints
  every problem found and the console stays locked until it succeeds.

OUTPUT FILES
    csopesy-log.txt            written by report-util in the folder you run from

NOTES
    - One CPU tick is 100 ms of real time.
    - screen -ls / report-util list processes currently on a core as "running";
      processes waiting in the ready queue or sleeping are not listed.
    - Generated names are p01, p02, ...; a name already used with "screen -s"
      is skipped.

FILES
    main.cpp            wires every role together                (Role D)
    Console.*           banner, command parser, screen           (Role C)
    Scheduler.*         CPU ticks, core threads, FCFS/RR         (Role A)
    Process.*           interpreter, logs, process-smi           (Role B)
    Program.*           instruction builder, random generator    (Role B)
    Config.*            config.txt parser + validation           (Role D)
    BatchGenerator.*    scheduler-start / scheduler-stop         (Role D)
    Report.*            screen -ls / report-util text            (Role D)
    config.txt          default parameters
    Makefile            build script