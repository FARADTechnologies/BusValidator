// parse_card_data.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

class CardDataProcessor {
private:
    std::string filePath;
    std::string fifoPath;
    std::string apiUrl;

    // Loglama fonksiyonu
    void writeLog(const std::string& level, const std::string& message) {
        std::ofstream logFile("/tmp/api_log.txt", std::ios_base::app);
        if (logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            std::time_t time_now = std::chrono::system_clock::to_time_t(now);
            // ctime already ends with newline; trim it.
            std::string timestr = std::ctime(&time_now);
            if (!timestr.empty() && timestr.back() == '\n') timestr.pop_back();
            logFile << "[" << level << "] " << timestr << " - " << message << "\n";
            logFile.close();
        }
    }

    void logInfo(const std::string& message) {
        std::cout << "[INFO] " << message << std::endl;
        writeLog("INFO", message);
    }

    void logError(const std::string& message) {
        std::cerr << "[ERROR] " << message << std::endl;
        writeLog("ERROR", message);
    }

    void logSuccess(const std::string& message) {
        std::cout << "[SUCCESS] " << message << std::endl;
        writeLog("SUCCESS", message);
    }

    std::string formatDuration(const std::chrono::milliseconds& duration) {
        auto ms = duration.count();
        if (ms < 1000) {
            return std::to_string(ms) + "ms";
        } else {
            double seconds = ms / 1000.0;
            std::ostringstream ss;
            ss << seconds << "s";
            return ss.str();
        }
    }

    // Hex -> ASCII (her 2 hex -> 1 char)
    std::string hexToAscii(const std::string& hex) {
        std::string ascii;
        for (size_t i = 0; i + 1 < hex.length(); i += 2) {
            std::string byteStr = hex.substr(i, 2);
            try {
                char c = static_cast<char>(std::stoi(byteStr, nullptr, 16));
                ascii.push_back(c);
            } catch (...) {
                // ignore malformed byte
            }
        }
        return ascii;
    }

    // DFEF4D/track1 parsing
    std::string extractPAN(const std::string& trackData) {
        size_t start = trackData.find(';');
        if (start == std::string::npos) return "";
        size_t end = trackData.find('?', start);
        if (end == std::string::npos) return "";
        std::string track1 = trackData.substr(start + 1, end - start - 1);
        size_t equalPos = track1.find('=');
        if (equalPos == std::string::npos) return "";
        return track1.substr(0, equalPos);
    }

    std::string extractExpiry(const std::string& trackData) {
        size_t start = trackData.find(';');
        if (start == std::string::npos) return "";
        size_t end = trackData.find('?', start);
        if (end == std::string::npos) return "";
        std::string track1 = trackData.substr(start + 1, end - start - 1);
        size_t eq = track1.find('=');
        if (eq == std::string::npos || track1.size() < eq + 5) return "";
        return track1.substr(eq + 1, 4); // YYMM
    }

    // C1DFEE TLV-based parsing (5A08 PAN, 5F2403 expiry)
    std::string extractPANFromC1DFEE(const std::string& hexData) {
        size_t pos = hexData.find("5A08");
        if (pos == std::string::npos) {
            logError("C1DFEE verisinde 5A08 etiketi bulunamadı!");
            return "";
        }
        size_t panStart = pos + 4;
        if (panStart + 16 > hexData.length()) {
            logError("C1DFEE verisinde PAN verisi tam değil!");
            return "";
        }
        std::string pan = hexData.substr(panStart, 16);
        logInfo("C1DFEE'den PAN çıkarıldı: " + pan);
        return pan;
    }

    std::string extractExpiryFromC1DFEE(const std::string& hexData) {
        size_t pos = hexData.find("5F2403");
        if (pos == std::string::npos) {
            logError("C1DFEE verisinde 5F2403 etiketi bulunamadı!");
            return "";
        }
        size_t expiryStart = pos + 6;
        if (expiryStart + 4 > hexData.length()) {
            logError("C1DFEE verisinde expiry verisi tam değil!");
            return "";
        }
        std::string expiry = hexData.substr(expiryStart, 4);
        logInfo("C1DFEE'den expiry çıkarıldı: " + expiry);
        return expiry;
    }

    // FIFO'ya non-blocking yazma (retry)
    void writeToFIFO(const std::string& pan, const std::string& expiry) {
        const int max_attempt_ms = 3000;
        const int sleep_ms = 100;
        int waited = 0;
        bool written = false;
        std::string payload = "PAN:" + pan;
        if (!expiry.empty()) payload += ";EXP:" + expiry;
        payload += "\n";

        while (waited < max_attempt_ms) {
            int fd = open(fifoPath.c_str(), O_WRONLY | O_NONBLOCK);
            if (fd >= 0) {
                ssize_t w = write(fd, payload.c_str(), (size_t)payload.size());
                close(fd);
                if (w == (ssize_t)payload.size()) {
                    logSuccess("PAN ve expiry FIFO'ya yazıldı: " + fifoPath);
                    written = true;
                    break;
                } else {
                    logError("FIFO yazma başarısız (kısmi yazıldı).");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            waited += sleep_ms;
        }
        if (!written) {
            logError("FIFO açılamadı / yazılamadı: " + fifoPath);
        }
    }

    // GUI'ye durum token'ı gönder (0=success,1=fail)
    void sendToGUI(const std::string& status) {
        std::string token = (status == "success" || status == "ok") ? "0" : "1";
        const int max_attempt_ms = 3000;
        const int sleep_ms = 100;
        int waited = 0;
        bool sent = false;
        std::string payload = token + "\n";

        while (waited < max_attempt_ms) {
            int fd = open(fifoPath.c_str(), O_WRONLY | O_NONBLOCK);
            if (fd >= 0) {
                ssize_t w = write(fd, payload.c_str(), (size_t)payload.size());
                close(fd);
                if (w == (ssize_t)payload.size()) {
                    logInfo("GUI'ye durum gönderildi: " + token);
                    sent = true;
                    break;
                } else {
                    logError("GUI FIFO'ya kısmi yazma hatası");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            waited += sleep_ms;
        }
        if (!sent) {
            logError("GUI FIFO'ya yazılamadı: " + fifoPath);
        }
    }

    // API çağrısı (curl ile)
    void sendToAPI(const std::string& pan, const std::string& formattedExpiry) {
        logInfo("sendToAPI fonksiyonu başlatıldı");
        logInfo("PAN: " + pan);
        logInfo("Formatlanmış Expiry: " + formattedExpiry);

        std::string jsonData = "{\"pan\":\"" + pan + "\",\"expiryDate\":\"" + formattedExpiry + "\",\"expiry_date\":\"" + formattedExpiry + "\"}";
        logInfo("Gönderilecek JSON: " + jsonData);
//
        std::string command = "curl -s -w '%{http_code}' -X POST " + apiUrl + " -H 'Content-Type: application/json' -d '" + jsonData + "'";
        logInfo("API'ye gönderiliyor: " + command);

        /*std::string command =
            "curl -s "
            "-D /home/atilhan/logs/api_headers.log "
            "-o /home/atilhan/logs/api_body.log "
            "-w '%{http_code}' "
            "-X POST " + apiUrl +
            " -H 'Content-Type: application/json' "
            "-d '" + jsonData + "'";
        */

        auto startTime = std::chrono::high_resolution_clock::now();
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            logError("popen() başarısız!");
            sendToGUI("fail");
            return;
        }

        char buffer[256];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        logInfo("API yanıt süresi: " + formatDuration(duration));

        if (result.length() >= 3) {
            std::string httpCode = result.substr(result.length() - 3);
            std::string responseBody = result.substr(0, result.length() - 3);
            logInfo("HTTP Durum Kodu: " + httpCode);
            logInfo("API Yanıtı: " + responseBody);
            if (httpCode == "200" || httpCode == "201") {
                logSuccess("API çağrısı başarılı! Süre: " + formatDuration(duration));
                sendToGUI("success");
            } else {
                logError("API çağrısı başarısız! HTTP Kodu: " + httpCode + " Süre: " + formatDuration(duration));
                sendToGUI("fail");
            }
        } else {
            logError("Geçersiz API yanıtı! Süre: " + formatDuration(duration));
            sendToGUI("fail");
        }
    }

public:
    CardDataProcessor(const std::string& filePath = "/tmp/card_data_hex.txt",
                     const std::string& fifoPath = "/tmp/bus_payment_control",
                     const std::string& apiUrl = "https://api.yeri.az/api/v1/payment/card/")
        : filePath(filePath), fifoPath(fifoPath), apiUrl(apiUrl) {}

    void processCardData() {
        try {
            logInfo("processCardData başlatıldı");
            std::ifstream infile(filePath);
            if (!infile) {
                logError("Dosya açılamadı: " + filePath);
                return;
            }

            std::string hexData;
            std::getline(infile, hexData);
            infile.close();

            if (hexData.empty()) {
                logError("Hex verisi boş: " + filePath);
                return;
            }

            logInfo("Okunan Hex Verisi (başlangıç 50 char): " + (hexData.size() > 50 ? hexData.substr(0,50) + "..." : hexData));

            std::string pan, expiry;

            if (hexData.size() >= 6 && hexData.substr(0, 6) == "C1DFEE") {
                logInfo("C1DFEE formatı tespit edildi");
                size_t pos5A08 = hexData.find("5A08");
                logInfo("5A08 pozisyonu: " + std::to_string(pos5A08));
                size_t pos5F2403 = hexData.find("5F2403");
                logInfo("5F2403 pozisyonu: " + std::to_string(pos5F2403));
                pan = extractPANFromC1DFEE(hexData);
                expiry = extractExpiryFromC1DFEE(hexData);
            } else {
                logInfo("DFEF4D formatı (veya ascii track) tespit edildi");
                std::string asciiData = hexToAscii(hexData);
                logInfo("ASCII DATA (başlangıç 60 char): " + (asciiData.size() > 60 ? asciiData.substr(0,60) + "..." : asciiData));
                pan = extractPAN(asciiData);
                expiry = extractExpiry(asciiData);
            }

            if (pan.empty()) {
                logError("PAN bulunamadı.");
                sendToGUI("fail");
                return;
            }
            logInfo("Çıkarılan PAN: " + pan);

            if (expiry.empty()) {
                logError("Expiry bulunamadı.");
                writeToFIFO(pan, "");
                sendToGUI("fail");
                return;
            }
            logInfo("Çıkarılan Expiry (YYMM): " + expiry);

            if (expiry.length() != 4) {
                logError("Geçersiz expiry uzunluğu: " + std::to_string(expiry.length()));
                writeToFIFO(pan, expiry);
                sendToGUI("fail");
                return;
            }

            std::string formattedExpiry = expiry.substr(2, 2) + "/" + expiry.substr(0, 2);
            logInfo("Dönüştürülen Expiry (MM/YY): " + formattedExpiry);

            logInfo("FIFO'ya yazma işlemi başlıyor");
            writeToFIFO(pan, expiry);
            logInfo("FIFO'ya yazma işlemi bitti");

            logInfo("sendToAPI çağrısı yapılıyor");
            sendToAPI(pan, formattedExpiry);
            logInfo("sendToAPI çağrısı bitti");

        } catch (const std::exception& e) {
            logError(std::string("İstisna yakalandı: ") + e.what());
            sendToGUI("fail");
        } catch (...) {
            logError("Bilinmeyen istisna yakalandı");
            sendToGUI("fail");
        }
    }
};

int main() {
    try {
        std::cout << "Program başlatılıyor..." << std::endl;
        CardDataProcessor processor;
        std::cout << "processCardData çağrılıyor..." << std::endl;
        processor.processCardData();
        std::cout << "processCardData tamamlandı." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Hata yakalandı: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Bilinmeyen hata yakalandı!" << std::endl;
        return 1;
    }
    return 0;
}
