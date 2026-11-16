// SPDX-License-Identifier: AGPL-3.0-only
// Melodee entry point: the sample-base runtime (base/src/runtime/) hosting
// MelodeeApp, plus Melodee's own tools (--self-test, previews, sound check).
#include "runtime/app.h"
#include "runtime/control_socket.h"
#include "runtime/inventory.h"
#include "runtime/options.h"
#include "runtime/run_app.h"
#include "drivers/usb_audio.h"
#include "drivers/usb_gadget.h"
#include "melodee/melodee_app.h"
#include "melodee/tools.h"
#include <sys/stat.h>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>

int main(int argc, char** argv) {
    umask(0077);
    try {
        if (auto code = melodee::run_tool(argc, argv)) return *code;
        auto options = parse_options(argc, argv, "melodee");
        switch (options.mode) {
        case Options::Mode::Run: {
            if (options.app != "melodee") throw std::runtime_error("This build runs only Melodee, not " + options.app);
            // INDIGO X DENIM LAB: USB-C widened here, before the runtime: 26 channels (main mix and twelve stereo
            // pairs), or the old 18 when the gadget refuses them. Done, so the runtime leaves the gadget as it is.
            if (options.usb_output == "auto" && options.usb_stems) {
                auto widened = widen_uac2_playback(UsbAudioOutput::channels);
                std::cout << "USB-C audio: " << describe(widened) << std::endl;
                if (widened.result == GadgetChange::Result::Failed)
                    std::cout << "USB-C audio, 18 channels: " << describe(widen_uac2_playback(18)) << std::endl;
                options.usb_stems = false;
            }
            auto app = std::make_unique<MelodeeApp>(); // Large: keep it off the stack.
            return run_app(*app, options);
        }
        case Options::Mode::Control: return send_control_command(options.runtime, options.command);
        case Options::Mode::ControlStream: return stream_control_commands(options.runtime);
        case Options::Mode::Hardware: return print_hardware();
        }
    } catch (const std::exception& error) {
        std::cerr << "melodee: " << error.what() << '\n';
    }
    return 1;
}
