# OPENOCD_CURSOR.md

Короткий рабочий гайд по OpenOCD в Cursor для проекта `F411CE` (macOS и Windows).

---

## 1) Рабочие параметры проекта

- Конфиг: `Run.cfg`
- MCU target: `stm32f4x`
- Интерфейс: `stlink-dap` + `dapdirect_swd`
- SWD clock: `4000 kHz`
- ELF: `Debug/F411CE.elf`

---

## 2) Почему раньше падало в Cursor

Ошибка вида:

`Can't find interface/stlink-dap.cfg`

означает, что OpenOCD запущен без каталога scripts. В `launch.json` в **searchDir** должен быть путь к скриптам OpenOCD:

- **macOS (Homebrew):** `/opt/homebrew/share/openocd/scripts` (Apple Silicon) или `/usr/local/share/openocd/scripts` (Intel)
- **Windows:** `C:\ST\STM32CubeIDE_1.17.0\...\st_scripts` (см. раздел 4)

---

## 3) macOS: отладка в Cursor

### Конфигурация launch.json

В `.vscode/launch.json` для macOS используются конфигурации **Debug (OpenOCD)** и **Attach (OpenOCD)** (первые в списке). В них указано:

- `searchDir`: `${workspaceFolder}` и `/opt/homebrew/share/openocd/scripts`
- `device`: `STM32F411CE`, `interface`: `swd`
- `rtos`: `FreeRTOS` — для отображения задач в отладчике
- `preLaunchTask`: `Build` — перед F5 собирается проект

**Как отлаживать:** подключите ST-Link к BlackPill → **F5** или Run and Debug → выберите **Debug (OpenOCD)**.

### Проверочные команды (терминал macOS)

```bash
# Проверка конфига (должен завершиться без ошибок)
openocd -f Run.cfg -c "shutdown"

# Прошивка
openocd -f Run.cfg -c "program Debug/F411CE.elf verify reset exit"
```

В норме: `Programming Finished`, `Verified OK`, `Resetting Target`.

### Требования на macOS

- **OpenOCD:** `brew install open-ocd`
- **Cortex-Debug:** установить расширение в Cursor (`marus25.cortex-debug`)
- **ARM GCC / Make:** через STM32CubeIDE или `brew install arm-none-eabi-gcc make` (см. VSCODE_GUIDE.md)

---

## 4) Windows: проверочные команды (PowerShell)

## Проверка конфига

```powershell
& "C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.100.202501161620\tools\bin\openocd.exe" `
  -s "C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.3.200.202510310951\resources\openocd\st_scripts" `
  -f "Run.cfg" `
  -c "shutdown"
```

## Прошивка

```powershell
& "C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.100.202501161620\tools\bin\openocd.exe" `
  -s "C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.3.200.202510310951\resources\openocd\st_scripts" `
  -f "Run.cfg" `
  -c "program Debug/F411CE.elf verify reset exit"
```

В норме:
- `Programming Finished`
- `Verified OK`
- `Resetting Target`

---

## 5) Что уже настроено в `.vscode`

- **launch.json**
  - **macOS:** конфигурации «Debug (OpenOCD)» и «Attach (OpenOCD)» с `searchDir` → `/opt/homebrew/share/openocd/scripts`.
  - **Windows:** конфигурации «Debug (OpenOCD) Windows» и «Attach (OpenOCD) Windows» с путём к `st_scripts` STM32CubeIDE.
- **tasks.json** (macOS): сборка и Flash через `openocd -f Run.cfg` (openocd из PATH).
- **tasks_windows.json** (Windows): `Flash` с `-s ...\st_scripts`, пути к GCC/Make/OpenOCD из STM32CubeIDE 1.17.0.

---

## 6) Быстрый troubleshooting

### ST-Link не найден

- **macOS:** проверить USB-кабель и питание; закрыть другие сессии OpenOCD/STM32CubeIDE.
- **Windows:** установить драйвер ST-Link (`STSW-LINK009`); проверить в диспетчере устройств.

### OpenOCD не запускается

- **macOS:** установить `brew install open-ocd`; в launch.json должен быть `searchDir` с `/opt/homebrew/share/openocd/scripts`. На Intel Mac заменить на `/usr/local/share/openocd/scripts` при необходимости.
- **Windows:** использовать полный путь к `openocd.exe` (см. команды в разделе 4) или задачу `Flash` в Cursor.

### Cursor видит старые пути

- Перезапустить окно: `Developer: Reload Window`.
- Убедиться, что открыта папка проекта `F411CE`.

