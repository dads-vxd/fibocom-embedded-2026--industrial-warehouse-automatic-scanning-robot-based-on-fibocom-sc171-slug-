#include "serial_sender.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char *argv[]) {
  std::string port = (argc > 1) ? argv[1] : "/dev/ttyUSB0";
  int baud = (argc > 2) ? std::stoi(argv[2]) : 9600;

  std::cout << "Serial send test" << std::endl;
  std::cout << "Port: " << port << ", Baud: " << baud << std::endl;
  std::cout << "Sending digits 1-9, one per second. Press Ctrl+C to stop."
            << std::endl;

  SerialSender sender;
  sender.start(port, baud);

  int n = 1;
  while (true) {
    std::string digit = std::to_string(n);
    // std::cout << "Sending: " << digit << std::endl;
    sender.send(digit);

    n++;
    if (n > 9)
      n = 1;

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  sender.stop();
  return 0;
}
