# Быстрый старт: F411CE в Cursor на Windows

---

## Шаг 1 — Открыть проект

Откройте именно папку:

`C:\Project\ProjectSTM32\F411CE`

---

## Шаг 2 — Установить расширения

В Cursor (`Ctrl+Shift+X`) установите:

- `ms-vscode.cpptools`
- `marus25.cortex-debug`
- `mcu-debug.rtos-views`

---

## Шаг 3 — Сборка

- `Ctrl+Shift+B` -> `Build`
- или `Ctrl+Shift+P` -> `Tasks: Run Task` -> `Build`

Доступные задачи:
- `Build`
- `Clean`
- `Clean & Build`
- `Flash`
- `Build & Flash`
- `Firmware Size`

---

## Шаг 4 — Прошивка (ST-Link + OpenOCD)

`Ctrl+Shift+P` -> `Tasks: Run Task` -> `Flash`

В норме в логе:
- `Programming Finished`
- `Verified OK`
- `Resetting Target`

---

## Шаг 5 — Отладка

- Нажмите `F5`
- выберите `Debug (OpenOCD)`

Отладчик стартует через `Run.cfg`, ELF: `Debug/F411CE.elf`.

---

## Важно для Windows/OpenOCD в Cursor

В проекте уже настроено:

- `Run.cfg`:
  - `source [find interface/stlink-dap.cfg]`
  - `transport select "dapdirect_swd"`
  - `set CLOCK_FREQ 4000`

- `.vscode/tasks_windows.json`:
  - для `Flash` добавлен `-s ...\st_scripts`
  - обновлены пути к GCC 13.3 и OpenOCD 2.4.100 (STM32CubeIDE 1.17.0)

- `.vscode/launch.json`:
  - `searchDir` включает каталог `st_scripts`

---

## Типовые проблемы

### `Can't find interface/stlink*.cfg`

Причина: OpenOCD не видит scripts.

Решение:
- использовать задачу `Flash` из проекта (она уже с `-s ...\st_scripts`);
- не запускать голый `openocd -f Run.cfg` без `-s`.

### `Unable to match requested speed ...`

Сейчас в `Run.cfg` выставлено `CLOCK_FREQ 4000`, предупреждение обычно не появляется.

### ST-Link не найден

Проверьте:
1. драйвер ST-Link (`STSW-LINK009`);
2. кабель/питание платы;
3. STM32CubeIDE не должен держать ST-Link одновременно.

---

## Что не ломает macOS

macOS-поток (`tasks.json`) не менялся.
Все Windows-правки сделаны в `tasks_windows.json` и документации.

