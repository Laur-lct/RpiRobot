# RpiRobot

RpiRobot is a Raspberry Pi-based robot project that features direct low-latency video streaming from the Raspberry Pi to a web browser over WebSocket. The project has been successfully tested with up to 7 connected clients.

## Hardware Requirements

- Raspberry Pi 1 Model A+
- Raspberry Pi Camera Module

## Software Requirements

- Raspberry Pi OS (Bullseye) with legacy Raspivid camera stack enabled
- uWebSockets library

## Installation and Setup

1. Install Raspberry Pi OS (Bullseye) on your Raspberry Pi and enable the legacy Raspivid camera stack.
2. Install the uWebSockets library by following the instructions in the [uWebSockets documentation](https://github.com/uNetworking/uWebSockets).
3. Compile the `video_stream_server.cpp` file located in the `examples` folder of the uWebSockets installation using the following command:

   ```
   sudo g++ -march=native -pthread -O3 -std=c++20 -Isrc -IuSockets/src -flto examples/video_stream_server.cpp uSockets/*.o -lz -o video_stream_server
   ```

4. Copy the compiled `video_stream_server` executable, as well as the `camera_settings.txt` file, to the `/usr/local/bin/` directory on your Raspberry Pi.
5. Compile the `motor_control.c` file and copy the resulting executable to the `/usr/local/bin/` directory as well.
6. Place the `index.html` file in a web-accessible directory on your Raspberry Pi.

## Usage

1. Run the `motor_control` to allow controlling the robot's movement
2. Run the `video_stream_server` executable to start the video streaming server.
3. Open the `index.html` file in a web browser to view the live video stream from the Raspberry Pi camera.

## Limitations

- This project is based on the older Raspberry Pi 1 Model A+ hardware, which may have limitations in terms of processing power and video quality compared to newer Raspberry Pi models.
- The video streaming is limited to a maximum of 7 connected clients, as tested.
