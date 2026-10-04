/*
 * CE Controller
 * -------------
 * Turns a TI-84 Plus CE into a USB key sender. The calculator pretends to be
 * a USB serial (CDC) device using srldrvce and streams the state of its
 * keypad to whatever it is plugged into. The companion web page
 * (controller.html) reads the stream and does all the key mapping, so
 * remapping never needs a recompile or a re-send of this program.
 *
 * Frame format, calculator -> computer (9 bytes):
 *
 *   0xA5, g1, g2, g3, g4, g5, g6, g7, checksum
 *
 *   g1..g7    kb_Data[1..7]. One bit per key, 1 = pressed. Bit layout is the
 *             one documented in keypadc.h.
 *   checksum  g1 ^ g2 ^ g3 ^ g4 ^ g5 ^ g6 ^ g7
 *
 * A frame is sent whenever any key changes, and every 250 ms otherwise so a
 * receiver that connects late still catches up. Sending any byte to the
 * calculator makes it send a frame immediately.
 *
 * To quit: hold [Clear] and [Del] together.
 */

#include <srldrvce.h>

#include <keypadc.h>
#include <tice.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FRAME_SYNC       0xA5
#define KEY_GROUPS       7
#define FRAME_LEN        (KEY_GROUPS + 2)
#define HEARTBEAT_CYCLES 12000000UL /* 250 ms at 48 MHz */
#define SRL_BUF_SIZE     512

static srl_device_t srl;
static bool has_srl_device = false;
static uint8_t srl_buf[SRL_BUF_SIZE];

/*
 * USB event handler. This follows the srl_echo example from the CE toolchain:
 * when the calculator is plugged into a computer, the computer shows up as the
 * "host" device and we open it as a serial port.
 */
static usb_error_t handle_usb_event(usb_event_t event, void *event_data,
                                    usb_callback_data_t *callback_data) {
    usb_error_t err;

    /* Let srldrvce see every event first. */
    if ((err = srl_UsbEventCallback(event, event_data, callback_data)) != USB_SUCCESS)
        return err;

    /* Enable newly connected devices (only matters when the calc is the host). */
    if (event == USB_DEVICE_CONNECTED_EVENT && !(usb_GetRole() & USB_ROLE_DEVICE)) {
        usb_ResetDevice(event_data);
    }

    /* Open the serial port once the connection is configured. */
    if (event == USB_HOST_CONFIGURE_EVENT ||
        (event == USB_DEVICE_ENABLED_EVENT && !(usb_GetRole() & USB_ROLE_DEVICE))) {
        usb_device_t device;

        if (has_srl_device)
            return USB_SUCCESS;

        if (event == USB_HOST_CONFIGURE_EVENT) {
            /* The device representing the computer we are plugged into. */
            device = usb_FindDevice(NULL, NULL, USB_SKIP_HUBS);
            if (device == NULL)
                return USB_SUCCESS;
        } else {
            device = event_data;
        }

        srl_error_t error = srl_Open(&srl, device, srl_buf, sizeof srl_buf,
                                     SRL_INTERFACE_ANY, 9600);
        if (error) {
            printf("serial error %d\n", error);
            return USB_SUCCESS;
        }

        printf("connected\n");
        has_srl_device = true;
    }

    if (event == USB_DEVICE_DISCONNECTED_EVENT) {
        usb_device_t device = event_data;
        if (has_srl_device && device == srl.dev) {
            srl_Close(&srl);
            has_srl_device = false;
            printf("disconnected\n");
        }
    }

    return USB_SUCCESS;
}

static void send_frame(const uint8_t keys[KEY_GROUPS]) {
    uint8_t frame[FRAME_LEN];
    uint8_t checksum = 0;
    uint8_t i;

    frame[0] = FRAME_SYNC;
    for (i = 0; i < KEY_GROUPS; i++) {
        frame[1 + i] = keys[i];
        checksum ^= keys[i];
    }
    frame[FRAME_LEN - 1] = checksum;

    srl_Write(&srl, frame, FRAME_LEN);
}

int main(void) {
    uint8_t last[KEY_GROUPS] = {0};
    uint32_t last_send;
    bool force = true;
    usb_error_t usb_error;

    os_ClrHome();
    printf("CE CONTROLLER\n");
    printf("Plug into USB host\n");
    printf("Quit: Clear + Del\n");

    usb_error = usb_Init(handle_usb_event, NULL, srl_GetCDCStandardDescriptors(),
                         USB_DEFAULT_INIT_FLAGS);
    if (usb_error) {
        usb_Cleanup();
        printf("usb init error %u\n", usb_error);
        printf("Press Clear\n");
        do {
            kb_Scan();
        } while (!kb_IsDown(kb_KeyClear));
        return 1;
    }

    last_send = usb_GetCycleCounter();

    do {
        uint8_t now[KEY_GROUPS];
        uint8_t i;
        bool changed;

        kb_Scan();
        for (i = 0; i < KEY_GROUPS; i++)
            now[i] = kb_Data[i + 1];

        usb_HandleEvents();

        if (has_srl_device) {
            uint8_t rx[32];
            int n = srl_Read(&srl, rx, sizeof rx);

            if (n < 0)
                has_srl_device = false; /* error, wait for a reconnect */
            else if (n > 0)
                force = true;           /* host asked for a fresh frame */
        }

        changed = memcmp(now, last, KEY_GROUPS) != 0;

        if (has_srl_device &&
            (changed || force ||
             (uint32_t)(usb_GetCycleCounter() - last_send) >= HEARTBEAT_CYCLES)) {
            send_frame(now);
            last_send = usb_GetCycleCounter();
            force = false;
        }

        memcpy(last, now, KEY_GROUPS);
    } while (!(kb_IsDown(kb_KeyClear) && kb_IsDown(kb_KeyDel)));

    usb_Cleanup();
    return 0;
}
