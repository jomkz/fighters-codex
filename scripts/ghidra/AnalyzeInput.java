// Joystick, keyboard, and mouse input handling.
// Dark zone: 0x420000-0x42FFFF (64 KB) -- zero prior coverage.
// Also covers world-coordinate transforms and viewport mapping.
// Invoke: run_ghidra.sh AnalyzeInput.java
// Output: $FA_PROJECT/output/AnalyzeInput.txt

public class AnalyzeInput extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeInput");
        header("INPUT -- joystick init and poll");
        dumpSymbolsMatching("_joyinit", "joyinit", "_joypoll", "joypoll",
                "_joyread", "joyread", "_joyupdate", "joyupdate",
                "_joyopen", "joyopen", "_joyclose", "joyclose",
                "_joystick", "joystick", "_joycal", "joycal",
                "_joyaxis", "joyaxis", "_joybutton", "joybutton",
                "_dinputinit", "dinputinit", "_dinpoll", "dinpoll");
        searchStrings(new String[]{"joystick", "Joystick", "DirectInput", "IDirectInput",
                "joyGetDevCaps", "joyGetPos", "joyGetPosEx"});
        header("INPUT -- range 0x420000-0x42FFFF");
        dumpRange(0x00420000L, 0x0042ffffL);
        header("INPUT -- keyboard handling");
        dumpSymbolsMatching("_keyinit", "keyinit", "_keypoll", "keypoll",
                "_keyread", "keyread", "_keydown", "keydown", "_keyup", "keyup",
                "_keyboard", "keyboard", "_keybind", "keybind",
                "_hotkey", "hotkey", "_keymap", "keymap");
        searchStrings(new String[]{"GetAsyncKeyState", "GetKeyState", "keyboard", "Keyboard"});
        header("INPUT -- mouse handling");
        dumpSymbolsMatching("_mouseinit", "mouseinit", "_mousepoll", "mousepoll",
                "_mouseread", "mouseread", "_mousepos", "mousepos",
                "_mousebutton", "mousebutton", "_mousemove", "mousemove",
                "_cursorinit", "cursorinit", "_cursor", "cursor");
        searchStrings(new String[]{"GetCursorPos", "SetCursorPos", "ShowCursor",
                "mouse", "Mouse"});
        header("INPUT -- control mapping and deadzone");
        dumpSymbolsMatching("_ctrlmap", "ctrlmap", "_controlmap", "controlmap",
                "_deadzone", "deadzone", "_sensitivity", "sensitivity",
                "_calibrate", "calibrate", "_ctrlupdate", "ctrlupdate",
                "_inputmap", "inputmap", "_axisinvert", "axisinvert");
        header("WORLD -- ?MAPWorldToScreen (0x422380)");
        dumpAt(0x00422380L);
        header("WORLD -- world coordinate symbols");
        dumpSymbolsMatching("worldtoscreen", "screentoworld", "worldtovp",
                "mapworld", "_mapworld", "_worldcoord", "worldcoord",
                "_wtoscreen", "wtoscreen", "_screenproject", "screenproject");
        header("WORLD -- viewport and projection symbols");
        dumpSymbolsMatching("_vpinit", "vpinit", "_vpupdate", "vpupdate",
                "_project", "projectpoint", "_unproject", "unproject",
                "_screentransform", "screentransform");
        closeOutput();
    }
}
