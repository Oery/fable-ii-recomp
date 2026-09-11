#!/usr/bin/env python3
# Virtual Xbox 360 pad via uinput. Persists while running; reads commands
# (one per line) from stdin: a, b, x, y, start, back, left, right, up, down,
# lb, rb, quit. Press = 120ms tap unless suffixed :<ms> (e.g. "a:600").
import sys
import time
from evdev import UInput, AbsInfo, ecodes as e

BTN = {
    'a': e.BTN_SOUTH, 'b': e.BTN_EAST, 'x': e.BTN_NORTH, 'y': e.BTN_WEST,
    'lb': e.BTN_TL, 'rb': e.BTN_TR, 'back': e.BTN_SELECT, 'start': e.BTN_START,
    'l3': e.BTN_THUMBL, 'r3': e.BTN_THUMBR,
}
HAT = {
    'left': (-1, 0), 'right': (1, 0), 'up': (0, -1), 'down': (0, 1),
}
CAP = {
    e.EV_KEY: list(BTN.values()),
    e.EV_ABS: [(e.ABS_X, AbsInfo(0, -32768, 32767, 16, 128, 0)),
               (e.ABS_Y, AbsInfo(0, -32768, 32767, 16, 128, 0)),
               (e.ABS_RX, AbsInfo(0, -32768, 32767, 16, 128, 0)),
               (e.ABS_RY, AbsInfo(0, -32768, 32767, 16, 128, 0)),
               (e.ABS_Z, AbsInfo(0, 0, 255, 0, 0, 0)),
               (e.ABS_RZ, AbsInfo(0, 0, 255, 0, 0, 0)),
               (e.ABS_HAT0X, AbsInfo(0, -1, 1, 0, 0, 0)),
               (e.ABS_HAT0Y, AbsInfo(0, -1, 1, 0, 0, 0))],
}
ui = UInput(CAP, name='Microsoft X-Box 360 pad', vendor=0x45E, product=0x28E,
            version=0x110, bustype=e.BUS_USB)
print('vpad ready', flush=True)
for raw in sys.stdin:
    cmd = raw.strip().lower()
    if not cmd:
        continue
    if cmd == 'quit':
        break
    ms = 120
    if raw.strip().lower().startswith('stick'):
        cmd = raw.strip().lower()
    else:
        if ':' in cmd:
            cmd, ms = cmd.split(':')[0], int(cmd.split(':')[1])
    if cmd.startswith('stick'):
        # stick:<x>,<y>[:<ms>]  x,y in -100..100 (left stick), e.g. stick:0,-100:3000
        parts = cmd.split(':')
        try:
            x, y = (int(v) for v in parts[1].split(','))
            sms = int(parts[2]) if len(parts) > 2 else ms
        except (IndexError, ValueError):
            print(f'bad stick cmd: {cmd}', flush=True)
            continue
        xv = max(-32767, min(32767, x * 32767 // 100))
        yv = max(-32767, min(32767, y * 32767 // 100))
        ui.write(e.EV_ABS, e.ABS_X, xv)
        ui.write(e.EV_ABS, e.ABS_Y, yv)
        ui.syn()
        time.sleep(sms / 1000)
        ui.write(e.EV_ABS, e.ABS_X, 0)
        ui.write(e.EV_ABS, e.ABS_Y, 0)
        ui.syn()
        print(f'sent {cmd}', flush=True)
        continue
    if cmd in BTN:
        ui.write(e.EV_KEY, BTN[cmd], 1)
        ui.syn()
        time.sleep(ms / 1000)
        ui.write(e.EV_KEY, BTN[cmd], 0)
        ui.syn()
    elif cmd in HAT:
        dx, dy = HAT[cmd]
        ui.write(e.EV_ABS, e.ABS_HAT0X, dx)
        ui.write(e.EV_ABS, e.ABS_HAT0Y, dy)
        ui.syn()
        time.sleep(ms / 1000)
        ui.write(e.EV_ABS, e.ABS_HAT0X, 0)
        ui.write(e.EV_ABS, e.ABS_HAT0Y, 0)
        ui.syn()
    else:
        print(f'unknown: {cmd}', flush=True)
        continue
    print(f'sent {cmd}', flush=True)
ui.close()
