#include "App.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <string>

// Frame buffer management
class FrameManager {
private:
    static constexpr size_t BUFFER_SIZE = 65536 * 4; // 256KB per buffer for safety
    char* writeFrame;
    char* readFrame;
    size_t writeSize = 0;
    std::atomic<bool> running{false};
    std::atomic<int> clientCount{0};
    std::thread captureThread;
    FILE* pipe = nullptr;
    uWS::Loop* loop;
    uWS::App* app;
    int robotInputPipe = -1;

        static std::string readCameraCommand() {
            std::ifstream file("camera_command.txt");
            if (file.is_open()) {
                    std::string command;
                    std::getline(file, command);
                    file.close();
                    if (!command.empty())
                        return command;
            }
                
        std::cerr << "Failed to open camera_command.txt or empty command, using default" << std::endl;
        return "raspivid -n -mm matrix -w 640 -h 480 -fps 25 -ih -if both  -br 60 -g 100 -t 0 -b 4000000 -o -";
        }

    void captureVideo() {
                pipe = popen(readCameraCommand().c_str(), "r");
                if (!pipe) {
                    std::cerr << "Failed to start raspivid" << std::endl;
            return;
        }
                
            // Get the file descriptor from the pipe
            int fd = fileno(pipe);
            if (fd == -1) {
                std::cerr << "Failed to get file descriptor for pipe" << std::endl;
                        running = false;
                }
                int flags = fcntl(fd, F_GETFL, 0);
                if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                    std::cerr << "Failed to set non-blocking mode" << std::endl;
                    running = false;
                }

        while (running) {
            size_t bytesRead = fread(writeFrame + writeSize, 1, 
                                   BUFFER_SIZE - writeSize, pipe);
            if (bytesRead > 0) {
                writeSize += bytesRead;
                std::cout << std::endl << "got " << bytesRead << " bytes";
                // Swap buffers
                size_t frameSize = writeSize;
                std::swap(writeFrame, readFrame);
                writeSize = 0;

                // Defer the broadcast to the main loop
                loop->defer([this, frameSize]() {
                    if (running && clientCount > 0) {
                        app->publish("video_stream", 
                                   std::string_view(readFrame, frameSize), 
                                   uWS::OpCode::BINARY, false);
                        std::cout << " send done " << clientCount;
                    }
                });
            }
                        else if (ferror(pipe)) {
                                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                        // No data available right now, sleep a bit to prevent busy-waiting
                                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                                        clearerr(pipe);
                                        continue;
                                    } else {
                                        // Real error occurred
                                        std::cerr << "Error reading from pipe: " << errno << std::endl;
                                        break;
                                    }
                                }
            if (feof(pipe)) break;
        }

        if (pipe) {
            pclose(pipe);
            pipe = nullptr;
        }
    }

public:
    FrameManager(uWS::Loop* l, uWS::App* a) : loop(l), app(a) {
        // Directly allocate memory for the frames
        writeFrame = new char[BUFFER_SIZE];
        readFrame = new char[BUFFER_SIZE];

        // Open the UNIX named pipe for writing robot inputs
        robotInputPipe = open("/tmp/robotIn", O_WRONLY | O_NONBLOCK);
        if (robotInputPipe == -1) {
            std::cerr << "Failed to open /tmp/robotIn pipe: " 
                      << strerror(errno) << std::endl;
        }
    }

    ~FrameManager() {
        stop();
        // Clean up allocated memory
        delete[] writeFrame;
        delete[] readFrame;

        // Close the input pipe
        if (robotInputPipe != -1) {
            close(robotInputPipe);
        }
      }

      bool writeInputToPipe(const char* inputData) {
          if (robotInputPipe == -1) {
              std::cerr << "Robot input pipe is not open" << std::endl;
              return false;
          }

          ssize_t bytesWritten = write(robotInputPipe, inputData, 3);
          if (bytesWritten != 3) {
              std::cerr << "Failed to write to robot input pipe: " 
                        << strerror(errno) << std::endl;
              return false;
          }
          return true;
      }

      void clientConnected() {
          if (++clientCount == 1) {
              // First client connected, start capture
              running = true;
              captureThread = std::thread(&FrameManager::captureVideo, this);
          }
      }

      void clientDisconnected() {
          if (--clientCount == 0) {
              // Last client disconnected, stop capture
              stop();
          }
      }

      void stop() {
          if (running) {
              running = false;
              std::cout << "stopping ";
              if (captureThread.joinable()) {
                  // Send SIGTERM to raspivid
                  if (pipe) {
                      pclose(pipe);
                      pipe = nullptr;
                  }
                  captureThread.join();
                  std::cout << "done " << std::endl;
              }
          }
      }
  };

  struct PerSocketData {
      // Can be used for per-socket data if needed
  };


  int main() {
      struct us_loop_t* loop = (struct us_loop_t*)uWS::Loop::get();
    
      auto app = std::make_unique<uWS::App>();
      auto frameManager = std::make_unique<FrameManager>((uWS::Loop*)loop, app.get());

      // WebSocket route for video streaming
          app->ws<PerSocketData>("/*", {
              .compression = uWS::DISABLED,
              .maxPayloadLength = 16 * 1024 * 1024,
              .idleTimeout = 16,
              .maxBackpressure = 1 * 1024 * 1024,
              .closeOnBackpressureLimit = true,
              .resetIdleTimeoutOnSend = false,
              .sendPingsAutomatically = true,
    
              // Ensure the order matches the library's WebSocketBehavior declaration
              .open = [frameManager = frameManager.get()](auto* ws) {
                  ws->subscribe("video_stream");
                  frameManager->clientConnected();
              },
              .message = [frameManager = frameManager.get()](auto* ws, std::string_view message, uWS::OpCode opCode) {
                  // Ensure message is exactly 3 bytes
                  std::cout << "got input: " << message.length() << " bytes ";
                  for (int i=0; i<message.length(); i++)
                     std::cout << message.data()[i] << " ";
                  std::cout << std::endl;
                  if (message.length() == 3) {
                      // Write the 3 bytes to the UNIX named pipe
                      frameManager->writeInputToPipe(message.data());
                  } else {
                      std::cerr << "Invalid input message length: " 
                                << message.length() << std::endl;
                  }
              },
              .close = [frameManager = frameManager.get()](auto* ws, int /*code*/, 
                       std::string_view /*message*/) {
                  ws->unsubscribe("video_stream");
                  frameManager->clientDisconnected();
              }
          }).listen(9001, [](auto* listen_socket) {
              if (listen_socket) {
                  std::cout << "Video streaming server listening on port 9001" << std::endl;
              } else {
                  std::cerr << "Failed to listen on port 9001" << std::endl;
              }
          });

      app->run();

      return 0;
  }
