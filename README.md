# Park Reservation Simulation

This project implements a simulation of a reservation system for a small private park, "Parque Berlín". The system helps manage the park's occupancy by allowing families to reserve specific time slots during the day, ensuring the park does not exceed its maximum capacity.

The system is implemented using concurrent processes and POSIX threads, with communication between processes achieved via named pipes. The project is developed in C and leverages Linux system calls for process and thread management.

## System Architecture

The application is designed with a client-server architecture:

- **Reservation Controller (Server)**: Manages the reservation requests, simulates time progression, and handles park occupancy.
- **Reservation Agents (Clients)**: Send reservation requests to the controller on behalf of families and handle the responses.

### Reservation Controller

The controller process performs the following tasks:

- Receives reservation requests from agents and approves or rejects them based on park occupancy.
- Simulates the passage of time and updates park occupancy every simulated hour.
- Generates a report at the end of the day summarizing park usage.

### Reservation Agents

Agents are client processes that send reservation requests to the controller. Each agent reads requests from a file and communicates with the controller via named pipes.

## Implementation Details

### Controller Invocation

The controller is invoked from the shell with the following command:

```bash
$ ./controlador –i horaInicio –f horafinal –s segundoshora –t totalpersonas –p pipecrecibe
```

Parameters:
- `horaInicio`: Start time for the simulation (1-24).
- `horaFinal`: End time for the simulation (1-24).
- `segundosHora\`: Real-time seconds that simulate one hour in the simulation.
- `totalPersonas`: Maximum number of people allowed in the park at any given hour.
- `pipecrecibe`: Named pipe for receiving requests from agents.

### Agent Invocation

Agents are invoked with the following command:

```bash
$ ./agente –s nombre –a archivosolicitudes –p pipecrecibe
```

Parameters:
- `nombre`: Name of the agent (used for identification).
- `archivoSolicitudes`: File containing reservation requests.
- `pipecrecibe`: Named pipe for communication with the controller.

## Simulation Flow

1. The controller initializes the simulation based on the provided parameters.
2. Agents send reservation requests read from their input files.
3. The controller processes requests and responds with approval, reprogramming, or rejection.
4. The simulation progresses in simulated hours, updating park occupancy and handling new requests.
5. At the end of the simulation, the controller generates a report and terminates all agents.

## Final Report

At the end of the simulation, the controller generates a report that includes:
- Peak hours with the highest occupancy.
- Hours with the lowest occupancy.
- Number of denied requests.
- Number of approved requests.
- Number of reprogrammed requests.

## Project Setup

To build and run the project, use the provided `Makefile`. Ensure that all source files are compiled and linked correctly.

### Building the Project

```bash
$ make
```

### Running the Simulation

First, start the controller:

```bash
$ ./controlador –i 7 –f 19 –s 1 –t 50 –p /tmp/pipe1
```

Then, start the agents:

```bash
$ ./agente –s Agent1 –a requests.txt –p /tmp/pipe1
$ ./agente –s Agent2 –a requests2.txt –p /tmp/pipe1
```

## Authors

This project was developed as part of the Operating Systems course at Pontifical Xavierian University, presented by:

- Felipe Londoño
- Alejandro Barragan
- Nicolas Cerón
