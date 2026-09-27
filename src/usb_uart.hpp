#ifndef USB_UART_HPP_
#define USB_UART_HPP_

namespace usb_uart
{
    using usb_sense_callback_t = void(*)(bool on);

    int setup();
    void set_callback(usb_sense_callback_t cb);
    bool usb_state();
}

#endif
