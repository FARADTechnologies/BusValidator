// fifo_writer.cpp - simple test writer to /tmp/bus_payment_control
#include <iostream>
#include <fstream>
#include <string>

int main() {
    const char *path = "/tmp/bus_payment_control";
    std::ofstream fifo(path);
    if (!fifo.is_open()) {
        std::cerr << "FIFO açılamadı! Lütfen /tmp/bus_payment_control oluşturulduğunu doğrula." << std::endl;
        return 1;
    }
    fifo << "PAN:1234567890123456;EXP:1226" << std::endl;
    fifo.close();
    std::cout << "FIFO'ya yazıldı!" << std::endl;
    return 0;
}
