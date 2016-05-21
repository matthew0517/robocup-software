#include "KickerBoard.hpp"
#include "kicker_commands.h"
#include <tuple>

using namespace std;

KickerBoard::KickerBoard(shared_ptr<SharedSPI> sharedSPI, PinName nCs,
                         PinName nReset, const string& progFilename)
    : AVR910(sharedSPI, nCs, nReset), _filename(progFilename) {}

bool KickerBoard::verify_param(const char* name, char expected,
                               int (AVR910::*paramMethod)(), char mask,
                               bool verbose) {
    if (verbose) printf("Checking %s...", name);
    int val = (*this.*paramMethod)();
    bool success = ((val & mask) == expected);
    if (verbose) {
        if (success)
            printf("done\r\n");
        else
            printf("Got unexpected value: 0x%X\r\n", val);
    }

    return success;
}

bool KickerBoard::flash(bool onlyIfDifferent, bool verbose) {
    // Check a few parameters before attempting to flash to ensure that we have
    // the right chip and it's connected correctly.
    auto checks = {
        make_tuple("Vendor ID", ATMEL_VENDOR_CODE, &AVR910::readVendorCode,
                   0xFF),
        make_tuple("Part Family", AVR_FAMILY_ID,
                   &AVR910::readPartFamilyAndFlashSize, AVR_FAMILY_MASK),
        make_tuple("Device ID", ATTINY84A_DEVICE_ID, &AVR910::readPartNumber,
                   0xFF),
    };
    for (auto& check : checks) {
        if (!verify_param(get<0>(check), get<1>(check), get<2>(check),
                          get<3>(check), verbose)) {
            return false;
        }
    }

    //  Open binary file to write to AVR.
    if (verbose) printf("Opening kicker firmware file...");
    FILE* fp = fopen(_filename.c_str(), "r");

    if (fp == NULL) {
        if (verbose)
            printf(
                "failed\r\nFailed to open binary. Check file path: "
                "'%s'\r\n\r\n",
                _filename.c_str());

        return false;
    } else {
        // Program it!
        if (verbose) printf("done\r\n");

        bool shouldProgram = true;
        // TODO: Fix memory checking
        /* if (onlyIfDifferent && */
        /*    (checkMemory(ATTINY84A_PAGESIZE, ATTINY84A_NUM_PAGES, fp, false)
         * == */
        /*     0)) */
        /*    shouldProgram = false; */

        if (!shouldProgram) {
            if (verbose)
                printf("kicker firmware is up-to-date, no need to flash\r\n");

            // exit programming mode by bringing nReset high
            exitProgramming();
        } else {
            if (verbose) printf("Starting device upload\r\n");
            bool nSuccess =
                program(fp, ATTINY84A_PAGESIZE, ATTINY84A_NUM_PAGES);

            if (verbose) printf("Device upload complete\r\n");

            if (nSuccess) {
                if (verbose) printf("FAILED\r\n");
            } else {
                if (verbose) printf("SUCCESS\r\n");
            }
        }

        fclose(fp);
    }
    return true;
}

// this function enforces the design choice that each cmd must have an arg
void KickerBoard::send_to_kicker(const uint8_t cmd, const uint8_t arg) {
    _spi->write(cmd);
    _spi->write(arg);
}

void KickerBoard::kick(uint8_t time) {
    chipSelect();
    send_to_kicker(KICK_CMD, time);
    chipDeselect();
}

void KickerBoard::chip(uint8_t time) {
    chipSelect();
    send_to_kicker(CHIP_CMD, time);
    chipDeselect();
}

uint8_t KickerBoard::read_voltage() {
    chipSelect();
    send_to_kicker(GET_VOLTAGE_CMD, NOP_ARG); // send our request
    uint8_t volts = _spi->write(NOP_ARG); // recieve voltage
    chipDeselect();
    return volts;
}

void KickerBoard::charge() {
    chipSelect();
    send_to_kicker(SET_CHARGE_CMD, ON_ARG);
    chipDeselect();
}

void KickerBoard::stop_charging() {
    chipSelect();
    send_to_kicker(SET_CHARGE_CMD, OFF_ARG);
    chipDeselect();
}
