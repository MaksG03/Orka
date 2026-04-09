// ═══════════════════════════════════════════════════════
// Selection Monitor — читання виділеного тексту
// Windows: SendInput (Ctrl+C) + Clipboard API
// Linux:   X11 PRIMARY selection via x11rb
// ═══════════════════════════════════════════════════════

#[cfg(target_os = "windows")]
pub mod windows {
    use std::thread::sleep;
    use std::time::Duration;
    use ::windows::Win32::Foundation::*;
    use ::windows::Win32::System::DataExchange::*;
    use ::windows::Win32::System::Memory::*;
    use ::windows::Win32::UI::Input::KeyboardAndMouse::*;
    use ::windows::Win32::UI::WindowsAndMessaging::CF_UNICODETEXT;

    pub fn get_selected_text() -> Option<String> {
        unsafe {
            let prev = get_clipboard_text();
            clear_clipboard();
            send_ctrl_c();
            sleep(Duration::from_millis(60));
            let selected = get_clipboard_text();
            match &prev {
                Some(t) => set_clipboard_text(t),
                None    => clear_clipboard(),
            }
            if selected == prev { return None; }
            selected.filter(|s| !s.trim().is_empty())
        }
    }

    pub fn inject_text(text: &str) {
        unsafe {
            set_clipboard_text(text);
            sleep(Duration::from_millis(20));
            send_ctrl_v();
            sleep(Duration::from_millis(20));
        }
    }

    unsafe fn send_ctrl_c() { send_combo(VK_CONTROL, VK_C); }
    unsafe fn send_ctrl_v() { send_combo(VK_CONTROL, VK_V); }

    unsafe fn send_combo(modifier: VIRTUAL_KEY, key: VIRTUAL_KEY) {
        let inputs = [
            make_input(modifier, KEYBD_EVENT_FLAGS(0)),
            make_input(key,      KEYBD_EVENT_FLAGS(0)),
            make_input(key,      KEYEVENTF_KEYUP),
            make_input(modifier, KEYEVENTF_KEYUP),
        ];
        SendInput(&inputs, std::mem::size_of::<INPUT>() as i32);
    }

    unsafe fn make_input(vk: VIRTUAL_KEY, flags: KEYBD_EVENT_FLAGS) -> INPUT {
        INPUT {
            r#type: INPUT_KEYBOARD,
            Anonymous: INPUT_0 {
                ki: KEYBDINPUT {
                    wVk: vk, wScan: 0,
                    dwFlags: flags, time: 0, dwExtraInfo: 0,
                },
            },
        }
    }

    unsafe fn get_clipboard_text() -> Option<String> {
        if OpenClipboard(HWND(0)).is_err() { return None; }
        let result = GetClipboardData(CF_UNICODETEXT.0 as u32).ok()
            .and_then(|h| {
                let ptr = GlobalLock(h.0 as *mut _) as *const u16;
                if ptr.is_null() { return None; }
                let len = (0..).take_while(|&i| *ptr.add(i) != 0).count();
                let text = String::from_utf16_lossy(
                    std::slice::from_raw_parts(ptr, len)
                ).to_string();
                GlobalUnlock(h.0 as *mut _);
                Some(text)
            });
        let _ = CloseClipboard();
        result
    }

    unsafe fn set_clipboard_text(text: &str) {
        if OpenClipboard(HWND(0)).is_err() { return; }
        let _ = EmptyClipboard();
        let wide: Vec<u16> = text.encode_utf16()
            .chain(std::iter::once(0)).collect();
        if let Ok(h) = GlobalAlloc(GMEM_MOVEABLE, wide.len() * 2) {
            let ptr = GlobalLock(h.0 as *mut _) as *mut u16;
            if !ptr.is_null() {
                std::ptr::copy_nonoverlapping(wide.as_ptr(), ptr, wide.len());
                GlobalUnlock(h.0 as *mut _);
                let _ = SetClipboardData(
                    CF_UNICODETEXT.0 as u32,
                    HANDLE(h.0 as isize)
                );
            }
        }
        let _ = CloseClipboard();
    }

    unsafe fn clear_clipboard() {
        if OpenClipboard(HWND(0)).is_ok() {
            let _ = EmptyClipboard();
            let _ = CloseClipboard();
        }
    }
}

#[cfg(target_os = "linux")]
pub mod linux {
    use x11rb::connection::Connection;
    use x11rb::protocol::xproto::*;
    use x11rb::protocol::xfixes::ConnectionExt as XFixesExt;
    use x11rb::protocol::xfixes::SelectionEventMask;

    pub fn get_selected_text() -> Option<String> {
        let (conn, screen_num) = x11rb::connect(None).ok()?;
        let screen = &conn.setup().roots[screen_num];
        let root = screen.root;

        let _ = conn.xfixes_query_version(5, 0).ok()?.reply().ok()?;
        conn.xfixes_select_selection_input(
            root,
            AtomEnum::PRIMARY.into(),
            SelectionEventMask::SET_SELECTION_OWNER,
        ).ok()?;

        let utf8 = conn.intern_atom(false, b"UTF8_STRING")
            .ok()?.reply().ok()?.atom;
        let orka_prop = conn.intern_atom(false, b"ORKA_SELECTION")
            .ok()?.reply().ok()?.atom;

        let window = conn.generate_id().ok()?;
        conn.create_window(
            0, window, root, -1, -1, 1, 1, 0,
            WindowClass::INPUT_ONLY, 0,
            &CreateWindowAux::default(),
        ).ok()?;

        conn.convert_selection(
            window,
            AtomEnum::PRIMARY.into(),
            utf8, orka_prop,
            x11rb::CURRENT_TIME,
        ).ok()?;
        conn.flush().ok()?;

        let start = std::time::Instant::now();
        loop {
            if start.elapsed().as_millis() > 500 { return None; }
            if let Ok(Some(event)) = conn.poll_for_event() {
                use x11rb::protocol::Event::*;
                if let SelectionNotify(e) = event {
                    if e.property == x11rb::NONE { return None; }
                    let reply = conn.get_property(
                        true, window, orka_prop,
                        AtomEnum::ANY, 0, u32::MAX,
                    ).ok()?.reply().ok()?;
                    conn.destroy_window(window).ok()?;
                    return String::from_utf8(reply.value).ok()
                        .filter(|s| !s.trim().is_empty());
                }
            }
            std::thread::sleep(std::time::Duration::from_millis(10));
        }
    }

    pub fn inject_text(text: &str) {
        std::process::Command::new("xdotool")
            .args(["type", "--clearmodifiers", "--delay", "0", text])
            .spawn().ok();
    }
}
