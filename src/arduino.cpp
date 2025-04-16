#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>

#include "arduino.hpp"

#include <spdlog/spdlog.h>

namespace UsArMirror {

Arduino::Arduino(const std::shared_ptr<State> &state) : state(state), running(true) {
    serialThread = std::thread(&Arduino::serialLoop, this);
}

Arduino::~Arduino() {
    running = false;
    if (serialThread.joinable()) {
        serialThread.join();
    }
}

void Arduino::serialLoop() {
    const char *portname = "/dev/ttyUSB0"; // Change this!
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);

    if (fd < 0) {
        spdlog::error("Error opening {}", portname);
        return;
    } else {
        spdlog::info("Successfully opened {}", portname);
    }

            spdlog::info("C");

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        spdlog::error("Error tcgetattr", portname);
        return;
    }

            spdlog::info("B");

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    tty.c_iflag &= ~IGNBRK;                     // disable break processing
    tty.c_lflag = 0;                            // no signaling chars, no echo
    tty.c_oflag = 0;                            // no remapping, no delays
    tty.c_cc[VMIN] = 1;                         // read doesn't block
    tty.c_cc[VTIME] = 5;                        // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);        // ignore modem controls
    tty.c_cflag &= ~(PARENB | PARODD);      // no parity
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        spdlog::error("Error from tcsetattr");
        return;
    }
            spdlog::info("A");

    // Write to Arduino
    while (running) {
        const std::string msg = "u 100\n";
        write(fd, msg.data(), msg.length());

        // Read response
        char buf[100];
        int n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            spdlog::info("Read: {}", std::string(buf, n));
        }

        sleep(1);
    }

    close(fd);
}

} // namespace UsArMirror