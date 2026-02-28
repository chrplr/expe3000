/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "dlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

static speed_t get_baudrate(int baudrate) {
    switch (baudrate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B9600;
    }
}

dlp_io8g_t* dlp_new(const char* device, int baudrate) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("Error opening serial port");
        return NULL;
    }

    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) {
        perror("Error from tcgetattr");
        close(fd);
        return NULL;
    }

    cfsetospeed(&tty, get_baudrate(baudrate));
    cfsetispeed(&tty, get_baudrate(baudrate));

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls,
                                    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Error from tcsetattr");
        close(fd);
        return NULL;
    }

    dlp_io8g_t* dlp = (dlp_io8g_t*)malloc(sizeof(dlp_io8g_t));
    if (!dlp) {
        close(fd);
        return NULL;
    }
    dlp->fd = fd;

    // Ping to check the device (sending 0x27 is ')
    unsigned char ping_cmd = 0x27;
    if (write(fd, &ping_cmd, 1) != 1) {
        perror("Error writing ping");
        dlp_close(dlp);
        return NULL;
    }

    unsigned char buff[8];
    int n = read(fd, buff, 1);
    if (n != 1 || buff[0] != 'Q') {
        fprintf(stderr, "Device did not respond to ping correctly\n");
        dlp_close(dlp);
        return NULL;
    }

    // set BINARY mode for return values (sending 0x5C is \)
    unsigned char binary_cmd = 0x5C;
    if (write(fd, &binary_cmd, 1) != 1) {
        perror("Error writing binary command");
        dlp_close(dlp);
        return NULL;
    }

    return dlp;
}

void dlp_close(dlp_io8g_t* dlp) {
    if (dlp) {
        if (dlp->fd >= 0) {
            close(dlp->fd);
        }
        free(dlp);
    }
}

bool dlp_ping(dlp_io8g_t* dlp) {
    unsigned char ping_cmd = 0x27;
    if (write(dlp->fd, &ping_cmd, 1) != 1) return false;

    unsigned char buff[1];
    int n = read(dlp->fd, buff, 1);
    return (n == 1 && buff[0] == 'Q');
}

size_t dlp_read(dlp_io8g_t* dlp, unsigned char* states) {
    const char* cmds = "ASDFGHJK";
    
    // Clear buffers
    tcflush(dlp->fd, TCIOFLUSH);

    if (write(dlp->fd, cmds, 8) != 8) return 0;

    int total_read = 0;
    while (total_read < 8) {
        int n = read(dlp->fd, states + total_read, 8 - total_read);
        if (n <= 0) break;
        total_read += n;
    }

    return (size_t)total_read;
}

void dlp_set(dlp_io8g_t* dlp, const char* lines) {
    tcflush(dlp->fd, TCOFLUSH);
    if (write(dlp->fd, lines, strlen(lines)) < 0) {
        perror("write error in dlp_set");
    }
}

void dlp_unset(dlp_io8g_t* dlp, const char* lines) {
    char* cmd = strdup(lines);
    if (!cmd) return;

    for (int i = 0; cmd[i]; i++) {
        switch (cmd[i]) {
            case '1': cmd[i] = 'Q'; break;
            case '2': cmd[i] = 'W'; break;
            case '3': cmd[i] = 'E'; break;
            case '4': cmd[i] = 'R'; break;
            case '5': cmd[i] = 'T'; break;
            case '6': cmd[i] = 'Y'; break;
            case '7': cmd[i] = 'U'; break;
            case '8': cmd[i] = 'I'; break;
        }
    }

    tcflush(dlp->fd, TCOFLUSH);
    if (write(dlp->fd, cmd, strlen(cmd)) < 0) {
        perror("write error in dlp_unset");
    }
    free(cmd);
}

#else
/* Windows implementation */
#include <windows.h>

/* Redefine dlp_io8g_t for Windows to use HANDLE */
typedef struct {
    HANDLE hSerial;
} dlp_io8g_win_t;

dlp_io8g_t* dlp_new(const char* device, int baudrate) {
    char full_device[64];
    // COM ports higher than 9 need the \\.\ prefix
    if (strncmp(device, "COM", 3) == 0) {
        snprintf(full_device, sizeof(full_device), "\\\\.\\%s", device);
    } else {
        strncpy(full_device, device, sizeof(full_device) - 1);
    }

    HANDLE hSerial = CreateFileA(full_device,
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error opening serial port %s\n", device);
        return NULL;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error getting device state\n");
        CloseHandle(hSerial);
        return NULL;
    }

    dcbSerialParams.BaudRate = baudrate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE; // O_SYNC equivalent-ish

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error setting device state\n");
        CloseHandle(hSerial);
        return NULL;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 500;
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        fprintf(stderr, "Error setting timeouts\n");
        CloseHandle(hSerial);
        return NULL;
    }

    dlp_io8g_t* dlp = (dlp_io8g_t*)malloc(sizeof(dlp_io8g_t));
    if (!dlp) {
        CloseHandle(hSerial);
        return NULL;
    }
    /* We reuse the fd int to store the HANDLE on Windows. 
       This is hacky but avoids changing dlp_io8g_t in dlp.h which 
       would require more refactoring. */
    dlp->fd = (intptr_t)hSerial;

    // Ping
    unsigned char ping_cmd = 0x27;
    DWORD written;
    if (!WriteFile(hSerial, &ping_cmd, 1, &written, NULL) || written != 1) {
        fprintf(stderr, "Error writing ping\n");
        dlp_close(dlp);
        return NULL;
    }

    unsigned char buff[1];
    DWORD read_bytes;
    if (!ReadFile(hSerial, buff, 1, &read_bytes, NULL) || read_bytes != 1 || buff[0] != 'Q') {
        fprintf(stderr, "Device did not respond to ping correctly\n");
        dlp_close(dlp);
        return NULL;
    }

    // Binary mode
    unsigned char binary_cmd = 0x5C;
    WriteFile(hSerial, &binary_cmd, 1, &written, NULL);

    return dlp;
}

void dlp_close(dlp_io8g_t* dlp) {
    if (dlp) {
        CloseHandle((HANDLE)(intptr_t)dlp->fd);
        free(dlp);
    }
}

bool dlp_ping(dlp_io8g_t* dlp) {
    if (!dlp) return false;
    unsigned char ping_cmd = 0x27;
    DWORD written, read_bytes;
    unsigned char buff[1];
    HANDLE h = (HANDLE)(intptr_t)dlp->fd;
    if (!WriteFile(h, &ping_cmd, 1, &written, NULL)) return false;
    if (!ReadFile(h, buff, 1, &read_bytes, NULL)) return false;
    return (read_bytes == 1 && buff[0] == 'Q');
}

size_t dlp_read(dlp_io8g_t* dlp, unsigned char* states) {
    if (!dlp) return 0;
    const char* cmds = "ASDFGHJK";
    HANDLE h = (HANDLE)(intptr_t)dlp->fd;
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    
    DWORD written;
    if (!WriteFile(h, cmds, 8, &written, NULL)) return 0;

    DWORD total_read = 0;
    while (total_read < 8) {
        DWORD n;
        if (!ReadFile(h, states + total_read, 8 - total_read, &n, NULL) || n == 0) break;
        total_read += n;
    }
    return (size_t)total_read;
}

void dlp_set(dlp_io8g_t* dlp, const char* lines) {
    if (!dlp) return;
    HANDLE h = (HANDLE)(intptr_t)dlp->fd;
    DWORD written;
    WriteFile(h, lines, (DWORD)strlen(lines), &written, NULL);
}

void dlp_unset(dlp_io8g_t* dlp, const char* lines) {
    if (!dlp) return;
    HANDLE h = (HANDLE)(intptr_t)dlp->fd;
    char* cmd = _strdup(lines);
    if (!cmd) return;

    for (int i = 0; cmd[i]; i++) {
        switch (cmd[i]) {
            case '1': cmd[i] = 'Q'; break;
            case '2': cmd[i] = 'W'; break;
            case '3': cmd[i] = 'E'; break;
            case '4': cmd[i] = 'R'; break;
            case '5': cmd[i] = 'T'; break;
            case '6': cmd[i] = 'Y'; break;
            case '7': cmd[i] = 'U'; break;
            case '8': cmd[i] = 'I'; break;
        }
    }
    DWORD written;
    WriteFile(h, cmd, (DWORD)strlen(cmd), &written, NULL);
    free(cmd);
}

#endif
