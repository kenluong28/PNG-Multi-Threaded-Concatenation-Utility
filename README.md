# PNG Multi-Threaded Concatenation Utility

## Overview

This project is a multi-threaded utility for finding, processing, and concatenating PNG image files. The project is structured into three labs, each focusing on different aspects of systems programming, including file handling, concurrency, and interprocess communication.

## Details

### Lab 1: Introduction to Systems Programming

Implements findpng and catpng utilities.
- findpng: Recursively searches for PNG files in a given directory.
- catpng: Concatenates valid PNG files vertically to reconstruct an image.
Includes starter/ with sample code and libraries.

### Lab 2: Multi-threaded Programming with Blocking I/O

Implements paster using multi-threading and blocking I/O with libcurl. Fetches image fragments from a web server using multiple threads. Uses pthreads for parallel execution to speed up the retrieval process. Features pnginfo.c for extracting PNG metadata.

### Lab 3: Interprocess Communication and Concurrency

Implements paster2 using interprocess communication (IPC). Uses shared memory and semaphores for synchronization. Follows the Producer-Consumer pattern for processing image fragments efficiently. Includes a CSV file (lab3_eceubuntu1.csv) for logging performance metrics.

## Installation and Usage

### Prerequisites

Ensure you have the following dependencies installed:
- GCC (GNU Compiler Collection)
- Make
- libcurl (for HTTP requests in Lab 2 and Lab 3)
- pthread (POSIX threads library)

### Compilation

Each lab has its own Makefile for compilation. To compile a specific lab:
```
cd lab2  # Change to the desired lab directory
make     # Compile the program
```

### Running the Programs

Finding and Concatenating PNG Files (Lab 1):
```
./findpng <directory>
./catpng <file1.png> <file2.png> ...
```
Multi-threaded PNG Retrieval (Lab 2):
```
./paster -t <num_threads> -n <image_number>
```
Example: Using 4 threads to download image 1
```
./paster -t 4 -n 1
```
IPC-Based PNG Retrieval (Lab 3):
```
./paster2 -p <num_producers> -c <num_consumers> -b <buffer_size> -n <image_number>
```
Example: Using 2 producers, 2 consumers, buffer size 10, and downloading image 2
```
./paster2 -p 2 -c 2 -b 10 -n 2
```
