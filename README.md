# CE Controller

Use a TI-84 Plus CE as a custom controller. The calculator program (CEPAD) sends
its keypad state over USB. The web page (`controller.html`) receives it and lets
you map every calculator key to any keyboard key.

Remapping happens entirely in the web page, so you never need to rebuild or
re-send the calculator program to change a mapping.

## Files

| File | What it is |
|---|---|
| `src/main.c`, `makefile` | The calculator program (C, CE toolchain). |
| `.github/workflows/build.yml` | Builds `CEPAD.8xp` on GitHub, so no install is needed. |
| `controller.html` | The mapping page. Open it in Chrome. No install. |

## 1. Build the calculator program (no install needed)

1. Create a new repository on github.com and upload everything in this folder.
   The `.github` folder is hidden in some file pickers. If the **Actions** tab
   shows no workflow afterwards, use **Add file > Create new file**, type
   `.github/workflows/build.yml` as the name, and paste in the contents.
2. Open the **Actions** tab, pick **Build CEPAD**, and click **Run workflow**
   (it also runs on every push).
3. When it finishes, open the run and download the **CEPAD** artifact. It is a
   zip containing `CEPAD.8xp` and `clibs.8xg`.
4. If `clibs.8xg` is missing from the zip, download it from
   <https://github.com/CE-Programming/libraries/releases/latest>.

## 2. Put it on the calculator

Send **both** files to the calculator the way you normally send programs:

- `clibs.8xg` (the CE C libraries, only needed once, and only if the calculator
  does not already have them)
- `CEPAD.8xp`

The calculator needs OS 5.4.0 or lower, or a jailbreak such as arTIfiCE, to run
C programs. Run it with `prgmCEPAD` from the home screen (older OS versions
need `Asm(prgmCEPAD)`), or launch it from a shell such as Cesium.

## 3. Use it

1. On the calculator, run CEPAD. It shows "Plug into USB host".
2. Plug the calculator into the Chromebook (or an Android phone with an OTG
   adapter).
3. Open `controller.html` in Chrome and click **Connect (Web Serial)**. Pick the
   calculator in the list. If Web Serial is missing or blocked, use
   **Connect (WebUSB)**.
4. Tap a key on the page, or just press it on the calculator, and choose what it
   sends. Mappings save automatically in the browser. Use "Show current mapping"
   for a text backup.

To quit CEPAD, hold **Clear** and **Del** together.

## What it can and cannot control

A web page can only send keyboard events to itself. The built-in test arena
responds to arrow keys, WASD and Space, and any web game you add to the same
page can listen for the same events. It cannot press keys in other apps, other
tabs, or on a phone's home screen. For that you would need either a browser
extension or a calculator that appears as a real USB keyboard or gamepad, which
is a different (and harder) build.

## Troubleshooting

- **Calculator not in the device list:** CEPAD must be running before you click
  Connect. The calculator appears as a serial device with ID `16C0:05E1`.
- **"Blocked" or SecurityError:** a managed Chromebook can disable Web Serial and
  WebUSB by policy. Nothing in this project can override that.
- **WebUSB fails to claim the interface:** try Web Serial instead.
- **Status says "no data":** CEPAD stopped or the cable was disturbed. Re-run
  CEPAD and reconnect.
- **Keys feel stuck:** the page releases everything when the calculator
  disconnects. Press Disconnect, then connect again.

## Protocol (for your own tools)

Calculator to computer, 9 bytes per frame:

```
0xA5, g1, g2, g3, g4, g5, g6, g7, checksum
```

`g1..g7` are the keypad groups from `keypadc.h` (`kb_Data[1..7]`), one bit per
key, 1 means pressed. `checksum` is the XOR of `g1..g7`. A frame is sent on every
change and every 250 ms as a heartbeat. Sending any byte to the calculator
requests an immediate frame.
