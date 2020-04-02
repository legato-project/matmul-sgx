------------------------
Purpose of matmul-sgx
------------------------
This project consists of a parallel and secure version of the multiplication multiplication problem. I performes the multiplication of two matrices (A and B) into a third one (C). Parallelization is achived using [OmpSs Programming Model](https://pm.bsc.es/ompss) and security is assured by means of [Intel SGX](https://software.intel.com/en-us/sgx/sdk).

------------------------------------
How to Build/Execute
------------------------------------
1. Install OmpSs
2. Install Intel(R) SGX SDK for Linux* OS
3. Make sure your environment is set:
    ```
    $ export TARGET=$HOME/ompss
    $ source ${sgx-sdk-install-path}/environment
    ```
4. Build the project with the prepared Makefile:
    1. Hardware Mode, Debug build:
        ```
        $ make
        ```
    2. Hardware Mode, Pre-release build:
        ```
        $ make SGX_PRERELEASE=1 SGX_DEBUG=0
        ```
    3. Hardware Mode, Release build:
        ```
        $ make SGX_DEBUG=0
        ```
    4. Simulation Mode, Debug build:
        ```
        $ make SGX_MODE=SIM
        ```
    5. Simulation Mode, Pre-release build:
        ```
        $ make SGX_MODE=SIM SGX_PRERELEASE=1 SGX_DEBUG=0
        ```
    6. Simulation Mode, Release build:
        ```
        $ make SGX_MODE=SIM SGX_DEBUG=0
        ```    
5. Execute the binary directly:
    1. Sequential execution:
        ```
        $ OMP_NUM_THREADS=1 ./app
        ```
    2. Parallel execution:
        ```
        $ export n=number_of_threads
        $ OMP_NUM_THREADS=$n ./app
        ```
6. Remember to "make clean" before switching build mode
