#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#include <termios.h>

#include <libusb.h>
#include "ftdi_sio.h"

struct termios tio, tio_stdin_orig;
struct pollfd pfd[2];

libusb_context          *ctx;
libusb_device           **devices;
libusb_device_handle    *dh;

#define VENDOR  0x0403
#define PRODUCT 0x6001
#define MANUFACTURER  "FTDI"
#define PRODUCT_NAME  "FT232R USB UART"

//  Pin Definitions
//  FT245BM FT232BM    Atmel pin
//  Data0     TXD      RxD
//  Data1     RXD      TxD
//  Data2     RTS      -
//  Data3     CTS      -
//  Data4     DTR      SCK
//  Data5     DSR      MISO
//  Data6     DCD      MOSI
//  Data7     RI       RESET

#define MASK_RESET  0x80
#define MASK_MOSI   0x40
#define MASK_MISO   0x20
#define MASK_SCLK   0x10

#define T           1000

bool
check_device_string(int id, const char *shouldbe) {
    char text[1024];

    if (id == 0)
        return false;

    int res = libusb_get_string_descriptor_ascii(dh, id, (uint8_t*)&text, sizeof(text));
    if (res < 0)
        return false;

    text[res] = '\0';
    return !strcmp(text, shouldbe);
}

uint8_t
read_pins() {
    uint8_t mask = 0;
    int res = libusb_control_transfer(dh, FTDI_SIO_READ_PINS_REQUEST_TYPE, FTDI_SIO_READ_PINS, 0, 0, &mask, 1, 200);
    if (res != 1)
        exit(5);
    return mask;
}

void
write_pins(uint8_t mask) {
    int n = 0;
    int res = libusb_bulk_transfer(dh, 0x02, &mask, 1, &n, 200);
    if (res < 0)
        exit(5);
    if (n < 1)
        exit(6);
}

void 
dump_pin(uint8_t value, const char *msg) {
    printf("\x1b[%dm%s\x1b[0m", value ? 31 : 32, msg);
}


uint8_t write_byte(uint8_t n) {
    uint8_t pins;
    uint8_t input = 0;

    printf("Writing 0x%02x\n", n);
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
        pins = read_pins();
        if (pins & MASK_MISO)
            input |= mask;
        if (n & mask)
            pins |= MASK_MOSI;
        else
            pins &= ~MASK_MOSI;
        write_pins(pins);

        usleep(T);

        pins |= MASK_SCLK;
        write_pins(pins);

        usleep(2 * T);
        
        pins &= ~MASK_SCLK;
        write_pins(pins);

        usleep(T);
    }
    printf("Read 0x%02x\n", input);
    return input;
}


int
main(int argc, const char **argv) {
    int res;

    fprintf(stderr, "-- Entering main\n");

    res = libusb_init(&ctx);
    if (res != LIBUSB_SUCCESS)
        exit(1); // die("libusb_init failed", res);

    int len = libusb_get_device_list(ctx, &devices);
    for (int i = 0; i < len; ++i) {
        libusb_device_descriptor descr_dev;

        res = libusb_get_device_descriptor(devices[i], &descr_dev);
        if (res != LIBUSB_SUCCESS)
            exit(2); // die("libusb_get_device_descriptor failed", res);

        if ((descr_dev.idVendor == VENDOR) && (descr_dev.idProduct == PRODUCT)) {
            res = libusb_open(devices[i], &dh);
            if (res != LIBUSB_SUCCESS)
                exit(3); // die("libusb_open failed", res);

            if (check_device_string(descr_dev.iManufacturer, MANUFACTURER) &&
                check_device_string(descr_dev.iProduct, PRODUCT_NAME)) {
                // gotcha!
                break;
            }
            libusb_close(dh);
            dh = NULL;
        }
    }
    libusb_free_device_list(devices, 1);
    if (!dh)
        exit(4); // die("Could not find device", LIBUSB_ERROR_OTHER);
    fprintf(stderr, "-- Found device\n");
    // now we have the device

    // reset device
    fprintf(stderr, "-- Resetting device\n");
    res = libusb_control_transfer(dh, FTDI_SIO_RESET_REQUEST_TYPE, FTDI_SIO_RESET_REQUEST, 0, 0, nullptr, 0, 4000);
    if (res != LIBUSB_SUCCESS) // FIXME: could not send data
        exit(5);
    
    // set bitbang mode
    fprintf(stderr, "-- Setting BB directions\n");
    uint16_t dir_mask = 0x100 | MASK_MOSI | MASK_SCLK | MASK_RESET; // 0=input, 1=output
    res = libusb_control_transfer(dh, FTDI_SIO_SET_BITMODE_REQUEST_TYPE, FTDI_SIO_SET_BITMODE, dir_mask, 0, nullptr, 0, 4000);
    if (res != LIBUSB_SUCCESS) // FIXME: could not send data
        exit(5);

    write_pins(0); // RESET = SCLK = MOSI = 0
    usleep(100);
    write_pins(MASK_RESET);
    usleep(20000);
    write_pins(0); // RESET = SCLK = MOSI = 0
    usleep(100);

#if 1
    printf("-- Device reset\n");
    write_byte(0xac);
    write_byte(0x53);
    write_byte(0x00);
    write_byte(0x00);

    for (int a = 0; a < 4; ++a) {
        printf("-- Read signature %d\n", a);
        write_byte(0x30);
        write_byte(0x00);
        write_byte(a);
        write_byte(0x00);
    }
    
    printf("-- Read lfuse\n");
    write_byte(0x50);
    write_byte(0x00);
    write_byte(0x00);
    write_byte(0x00);

    /*
    printf("-- WRITE lfuse\n");
    write_byte(0xac);
    write_byte(0xa0);
    write_byte(0x00);
    write_byte(0xa4);   // new lfuse value
    */

#else
    tcgetattr(0, &tio_stdin_orig);
    tcgetattr(0, &tio);
    tio.c_lflag &= ~(ICANON | ECHO);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &tio);

    pfd[0].fd = 0;
    pfd[0].events = POLLIN;

    fprintf(stderr, "-- Entering main loop\n");
    uint8_t pins = read_pins();
    while (true) {
        char key;

        pfd[0].revents = 0;
        res = poll(pfd, 1, 1000);

        if (pfd[0].revents & POLLIN) {
            len = read(0, &key, 1);
            if ((len < 0) && ((errno != EAGAIN) && (errno != EINTR)))
                break;
            if (len > 0) {
                if (key == '\x1b')
                    break;
                pins = read_pins();
                switch (key) {
                    case 'c': // sclk to low
                    pins &= ~MASK_SCLK;
                    break;

                    case 'C': // sclk to high
                    pins |= MASK_SCLK;
                    break;

                    case ' ': // sclk toggle
                    pins ^= MASK_SCLK;
                    break;

                    case '0': // mosi to low
                    pins &= ~MASK_MOSI;
                    break;

                    case '1': // mosi to high
                    pins |= MASK_MOSI;
                    break;

                    case 'r': // reset to low
                    pins &= ~MASK_RESET;
                    break;

                    case 'R': // reset to high
                    pins |= MASK_RESET;
                    break;
                }
                write_pins(pins);
            }
        }

        pins = read_pins();
        dump_pin(pins & MASK_RESET,  "RESET");
        printf(" ");
        dump_pin(pins & MASK_MOSI,   "MOSI");
        printf(" ");
        dump_pin(pins & MASK_MISO,   "MISO");
        printf(" ");
        dump_pin(pins & MASK_SCLK,   "SCLK");
        printf("\n");
    }

    tcsetattr(0, TCSANOW, &tio_stdin_orig);
#endif

    libusb_close(dh);
    libusb_exit(ctx);
    fprintf(stderr, "-- Exiting\n");
    return 0;
}

