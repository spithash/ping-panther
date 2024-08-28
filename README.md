# Ping Panther (IP Monitoring Tool)

Ping Panther is a simple command line tool for monitoring the status of multiple IP addresses in real-time. It uses a command-line interface with ncurses to display the current status of each monitored IP, making it easy to keep track of your network's connectivity.

## Features

- **Real-Time Monitoring**: Continuously monitors the specified IP addresses and provides real-time status updates.
- **Visual Status Display**: Uses a ncurses-based interface to show the status of each IP address (UP/DOWN) and detailed ping output.
- **Multi-Threaded**: Efficiently handles multiple IPs simultaneously by using a thread for each IP address.

## Requirements

- **C Compiler**: The program is written in C, so you'll need a C compiler like `gcc`.
- **ncurses Library**: The ncurses library is required for the text-based user interface. You can install it via your package manager:
  - **Debian/Ubuntu**: `sudo apt install libncurses6 libncurses-dev libncursesw6`
  - **Fedora/Red Hat**: `sudo dnf install ncurses-devel`
  - **Arch Linux**: `sudo pacman -S ncurses`

## Compilation

Compile the program using "make" (it also supports "make clean" in case you want to clean and rebuild)

## Usage

```
./pingpanther --ips 192.168.1.1 8.8.8.8 1.1.1.1
```
