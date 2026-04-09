**ORKA**

Intelligent Keyboard Layout Converter

**Технічна специфікація · Версія 2.5**

  ------------------ --------------------------------------------------
  **Розробник**      Perepelytsia Orka Technologies

  **Платформи**      Windows 10/11 · Linux (X11/Wayland)

  **Мови**           EN↔UK (v1.0) · EN↔KO (v1.5) · EN↔HE (v2.0)

  **Горизонт**       v1.0 → v2.0 · \~12 місяців

  **Статус doc.**    Технічна специфікація v2.5 --- видалено
                     macOS-розділи, додано 3 технічні виправлення
  ------------------ --------------------------------------------------

  --------------------------------------------------------------------
  **⚠ Сфера документа**

  Цей документ містить ВИКЛЮЧНО технічну специфікацію: архітектуру,
  API, алгоритми, тест-кейси, системні вимоги та реєстр ризиків.

  Платформи: Windows 10/11 та Linux (X11/Wayland). macOS виключено з
  поточного скопу.

  Маркетингова стратегія, GTM-плани, канали розповсюдження та
  конкурентний аналіз --- виключені.
  --------------------------------------------------------------------

**1. Зведення технічного аудиту v2.5**

Цей документ є результатом третього проходу технічного аудиту (v2.5).
Нижче --- повний перелік виявлених і виправлених проблем додатково до
тих, що були у v2.3.

  -------- ----------------------- --------------- ------------------------------------------ --------------
  **\#**   **Проблема**            **Категорія**   **Виправлення**                            **Статус**

  1        §3.2: Language Engine   Хибне посилання Замінено на §3.4                           **✅
           --- критерій \"§10.1                                                               Виправлено**
           green\" (§10.1 =                                                                   
           маркетинг, не                                                                      
           тест-кейси)                                                                        

  2        Таблиця §1: macOS       Хибне посилання Замінено на §3.4 / §6                      **✅
           архітектура \"додана у                                                             Виправлено**
           §3 (в §5)\" --- реально                                                            
           в §6.3                                                                             

  3        §5.2: \"суперечність    Хибне посилання Описові посилання без §-нумерації          **✅
           між §4.2.3 та §5.2\"                                                               Виправлено**
           --- нумерація старого                                                              
           документа                                                                          

  4        §9: \"vtrata даних\"    Друкарська      \"втрата даних\"                           **✅
           --- латинська розкладка помилка                                                    Виправлено**

  5        \"ICU ibidi\" ---       Технічна        \"ICU (BiDi, UAX #9)\" --- коректна назва  **✅
           бібліотеки не існує (3  помилка                                                    Виправлено**
           згадки)                                                                            

  6        §6.3 Permissions:       Технічна        \"System Settings → Privacy & Security →   **✅
           \"Privacy →             помилка         Input Monitoring\"                         Виправлено**
           Accessibility\" ---                                                                
           неправильний розділ                                                                
           macOS                                                                              

  7        §6.3 macOS Selection    Концептуальна   AXUIElement (kAXSelectedTextAttribute) +   **✅
           Monitor:                помилка         CGEventTap для детекції виділення          Виправлено**
           \"NSPasteboard +                                                                   
           changeCount polling\"                                                              
           --- NSPasteboard =                                                                 
           clipboard, НЕ виділений                                                            
           текст                                                                              

  8        §3.2 Selection Monitor  Технічна        Замінено на IAccessible (MSAA) / UI        **✅
           Windows: \"Fallback:    помилка         Automation --- правильні Windows API       Виправлено**
           polling IAccessible2\"                                                             
           --- IAccessible2 =                                                                 
           Linux/Mozilla API                                                                  

  9        §3.2 Overlay UI: тільки Неповнота       Додана Linux overlay специфікація для X11  **✅
           Windows-специфікація,                   та wlroots                                 Виправлено**
           відсутня Linux                                                                     
           (X11/Wayland)                                                                      

  10       §8 macOS: \"Input       Неповнота       Додано обидва дозволи: Input Monitoring +  **✅
           Monitoring permission\"                 Accessibility                              Виправлено**
           --- неповно;                                                                       
           AXUIElement також                                                                  
           потребує Accessibility                                                             

  11       Увесь розділ macOS      Скоп            Видалено §3.4, §4 macOS рядки, §5 macOS    **✅
           (v2.0) --- платформа                    рядок, §6 macOS рядок, §8.4, §9 macOS      Виправлено**
           виключена з поточного                   стовпець, §12 macOS критерії, §11 macOS    
           скопу (v1.0--v2.0                       ризики                                     
           Win/Linux)                                                                         

  12       EDR false positive:     Технічна        Умовна компіляція #ifndef                  **✅
           глобальні хуки у        помилка         ENTERPRISE_BUILD; Enterprise --- тільки    Виправлено**
           Enterprise-білді                        UIA Events + RegisterHotKey                

  13       Windows: clipboard      Технічна        IUIAutomationTextPattern::GetSelection()   **✅
           notification \"Orka     помилка         як пріоритет; OpenClipboard --- останній   Виправлено**
           pasted from\...\" у Win                 fallback з відновленням токена власника    
           11                                                                                 
  -------- ----------------------- --------------- ------------------------------------------ --------------

**2. Загальна архітектура**

ORKA складається з чотирьох незалежних компонентів, що взаємодіють через
внутрішній IPC-канал:

  --------------- ----------------------- ------------------------------
  **Компонент**   **Відповідальність**    **Ключові API (Win + Linux)**

  **Selection     Детектує кінець         Win: WH_MOUSE_LL + UI
  Monitor**       виділення тексту;       Automation / IAccessible
                  отримує виділений текст (MSAA) Linux X11:
                                          XFixesSelectSelectionInput
                                          Linux Wayland:
                                          wlr-data-control /
                                          zwp-primary-selection

  **Language      Конвертує текст між     EN↔UK: Unicode code point
  Engine**        розкладками із          mapping EN↔KO: libhangul (MIT)
                  збереженням регістру,   EN↔HE: ICU (BiDi, UAX #9)
                  пунктуації та Bidi      

  **Text          Вставляє конвертований  Win: SendInput /
  Injector**      текст у цільове поле    IUIAutomationTextPattern /
                  відповідним методом     OpenClipboard (fallback)
                                          Linux: xdotool / wl-clipboard

  **Overlay UI**  Прозора плаваюча кнопка Win: WS_EX_LAYERED \|
                  з intent detection (300 WS_EX_TOOLWINDOW Linux X11:
                  мс)                     libX11 override-redirect Linux
                                          Wayland/wlroots: layer-shell
                                          protocol
  --------------- ----------------------- ------------------------------

  --------------------------------------------------------------------
  **Архітектурний принцип**

  Жоден компонент не логує індивідуальні натискання клавіш. Selection
  Monitor відстежує виключно КІНЕЦЬ події виділення (mouse-up,
  Shift+arrow), а не послідовності клавіш.

  Це є обов\'язковою умовою для проходження MS Store WACK.

  Enterprise-збірка (#ifndef ENTERPRISE_BUILD): хуки
  WH_MOUSE_LL/WH_KEYBOARD_LL фізично виключені з бінарника. Детекція
  --- виключно через UIA Events.
  --------------------------------------------------------------------

**3. Selection Monitor**

**3.1 Windows --- Consumer збірка**

Основний механізм детекції виділення:

- Встановити WH_MOUSE_LL + WH_KEYBOARD_LL у low-priority thread. Timeout
  хука ≤ 5 мс --- вимога Windows для сумісності з EDR/AV.

- На події mouse-up або Shift+Arrow --- отримати виділення через UI
  Automation: IUIAutomation → IUIAutomationTextPattern::GetSelection().

- Fallback --- polling IAccessible (MSAA): AccessibleObjectFromWindow →
  get_accSelection() кожні 80 мс при активному вікні. Примітка:
  IAccessible2 є Linux/Mozilla API і не застосовується на Windows.

  --------------------------------------------------------------------
  **EDR-сумісність (Consumer)**

  WH_MOUSE_LL та WH_KEYBOARD_LL у low-priority thread з timeout ≤ 5 мс
  відповідають вимогам Windows hook policy.

  Для корпоративних середовищ, де EDR блокує хуки: переключитися на UI
  Automation polling (без хуків) або на Hotkey mode як повний
  fallback.

  Необхідно надати whitelist documentation для IT-адміністраторів з
  хешами та publisher-підписом.
  --------------------------------------------------------------------

**3.2 Windows --- Enterprise збірка (#ifndef ENTERPRISE_BUILD)**

Технічне виправлення проблеми EDR false positive у корпоративних
середовищах:

  --------------------------------------------------------------------
  **✅ Виправлення: умовна компіляція для Enterprise**

  Код хуків WH_MOUSE_LL / WH_KEYBOARD_LL фізично вирізаний з
  Enterprise-бінарника через #ifndef ENTERPRISE_BUILD.

  Детекція виділення: підписка на UIA_Text_TextSelectionChangedEventId
  (подієва архітектура, без polling).

  Виклик конвертації: виключно через RegisterHotKey --- нуль
  глобальних хуків у процесі.

  Це усуває саму причину EDR-реакції (CrowdStrike, SentinelOne): факт
  встановлення глобального хука.
  --------------------------------------------------------------------

> #ifdef ENTERPRISE_BUILD
>
> // Хуки повністю відсутні --- тільки UIA Events
>
> IUIAutomation::AddAutomationEventHandler(
>
> UIA_Text_TextSelectionChangedEventId, \...);
>
> RegisterHotKey(HWND_MESSAGE, id, MOD_CONTROL\|MOD_SHIFT, \'O\');
>
> #else
>
> // Consumer: low-priority hooks
>
> SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, \...);
>
> #endif

**3.3 Linux --- X11**

- Підписатися на зміни PRIMARY selection:
  XFixesSelectSelectionInput(display, root, XA_PRIMARY,
  XFixesSetSelectionOwnerNotifyMask).

- На XFixesSelectionNotifyEvent: викликати XGetSelectionOwner →
  XConvertSelection(XA_PRIMARY, XA_UTF8_STRING) для отримання тексту.

**3.4 Linux --- Wayland**

  ----------------- ------------------------------------- ------------------------
  **Compositor**    **Протокол**                          **Режим Overlay**

  **X11 (будь-який  XFixes PRIMARY selection              Так (libX11
  DE)**                                                   override-redirect)

  **Wayland +       zwlr-data-control-v1                  Так (layer-shell)
  wlroots (KDE      (wlr-data-control)                    
  Plasma, Sway)**                                         

  **Wayland + GNOME xdg-desktop-portal (D-Bus)            НІ --- тільки Hotkey
  (Ubuntu 22.04+)**                                       mode (обов\'язковий
                                                          fallback)

  **Wayland +       zwp-primary-selection-unstable-v1 +   Так (через розширення)
  GNOME +           D-Bus portal                          
  розширення**                                            
  ----------------- ------------------------------------- ------------------------

  --------------------------------------------------------------------
  **⚠ Wayland / GNOME --- обмеження**

  GNOME Wayland за замовчуванням забороняє доступ до PRIMARY selection
  без розширення. Overlay-кнопка недоступна.

  При першому запуску на GNOME Wayland показати UX-повідомлення:

  \"На вашому робочому столі (GNOME Wayland) плаваюча кнопка
  недоступна. Orka працює через Ctrl+Shift+O. Це можна змінити у
  Налаштуваннях.\"

  Hotkey mode (§6) є повноцінною альтернативою для всіх платформ, де
  overlay недоступний.
  --------------------------------------------------------------------

**4. Text Injector --- матриця пріоритетів**

Метод вставки обирається за пріоритетом залежно від типу поля та
платформи:

  -------------- ---------------- --------------------------------------- --------------------
  **Сценарій**   **Метод          **API**                                 **Примітка**
                 (пріоритет)**                                            

  Win ---        **1. SendInput** SendInput(INPUT_KEYBOARD,               Найшвидший, без
  стандартне                      KEYEVENTF_UNICODE)                      залежностей
  поле                                                                    

  Win --- Rich   **2. UIA         IUIAutomationValuePattern::SetValue()   Зберігає
  text / Office  SetValue**       або IUIAutomationTextPattern            форматування

  Win ---        **3.             OpenClipboard →                         SendInput
  захищене поле  OpenClipboard    SetClipboardData(CF_UNICODETEXT);       заблоковано ОС;
  (UAC,          (fallback)**     зберегти/відновити попереднього         обов\'язкове
  password)                       власника                                toast-повідомлення

  Linux X11      **1. xdotool**   xdotool type \--clearmodifiers \--delay 
                                  0                                       

  Linux X11 ---  **2. xdotool     xdotool key ctrl+v після встановлення   Fallback для
  захищене       clipboard**      тексту в PRIMARY                        Electron та
                                                                          захищених полів

  Linux Wayland  **1.             wl-copy + Ctrl+V simulation via         Wayland sandbox
                 wl-clipboard**   wlr-virtual-keyboard                    обмеження
  -------------- ---------------- --------------------------------------- --------------------

  --------------------------------------------------------------------
  **✅ Виправлення: Windows clipboard notification у Win 11**

  Проблема: SetClipboardData trick для приховування clipboard
  notification ігнорується новими білдами Win 11.

  Рішення 1 (пріоритет): IUIAutomationTextPattern::GetSelection() для
  безпосереднього читання тексту з елемента --- без clipboard взагалі.

  Рішення 2 (fallback): OpenClipboard з обов\'язковим збереженням
  токена попереднього власника (GetClipboardOwner) та відновленням
  після операції. Ізольований блок try/catch.

  OpenClipboard залишається виключно останнім fallback --- для полів,
  де UIA недоступний.
  --------------------------------------------------------------------

> // Win 11 --- правильний порядок:
>
> // 1. Спробувати IUIAutomationTextPattern::GetSelection()
>
> // 2. Fallback: OpenClipboard з відновленням власника
>
> HWND prevOwner = GetClipboardOwner();
>
> if (OpenClipboard(hwnd)) {
>
> // \... операція \...
>
> CloseClipboard();
>
> // Відновити попереднього власника
>
> }

**5. Overlay UI**

Intent detection: overlay з\'являється лише після 300 мс нерухомості
миші після виділення. Панель не з\'являється на порожній рядок, лише
пробіли або лише цифри.

  --------------- --------------------------- -------------------------------
  **Платформа**   **Window API**              **Деталі**

  **Windows**     WS_EX_LAYERED \|            CreateWindowEx з WS_EX_TOPMOST.
                  WS_EX_TOOLWINDOW            Мін. розмір кнопки: 48×48 px.
                                              SetLayeredWindowAttributes для
                                              прозорості.

  **Linux X11**   override-redirect window    XCreateWindow з
                  (libX11)                    override_redirect=True.
                                              Positioned через XQueryPointer.
                                              Клас вікна:
                                              \_NET_WM_WINDOW_TYPE_TOOLTIP.

  **Linux         xdg-layer-shell             Layer: overlay. Anchor: none
  Wayland +       (layer-shell-unstable-v1)   (позиція відносно курсора).
  wlroots**                                   Потребує
                                              wlr-layer-shell-unstable-v1
                                              підтримки compositor.

  **Linux GNOME   Overlay недоступний         Переключитися на Hotkey mode.
  Wayland**                                   UX-повідомлення при першому
                                              запуску.
  --------------- --------------------------- -------------------------------

**6. Hotkey Mode (Ctrl+Shift+O)**

Hotkey mode є повноцінною альтернативою overlay для всіх платформ і
обов\'язковим режимом за замовчуванням на GNOME Wayland та в
Enterprise-розгортаннях.

  --------------- ------------------------------ --------------------------
  **Платформа**   **Реєстрація хоткея**          **Дія при натисканні**

  **Windows**     RegisterHotKey(HWND_MESSAGE,   Виклик Selection Monitor →
                  id, MOD_CONTROL\|MOD_SHIFT,    Language Engine → Text
                  \'O\')                         Injector

  **Linux X11**   XGrabKey(display, keycode,     Аналогічно Windows
                  ControlMask\|ShiftMask, root,  
                  True, GrabModeAsync,           
                  GrabModeAsync)                 

  **Linux         xdg-desktop-portal             Аналогічно Windows
  Wayland**       GlobalShortcuts або пряма      
                  реєстрація через compositor    
                  API                            
  --------------- ------------------------------ --------------------------

  --------------------------------------------------------------------
  **Enterprise default**

  Hotkey mode є єдиним режимом за замовчуванням у корпоративних
  розгортаннях (без WH_MOUSE_LL/WH_KEYBOARD_LL → нуль EDR false
  positive).

  Group Policy може примусово встановити Hotkey-only mode через
  ADMX-шаблони.
  --------------------------------------------------------------------

**7. Language Engine**

**7.1 EN↔UK (v1.0)**

Unicode code point mapping з повним збереженням регістру та пунктуації.
Типографічні лапки та апостроф обробляються окремо (U+2018, U+2019,
U+201C, U+201D, «, »).

  ---------------------- ----------------------------------------------
  **Клас символів**      **Правило конвертації**

  **Літери (a--z,        Пошук у mapping-таблиці. Регістр зберігається:
  A--Z)**                uppercase → uppercase вихідного символу.

  **Цифри (0--9)**       Передаються без змін.

  **Пунктуація ASCII**   Передається без змін (., ! ? , ; : тощо).

  **Типографічні лапки   Обробляються окремими записами у
  (U+2018, U+2019,       mapping-таблиці.
  U+201C, U+201D, «,     
  »)**                   

  **Символи поза         Передаються без змін (pass-through).
  таблицею**             
  ---------------------- ----------------------------------------------

**7.2 EN↔KO --- Хангиль (v1.5)**

Бібліотека: libhangul (C, ліцензія MIT). Розкладка: Dubeolsik (두벌식).
Syllable composer + composition buffer.

**7.2.1 IME State Machine --- уніфікована поведінка**

  ---------------------- ----------------------------------------------
  **Стан IME**           **Поведінка ORKA**

  **IME неактивний**     Конвертація виконується одразу

  **IME активний,        1\. Надіслати VK_ESCAPE → скасувати
  незавершений склад**   незавершений склад (не підтверджувати) 2.
                         Виконати конвертацію наявного завершеного
                         тексту 3. Показати toast: \"Незавершений склад
                         скасовано\"

  **IME активний,        Конвертація виконується одразу --- склад вже у
  завершений склад**     буфері
  ---------------------- ----------------------------------------------

  --------------------------------------------------------------------
  **Вибір VK_ESCAPE (а не VK_RETURN)**

  VK_RETURN підтверджує поточний склад І може надіслати форму або
  виконати небажану дію в деяких застосунках (наприклад, Slack, Teams,
  веб-форми).

  VK_ESCAPE безпечно скасовує IME composition без побічних ефектів.
  --------------------------------------------------------------------

**7.3 EN↔HE --- Іврит з Bidi (v2.0)**

Bidi-рушій: ICU (International Components for Unicode) з вбудованою
підтримкою BiDi (UAX #9). Примітка: \"ibidi\" --- неіснуюча назва.
Коректно: ICU BiDi або окремо fribidi (С-бібліотека).

**7.3.1 Nikud (огласовки) --- lossy конвертація**

Nikud (U+05B0--U+05C7) не мають відповідників у QWERTY. Конвертація
HE→EN є lossy:

  -------------- --------------------------- --------------------------
  **Напрямок**   **Поведінка з Nikud**       **UX**

  **EN → HE**    Nikud не генеруються (не    Без змін
                 вводяться з QWERTY)         

  **HE → EN (без Стандартна конвертація      Без змін
  Nikud)**       через reverse-mapping       

  **HE → EN (з   Nikud стрипуються.          Обов\'язкове попередження
  Nikud)**       Конвертація базових         перед конвертацією:
                 символів виконується.       \"Огласовки будуть
                                             втрачені\". Користувач
                                             підтверджує.
  -------------- --------------------------- --------------------------

**8. Пакування та дистрибуція**

**8.1 Windows --- MSIX (MS Store)**

  ---------------------- ------------------------------------------------
  **Вимога**             **Деталі**

  **Code Signing**       EV (Extended Validation) Code Signing
                         Certificate. SignTool з RFC 3161
                         timestamp-сервером.

  **AppxManifest.xml**   Задекларувати restrictedCapabilities для
                         clipboard access. Без цього --- відхилення WACK.

  **WACK**               Windows App Certification Kit --- 100% pass
                         обов\'язковий перед сабмітом у MS Store.

  **Desktop Bridge**     MSIX + Desktop Bridge (Centennial) для
                         використання Win32 API у Store-контексті.
  ---------------------- ------------------------------------------------

**8.2 Windows --- MSI (Enterprise)**

  -------------------- ------------------------------------------------
  **Параметр**         **Значення**

  **Silent install**   msiexec /i orka.msi /quiet
                       HOTKEY=\"Ctrl+Shift+O\"

  **Group Policy**     ADMX-шаблони для централізованого розгортання та
                       примусового Hotkey-only mode

  **MDM                Microsoft Intune ADMX profiles для Windows
  (корпоративний)**    розгортання
  -------------------- ------------------------------------------------

Примітка щодо HOTKEY у msiexec: значення параметра з пробілами
передається у лапках. Синтаксис:

> msiexec /i orka.msi /quiet HOTKEY=\"Ctrl+Shift+O\"

При виклику з CMD: HOTKEY=\\\"Ctrl+Shift+O\\\" (екранування лапок
обов\'язкове).

**8.3 Linux --- .deb / apt**

  -------------------- ------------------------------------------------
  **Вимога**           **Деталі**

  **GPG-підпис**       Підписати .deb пакет та InRelease файл
                       репозиторію

  **Встановлення**     sudo apt install orka

  **Залежності**       libx11, libxfixes (X11); libhangul (v1.5);
                       libicu (v2.0)

  **Права**            Стандартний користувач; без sudo після
                       встановлення
  -------------------- ------------------------------------------------

**9. Системні вимоги**

  -------------------- ------------------------ ------------------------
  **Параметр**         **Windows**              **Linux**

  **ОС**               Windows 10 21H2+ / 11    Ubuntu 20.04+ / Mint 20+
                                                / Debian 11+

  **Архітектура**      x64, ARM64               x64, ARM64

  **RAM**              512 MB (рек. 1 GB)       256 MB (рек. 512 MB)

  **Права (runtime)**  Стандартний користувач   Стандартний користувач
                       (без admin)              (без sudo)

  **Права              Admin для MSI; не        sudo для першого apt
  (встановлення)**     потрібен для MS Store    install

  **Встановлення**     MS Store / MSIX / MSI    sudo apt install orka /
                       (Enterprise)             .deb
  -------------------- ------------------------ ------------------------

**10. Обов\'язкові тест-кейси**

**10.1 EN↔UK (v1.0)**

  ----------------------- ----------------------- -----------------------
  **Вхід**                **Очікуваний вихід**    **Що перевіряється**

  HelloWorld              ХеллоВорлд (і назад)    CamelCase ---
                                                  збереження регістру

  HELLO_WORLD             ПРИВІТ_СВІТ             ALL_CAPS + underscore
                                                  passthrough

  Hello, World!           Хелло, Ворлд!           Змішаний регістр +
                                                  пунктуація

  \"Hello\"               «Хелло»                 Типографічні лапки

  (порожній рядок)        Панель НЕ з\'являється  Edge case: порожнє
                                                  виділення

  (тільки пробіли)        Панель НЕ з\'являється  Edge case:
                                                  whitespace-only

  (тільки цифри)          Панель НЕ з\'являється  Edge case: digits-only

  Виділення у             Clipboard-only + toast  Захищені поля
  password-полі                                   

  Рух миші до кнопки Copy Панель НЕ з\'являється  Intent detection delay
                          протягом 300 мс         
  ----------------------- ----------------------- -----------------------

**10.2 EN↔KO --- Хангиль (v1.5)**

  ----------------------- ----------------------- -----------------------
  **Вхід (QWERTY)**       **Очікуваний вихід**    **Що перевіряється**

  r                       ㄱ                      Однофонемний склад
                                                  (jamo без голосної)

  rk                      가                      Повний склад (leading +
                                                  vowel)

  rtk                     각                      Закритий склад
                                                  (leading + vowel +
                                                  trailing)

  rkr                     ㄱㄱ                    Без голосних --- два
                                                  jamo окремо

  Hello rk dl             Hello 가 이             Змішаний рядок EN+KO

  IME активний +          VK_ESCAPE →             IME State Machine
  незавершений склад      конвертація + toast     
  ----------------------- ----------------------- -----------------------

**10.3 EN↔HE --- Іврит (v2.0)**

  ----------------------- ----------------------- -----------------------
  **Вхід**                **Очікуваний вихід**    **Що перевіряється**

  akln                    שלום                    Простий RTL

  Hi akln                 Hi שלום                 Змішаний LTR+RTL (Bidi
                                                  коректний)

  RTL у MS Word /         Коректне відображення   Bidi у різних
  LibreOffice / textarea                          застосунках

  HE → EN (без Nikud)     Стандартна конвертація  Стандартний
                                                  reverse-mapping

  HE → EN (з Nikud)       Попередження →          Lossy конвертація
                          підтвердження → strip   
                          Nikud → конвертація     

  Поле без Bidi-підтримки Чисте відображення без  Не вставляти raw
                          U+202B/U+202C           Bidi-маркери
  ----------------------- ----------------------- -----------------------

**11. Реєстр технічних ризиків**

  ------------------------ -------------- ------------------------------------------ -------------
  **Ризик**                **Рівень**     **Мітигація**                              **Фаза**

  EDR/AV false positive на **Високий**    Enterprise-збірка (#ifndef                 v1.0
  WH_MOUSE_LL +                           ENTERPRISE_BUILD): тільки UIA Events +     
  WH_KEYBOARD_LL                          RegisterHotKey. Consumer: low-priority     
                                          thread, timeout ≤ 5 мс. Whitelist docs для 
                                          IT.                                        

  Win 11 clipboard         **Середній**   IUIAutomationTextPattern::GetSelection()   v1.0
  notification (\"Orka                    --- пріоритетний шлях без clipboard.       
  pasted from\...\")                      OpenClipboard з відновленням токена        
                                          власника --- тільки останній fallback.     

  GNOME Wayland: overlay   **Високий**    Hotkey mode як default. UX-повідомлення    v1.1
  недоступний                             при першому запуску. Явно задокументовано  
                                          обмеження.                                 

  IME Хангиль: VK_ESCAPE   **Середній**   Toast-повідомлення. Підтримка Ctrl+Z для   v1.5
  скасовує незавершений                   скасування конвертації.                    
  склад (втрата даних)                                                               

  HE Nikud lossy           **Низький**    UI попередження перед конвертацією з явним v2.0
  конвертація                             підтвердженням.                            

  MS Store відхилення      **Низький**    Задекларовано у AppxManifest.xml. WACK     v1.0
  через                                   проходить. Прецеденти є.                   
  restrictedCapabilities                                                             
  clipboard                                                                          
  ------------------------ -------------- ------------------------------------------ -------------

**12. Критерії приймання за версіями**

  ------------ ----------------------------------- -----------------------
  **Версія**   **Критерій**                        **Порогове значення**

  **v1.0**     Selection latency (Win/Linux)       **\< 100 мс**

  **v1.0**     Text Injector: втрати тексту        **0 з 100
                                                   тест-сценаріїв**

  **v1.0**     EDR false positive (Consumer        **0 у тест-середовищі**
               збірка)                             

  **v1.0**     EDR false positive (Enterprise      **0 --- хуки фізично
               збірка)                             відсутні**

  **v1.0**     Overlay при не-виділенні            **Не з\'являється**

  **v1.0**     WACK (MS Store)                     **100% pass**

  **v1.0**     Встановлення Linux                  **1 командою**

  **v1.0**     Clipboard notification Win 11       **0 тригерів при
                                                   стандартному сценарії**

  **All**      Тест-кейси §10                      **100% green**
  ------------ ----------------------------------- -----------------------

ORKA Technical Specification v2.5 · Perepelytsia Orka Technologies ·
Конфіденційно
