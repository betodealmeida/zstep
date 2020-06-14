from typing import Optional

import mido


def get_ss_port() -> Optional[str]:
    for name in mido.get_input_names():
        if name.startswith("SSCOM"):
            return name


def get_opz_port() -> Optional[str]:
    for name in mido.get_input_names():
        if name.startswith("OP-Z"):
            return name


def main() -> None:
    ss_port = get_ss_port()
    opz_port = get_opz_port()

    # used for stop/start
    cc_104_value = 0
    last_control = "stop"

    outport = mido.open_output(opz_port)
    with mido.open_input(ss_port) as inport:
        for msg in inport:
            if msg.control == 104:
                if msg.value > cc_104_value:
                    if last_control == "stop":
                        outport.send(mido.Message("start"))
                    last_control = "start"
                else:
                    outport.send(mido.Message("stop"))
                    last_control = "stop"
                cc_104_value = msg.value
            else:
                outport.send(msg)


if __name__ == "__main__":
    main()
