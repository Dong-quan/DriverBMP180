#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <stdint.h>
#include <string.h>

// Macro IOCTL giống như trong driver kernel
#define IOCTL_MAGIC           'b'
#define IOCTL_READ_TEMP       _IOR(IOCTL_MAGIC, 0, long)
#define IOCTL_READ_PRESSURE   _IOWR(IOCTL_MAGIC, 1, struct bmp180_pressure_data)
#define IOCTL_READ_ALTITUDE   _IOR(IOCTL_MAGIC, 2, long)
#define IOCTL_GET_TEMP_LEVEL  _IOR(IOCTL_MAGIC, 3, int)

// Cấu trúc dữ liệu cho áp suất (phải giống với kernel)
struct bmp180_pressure_data {
    uint8_t oss;
    long pressure;
};

// Hàm in mức nhiệt độ
const char* classify_temp_level(int level) {
    switch (level) {
        case 0: return "Cold";
        case 1: return "Warm";
        case 2: return "Hot";
        default: return "Unknown";
    }
}

int main() {
    int fd;
    long temperature;
    struct bmp180_pressure_data pressure_data;
    long altitude;
    int temp_level;

    fd = open("/dev/bmp180", O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return EXIT_FAILURE;
    }

    while (1) {
        // Đọc nhiệt độ
        if (ioctl(fd, IOCTL_READ_TEMP, &temperature) < 0) {
            perror("Failed to read temperature");
            break;
        }
        printf("Temperature: %.1f °C\n", temperature / 10.0);

        // Phân loại mức nhiệt độ
        if (ioctl(fd, IOCTL_GET_TEMP_LEVEL, &temp_level) < 0) {
            perror("Failed to get temperature level");
            break;
        }
        printf("Temperature level: %s\n", classify_temp_level(temp_level));

        // Đọc áp suất (oss = 0 hoặc từ 0–3)
        pressure_data.oss = 0;
        if (ioctl(fd, IOCTL_READ_PRESSURE, &pressure_data) < 0) {
            perror("Failed to read pressure");
            break;
        }
        printf("Pressure: %.2f Pa\n", (double)pressure_data.pressure);

        // Tính toán độ cao
        if (ioctl(fd, IOCTL_READ_ALTITUDE, &altitude) < 0) {
            perror("Failed to read altitude");
            break;
        }
        printf("Altitude: %.2f m\n", altitude * 1.0);

        printf("-------------------------------\n");
        sleep(2);
    }

    close(fd);
    return EXIT_SUCCESS;
}
