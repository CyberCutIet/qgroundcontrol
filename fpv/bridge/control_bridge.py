import asyncio
import errno
import socket
import struct
from contextlib import suppress

import evdev
from bridge_status import BridgeStatus

PORT = 7002
POCKET_NAME = "EdgeTX Radiomaster Pocket Joystick"

RC_MIN = 192
RC_MID = 992
RC_MAX = 1792

AXIS_CHANNELS = {
    0: 0,  # CH1
    1: 1,  # CH2
    2: 2,  # CH3
    3: 3,  # CH4
    5: 6,  # AUX3 / CH7
    6: 7,  # AUX4 / CH8
    7: 8,  # AUX5 / CH9
    4: 11,  # AUX8 / CH12 - knob
}

BUTTONS = {
    304: 4,  # AUX1 / CH5
    305: 5,  # AUX2 / CH6
}

PACKER = struct.Struct("<16H")


def scale(value, info):
    if not info or info.max == info.min:
        return RC_MID

    value = max(info.min, min(info.max, value))

    return int(RC_MIN + (value - info.min) * (RC_MAX - RC_MIN) / (info.max - info.min))


def find_pockets():
    found = []

    for path in evdev.list_devices():
        try:
            dev = evdev.InputDevice(path)

            if dev.name == POCKET_NAME:
                found.append(path)

            dev.close()

        except OSError:
            pass

    return found


async def run_pocket(path, sock, status):
    status.set("error", "opening")
    dev = evdev.InputDevice(path)
    tasks = []

    try:
        dev.grab()

        caps = dev.capabilities(absinfo=True)
        abs_info = dict(caps.get(evdev.ecodes.EV_ABS, []))

        axes = {code: info.value for code, info in abs_info.items()}

        buttons = set(dev.active_keys())

        async def reader():
            async for event in dev.async_read_loop():
                if event.type == evdev.ecodes.EV_ABS:
                    axes[event.code] = event.value

                elif event.type == evdev.ecodes.EV_KEY:
                    if event.value:
                        buttons.add(event.code)
                    else:
                        buttons.discard(event.code)

        async def sender():
            loop = asyncio.get_running_loop()

            period = 1.0 / HZ
            next_send = loop.time()

            while True:
                rc = [RC_MID] * 16

                for code, channel in AXIS_CHANNELS.items():
                    if code in abs_info:
                        rc[channel] = scale(
                            axes.get(code, abs_info[code].value),
                            abs_info[code],
                        )

                for code, channel in BUTTONS.items():
                    rc[channel] = RC_MAX if code in buttons else RC_MIN

                sock.sendto(
                    PACKER.pack(*rc),
                    (TARGET_IP, PORT),
                )
                status.set("ready")

                next_send += period
                delay = next_send - loop.time()

                if delay > 0:
                    await asyncio.sleep(delay)
                else:
                    next_send = loop.time()

        reader_task = asyncio.create_task(reader())
        sender_task = asyncio.create_task(sender())
        tasks = [reader_task, sender_task]

        done, pending = await asyncio.wait(
            (reader_task, sender_task),
            return_when=asyncio.FIRST_COMPLETED,
        )

        for task in pending:
            task.cancel()

        await asyncio.gather(
            *pending,
            return_exceptions=True,
        )

        for task in done:
            task.result()

    finally:
        # Also stop both tasks if main() is cancelled (for example, Ctrl+C).
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        status.set("disconnected")
        with suppress(OSError):
            dev.ungrab()

        dev.close()


async def main():
    sock = socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM,
    )
    status = BridgeStatus()
    status_task = asyncio.create_task(status.publish())

    try:
        while True:
            pockets = find_pockets()

            if not pockets:
                status.set("disconnected")
                await asyncio.sleep(0.5)
                continue

            if len(pockets) > 1:
                status.set("error", "ambiguous")
                await asyncio.sleep(0.5)
                continue

            try:
                await run_pocket(
                    pockets[0],
                    sock,
                    status,
                )

            except OSError as error:
                reason = {
                    errno.EACCES: "permission",
                    errno.EPERM: "permission",
                    errno.EBUSY: "busy",
                }.get(error.errno, "io")
                status.set("error", reason)
                await asyncio.sleep(0.5)

    finally:
        status_task.cancel()
        await asyncio.gather(status_task, return_exceptions=True)
        sock.close()


# CONFIG

TARGET_IP = "10.10.10.13"
HZ = 100


if __name__ == "__main__":
    with suppress(KeyboardInterrupt):
        asyncio.run(main())
