#include <iostream>
#include <cstdlib>
#include <signal.h>
#include <rtmidi/RtMidi.h>
#include <unistd.h>

bool done;
static void finish(int ignore){ done = true; }
std::vector<unsigned char> start{ 250 };
std::vector<unsigned char> continue_{ 251 };
std::vector<unsigned char> stop{ 252 };
std::vector<unsigned char> next{ 176, 103, 16 };
std::vector<unsigned char> prev{ 176, 103, 17 };

int main() {
    RtMidiIn *ss2in = new RtMidiIn();
    RtMidiIn *opzin = new RtMidiIn();
    RtMidiOut *opzout = new RtMidiOut();
    unsigned int nPorts;
    std::string portName;
    std::vector<unsigned char> message;
    std::vector<unsigned char> pattern_change;
    double stamp;
    int nBytes;
    int last_cc104_value, tick;
    bool running;

    // find input port for Softstep2
    nPorts = ss2in->getPortCount();
    for (unsigned int i = 0; i < nPorts; i++) {
        try {
            portName = ss2in->getPortName(i);
        } catch (RtMidiError &error) {
            error.printMessage();
            goto cleanup;
        }
        if (portName.rfind("SSCOM", 0) == 0) {
            ss2in->openPort(i);
            ss2in->ignoreTypes(false, false, false);
            break;
        }
    }

    // find input port for OP-Z 
    nPorts = opzin->getPortCount();
    for (unsigned int i = 0; i < nPorts; i++) {
        try {
            portName = opzin->getPortName(i);
        } catch (RtMidiError &error) {
            error.printMessage();
            goto cleanup;
        }
        if (portName.rfind("OP-Z", 0) == 0) {
            opzin->openPort(i);
            opzin->ignoreTypes(false, false, false);
            break;
        }
    }

    // find output port for OP-Z 
    nPorts = opzout->getPortCount();
    for (unsigned int i = 0; i < nPorts; i++) {
        try {
            portName = opzout->getPortName(i);
        } catch (RtMidiError &error) {
            error.printMessage();
            goto cleanup;
        }
        if (portName.rfind("OP-Z", 0) == 0) {
            opzout->openPort(i);
            break;
        }
    }

    done = false;
    (void) signal(SIGINT, finish);
    last_cc104_value = 0;
    running = false;
    tick = 0;

    while (!done) {
        // read clock from OP-Z
        stamp = opzin->getMessage(&message);
        nBytes = message.size();
        if (nBytes > 0) {
            // start
            if ((int)message[0] == 250) {
                running = true;
                tick = 0;
            }
            // continue
            else if ((int)message[0] == 251) {
                running = !running;
            }
            // stop
            else if ((int)message[0] == 252) {
                running = false;
            }

            // clock
            if (running && ((int)message[0] == 248)) {
                tick = (tick + 1) % 96;

                // send queued pattern change
                if ((tick == 0) && (!pattern_change.empty())) {
                    std::cout << "sending pattern change" << std::endl;
                    opzout->sendMessage(&pattern_change);
                    pattern_change.clear();
                }
            }
        }
            
        // read input from the Softstep 2
        stamp = ss2in->getMessage(&message);
        nBytes = message.size();
        if (nBytes > 0) {
            // handle start/stop on CC 104
            if ((int)message[1] == 104) {
                if ((int)message[2] > last_cc104_value) {
                    if (!running) {
                        opzout->sendMessage(&start);
                        std::cout << "sending start" << std::endl;
                    }
                    running = true;
                    tick = 0;
                } else {
                    if (running) {
                        opzout->sendMessage(&stop);
                        std::cout << "sending stop" << std::endl;
                    }
                    running = false;
                }
                last_cc104_value = (int)message[2];
            }
            // forward mute groups on CC 55
            else if ((int)message[1] == 55) {
                opzout->sendMessage(&message);
            }

            // queue pattern change for next bar
            else if ((message == next) || (message == prev)) {
                pattern_change = message;
            }
        }
    }

    cleanup:
        delete ss2in;
        delete opzin;
        delete opzout;

    return 0;
}
