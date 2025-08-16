# Ping Pong LAN Game

A two-player real-time Ping Pong game built in C using TCP sockets and multithreading. The game supports synchronized gameplay over a local area network (LAN) with one player acting as a server and the other as a client.

## Features

### Client–Server Architecture
- One player hosts the game as the server.
- The second player connects as the client.

### Real-Time Synchronization
- Paddle positions, ball position, and scores are exchanged between server and client every few milliseconds.
- Smooth and responsive gameplay experience.

### Gameplay Rules
- Players control paddles (left/right movement).
- Ball bounces between paddles.
- If a player misses the ball, the opponent scores a point.

## Installation & Compilation
To compile the game, run:
gcc -o pingpong pingpong.c -lpthread -lncurses

## Usage

### Start the Server
./pingpong server <PORT>  
Example:  
./pingpong server 5000  

### Start the Client
./pingpong client <SERVER_IP> <PORT>  
Example:  
./pingpong client 192.168.1.10 5000  

## Controls
- Left Arrow → Move paddle left  
- Right Arrow → Move paddle right  
- Q → Quit game  

## Tech Stack
- C  
- TCP sockets for networking  
- Pthreads for multithreading  
- ncurses for terminal-based rendering  

## Author
Developed by Aasritha
