# Работа с проектом F411CE (STM32F411CEUx) в Cursor / VS Code на macOS

Подробное руководство по редактированию, сборке и отладке STM32F4-проекта в Cursor (или VS Code) на macOS.

---

## Содержание

1. [Требования](#требования)
2. [Настройка окружения](#настройка-окружения)
3. [Структура проекта](#структура-проекта)
4. [Сборка проекта](#сборка-проекта)
5. [Прошивка контроллера](#прошивка-контроллера)
6. [Отладка](#отладка)
7. [Полезные команды](#полезные-команды)
8. [Горячие клавиши](#горячие-клавиши)
9. [Troubleshooting](#troubleshooting)

---

## Требования

### Обязательное ПО (macOS)

1. **Cursor** — [скачать](https://cursor.sh/) (или VS Code — [скачать](https://code.visualstudio.com/))
2. **ARM GCC Toolchain** — через Homebrew: `brew install arm-none-eabi-gcc`
3. **Make** — встроен в macOS (Xcode Command Line Tools)
4. **OpenOCD** — через Homebrew: `brew install open-ocd`

> На macOS драйверы ST-Link не нужны — OpenOCD работает через libusb напрямую.

### Рекомендуемые расширения

```bash
# C/C++ IntelliSense
cursor --install-extension ms-vscode.cpptools

# Cortex-Debug (для отладки STM32)
cursor --install-extension marus25.cortex-debug
```

> Для VS Code замените `cursor` на `code` в командах выше.

---

## Настройка окружения

### Установка через Homebrew

```bash
# Toolchain
brew install arm-none-eabi-gcc

# OpenOCD для прошивки и отладки
brew install open-ocd

# Xcode Command Line Tools (если make не установлен)
xcode-select --install
```

### Проверка установки

Откройте терминал и выполните:

```bash
arm-none-eabi-gcc --version
make --version
openocd --version
```

Все команды должны вернуть версии программ.

### Совместимость toolchain

Автогенерированные Makefile из STM32CubeIDE используют флаг `-fcyclomatic-complexity`, который поддерживается только в специальной сборке GCC от ST (13.3.rel1). Стандартный `arm-none-eabi-gcc` из Homebrew этот флаг не поддерживает.

**Исправление:** в каждом файле `Debug/*/subdir.mk` удалите `-fcyclomatic-complexity` из строки компиляции. Это не влияет на генерируемый код — флаг только создаёт `.cyclo` файлы с метриками.

---

## Структура проекта

```
F411CE/
├── .vscode/                    # Конфигурация Cursor / VS Code
│   ├── c_cpp_properties.json   # IntelliSense (пути к заголовкам)
│   ├── tasks.json              # Задачи сборки/прошивки
│   ├── launch.json             # Конфигурация отладчика
│   └── settings.json           # Настройки проекта
├── Core/
│   ├── Inc/                    # Заголовочные файлы (DataFile.h, FreeRTOSConfig.h)
│   └── Src/                    # Исходники (main.c, uart.c, fram.c, crc16.c)
├── Drivers/                    # HAL драйверы STM32F4xx
├── Middlewares/                # FreeRTOS v10.3.1
├── Debug/                      # Выходные файлы сборки
│   ├── makefile                # Автогенерируемый Makefile
│   └── F411CE.elf              # Прошивка после сборки
├── F411CE.ioc                  # STM32CubeMX проект
├── STM32F411CEUX_FLASH.ld      # Linker script (Flash)
├── STM32F411CEUX_RAM.ld        # Linker script (RAM)
├── Run.cfg                     # OpenOCD конфигурация
├── CLAUDE.md                   # Инструкции для AI-ассистента
└── VSCODE_GUIDE.md             # Это руководство
```

---

## Сборка проекта

### Первая сборка

**ВАЖНО**: Перед первой сборкой в Cursor необходимо один раз собрать проект в STM32CubeIDE. Это генерирует папку `Debug/` с полным Makefile.

Если `Debug/makefile` уже существует (как в этом репозитории) — STM32CubeIDE не нужен, можно сразу собирать.

### Через терминал

```bash
# Полная сборка
make -C Debug all

# Параллельная сборка (быстрее)
make -C Debug all -j8

# Очистка
make -C Debug clean

# Пересборка
make -C Debug clean && make -C Debug all -j8
```

### Через Cursor Tasks

Файл `.vscode/tasks.json` уже настроен. Два способа запуска:

**Способ 1:** **Cmd+Shift+B** → выбрать задачу из списка

**Способ 2 (если Cmd+Shift+B не работает в Cursor):** **Cmd+Shift+P** → ввести `Tasks: Run Task` → Enter → выбрать задачу

Доступные задачи:
  - `Build` — сборка проекта (по умолчанию)
  - `Clean` — очистка
  - `Clean & Build` — пересборка
  - `Flash` — прошивка через OpenOCD
  - `Build & Flash` — сборка + прошивка
  - `Firmware Size` — показать размер прошивки (text/data/bss)

---

## Прошивка контроллера

### Метод 1: OpenOCD (рекомендуется)

```bash
openocd -f Run.cfg -c "program Debug/F411CE.elf verify reset exit"
```

### Метод 2: Cursor Task

Запустите через палитру команд: **Cmd+Shift+P** → `Tasks: Run Task` → `Flash`.

Или используйте `Build & Flash` для сборки и прошивки за один шаг.

---

## Отладка

### Cortex-Debug (рекомендуется)

1. Установите расширение **Cortex-Debug**:
   ```bash
   cursor --install-extension marus25.cortex-debug
   ```

2. `launch.json` уже настроен в проекте

3. Подключите ST-Link к плате BlackPill

4. Нажмите **F5** или откройте вкладку **Run and Debug** (Cmd+Shift+D) и выберите конфигурацию **Debug (OpenOCD)**

### Возможности отладчика

- **Breakpoints** — точки останова (клик на номере строки)
- **Step Over (F10)** — выполнить строку
- **Step Into (F11)** — войти в функцию
- **Step Out (Shift+F11)** — выйти из функции
- **Continue (F5)** — продолжить выполнение
- **Variables** — просмотр переменных
- **Watch** — отслеживание выражений
- **Call Stack** — стек вызовов

> SVD файл для просмотра периферийных регистров не включён в проект. Его можно скачать отдельно — ищите `STM32F411.svd` в [cmsis-svd репозитории](https://github.com/posborne/cmsis-svd/tree/master/data/STMicro). После скачивания добавьте в `launch.json`: `"svdFile": "${workspaceFolder}/STM32F411.svd"`.

---

## Конфигурационные файлы

### `.vscode/tasks.json`

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build",
      "type": "shell",
      "command": "make",
      "args": ["-C", "Debug", "all", "-j8"],
      "group": {
        "kind": "build",
        "isDefault": true
      },
      "problemMatcher": ["$gcc"],
      "presentation": {
        "reveal": "always",
        "panel": "shared"
      }
    },
    {
      "label": "Clean",
      "type": "shell",
      "command": "make",
      "args": ["-C", "Debug", "clean"],
      "problemMatcher": []
    },
    {
      "label": "Clean & Build",
      "dependsOrder": "sequence",
      "dependsOn": ["Clean", "Build"],
      "problemMatcher": [],
      "group": "build"
    },
    {
      "label": "Flash",
      "type": "shell",
      "command": "openocd",
      "args": [
        "-f",
        "Run.cfg",
        "-c",
        "program Debug/F411CE.elf verify reset exit"
      ],
      "problemMatcher": [],
      "presentation": {
        "reveal": "always"
      }
    },
    {
      "label": "Build & Flash",
      "dependsOrder": "sequence",
      "dependsOn": ["Build", "Flash"],
      "problemMatcher": []
    },
    {
      "label": "Firmware Size",
      "type": "shell",
      "command": "arm-none-eabi-size",
      "args": ["Debug/F411CE.elf"],
      "problemMatcher": [],
      "presentation": {
        "reveal": "always"
      }
    }
  ]
}
```

### `.vscode/launch.json`

Конфигурация для отладки через Cortex-Debug:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug (OpenOCD)",
      "cwd": "${workspaceFolder}",
      "executable": "${workspaceFolder}/Debug/F411CE.elf",
      "request": "launch",
      "type": "cortex-debug",
      "runToEntryPoint": "main",
      "servertype": "openocd",
      "configFiles": ["Run.cfg"],
      "searchDir": ["${workspaceFolder}"],
      "preLaunchTask": "Build",
      "showDevDebugOutput": "raw"
    },
    {
      "name": "Attach (OpenOCD)",
      "cwd": "${workspaceFolder}",
      "executable": "${workspaceFolder}/Debug/F411CE.elf",
      "request": "attach",
      "type": "cortex-debug",
      "servertype": "openocd",
      "configFiles": ["Run.cfg"],
      "searchDir": ["${workspaceFolder}"]
    }
  ]
}
```

---

## Полезные команды

### Терминальные команды

```bash
# Сборка с подробным выводом
make -C Debug all VERBOSE=1

# Параллельная сборка (быстрее)
make -C Debug all -j8

# Получить размер прошивки
arm-none-eabi-size Debug/F411CE.elf

# Дизассемблирование
arm-none-eabi-objdump -d Debug/F411CE.elf > disassembly.txt

# Просмотр карты памяти
less Debug/F411CE.map

# Генерация HEX из ELF
arm-none-eabi-objcopy -O ihex Debug/F411CE.elf Debug/F411CE.hex

# Генерация BIN из ELF
arm-none-eabi-objcopy -O binary Debug/F411CE.elf Debug/F411CE.bin
```

### OpenOCD команды

```bash
# Запуск GDB сервера (отладка без Cursor)
openocd -f Run.cfg

# Сброс платы
openocd -f Run.cfg -c "init; reset; exit"

# Чтение Flash памяти (512 KB)
openocd -f Run.cfg -c "init; flash read_bank 0 flash_dump.bin 0 0x80000; exit"
```

---

## Горячие клавиши

> На macOS Cursor/VS Code используют **Cmd** вместо **Ctrl**.

### Редактирование

| Клавиша | Действие |
|---------|----------|
| `Ctrl+Space` | Автодополнение |
| `F12` | Перейти к определению |
| `Shift+F12` | Найти все ссылки |
| `Cmd+.` | Быстрое исправление |
| `F2` | Переименовать символ |
| `Cmd+K Cmd+0` | Свернуть все |
| `Cmd+K Cmd+J` | Развернуть все |

### Сборка и отладка

| Клавиша | Действие |
|---------|----------|
| `Cmd+Shift+B` | Запустить задачу сборки |
| `F5` | Начать отладку |
| `Cmd+F5` | Запустить без отладки |
| `Shift+F5` | Остановить отладку |
| `F9` | Установить/снять breakpoint |
| `F10` | Step Over (следующая строка) |
| `F11` | Step Into (войти в функцию) |
| `Shift+F11` | Step Out (выйти из функции) |

### Навигация

| Клавиша | Действие |
|---------|----------|
| `Cmd+P` | Быстрый переход к файлу |
| `Cmd+Shift+F` | Поиск в проекте |
| `Ctrl+G` | Перейти к строке |
| `Ctrl+-` / `Ctrl+Shift+-` | Назад/Вперёд по истории |
| `Ctrl+Tab` | Переключение между файлами |

---

## Troubleshooting

### Проблема: `arm-none-eabi-gcc not found`

**Решение:**
```bash
brew install arm-none-eabi-gcc
```
Перезапустите Cursor после установки.

### Проблема: `make: command not found`

**Решение:**
```bash
xcode-select --install
```

### Проблема: `openocd` не установлен

**Решение:**
```bash
brew install open-ocd
```

### Проблема: `openocd` не находит ST-Link

**Решение:**
1. Проверьте подключение USB-кабеля к BlackPill
2. Проверьте: `openocd -f Run.cfg`
3. Убедитесь, что ST-Link не занят другой программой (STM32CubeIDE)
4. На macOS может потребоваться разрешение на USB-доступ в System Settings → Privacy & Security

### Проблема: `timed out while waiting for target halted`

**Причина:** `Run.cfg` требует подключённый пин NRST (аппаратный reset), а провод NRST не подключён к ST-Link.

**Решение:** в `Run.cfg` замените строку reset_config на программный сброс:
```
# Было (требует провод NRST):
reset_config srst_only srst_nogate connect_assert_srst

# Стало (достаточно SWDIO + SWCLK + GND):
reset_config none
set CONNECT_UNDER_RESET 0
```

### Проблема: ошибка `-fcyclomatic-complexity`

**Решение:** Удалите флаг `-fcyclomatic-complexity` из всех файлов `Debug/*/subdir.mk`. Этот флаг специфичен для GCC от STM32CubeIDE и не поддерживается стандартным `arm-none-eabi-gcc`.

```bash
# Найти все файлы с этим флагом
grep -rl "fcyclomatic-complexity" Debug/
```

### Проблема: IntelliSense не работает

**Решение:**
1. Откройте Command Palette (Cmd+Shift+P)
2. Выполните: `C/C++: Reset IntelliSense Database`
3. Перезапустите Cursor
4. Проверьте `.vscode/c_cpp_properties.json` — должны быть указаны правильные пути к заголовочным файлам

### Проблема: отладчик не останавливается на breakpoint

**Решение:**
1. Убедитесь, что проект собран с флагами `-g3 -O0` (Debug конфигурация)
2. Проверьте, что прошивка актуальная (пересоберите и прошейте заново)
3. В `launch.json` должно быть: `"runToEntryPoint": "main"`

### Проблема: сборка проходит, но прошивка не запускается

**Решение:**
1. Проверьте linker script (`STM32F411CEUX_FLASH.ld`)
2. Убедитесь, что OpenOCD корректно прошил: должна быть строка `verified XXX bytes`
3. Сделайте полный сброс: `openocd -f Run.cfg -c "init; reset halt; reset; exit"`

### Проблема: недостаточно памяти (RAM переполнена)

**Решение:**
STM32F411CEUx имеет 128 KB RAM и 512 KB Flash.
1. Проверьте использование памяти: `arm-none-eabi-size Debug/F411CE.elf`
2. Смотрите карту памяти: `grep -E '\.bss|\.data|\.rodata' Debug/F411CE.map`
3. Оптимизируйте:
   - Уменьшите размеры буферов в `DataFile.h`
   - Используйте `const` для строк (в FLASH вместо RAM)
   - Уменьшите размеры стеков FreeRTOS задач в `FreeRTOSConfig.h`
   - Примените флаг `-Os` для оптимизации по размеру

---

## Дополнительные ресурсы

- [STM32CubeIDE Documentation](https://www.st.com/en/development-tools/stm32cubeide.html)
- [ARM GCC Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)
- [OpenOCD Documentation](https://openocd.org/doc/)
- [Cortex-Debug Extension](https://github.com/Marus/cortex-debug)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)

---

## Workflow: CubeMX + Cursor

1. **Настройка периферии**: откройте `F411CE.ioc` в STM32CubeMX → измените конфигурацию → Generate Code
2. **Редактирование**: работайте с кодом в Cursor (добавляйте логику в `USER CODE` блоки)
3. **Сборка**: `Cmd+Shift+B` → Build
4. **Прошивка**: Task `Flash` или `Build & Flash`
5. **Отладка**: `F5` → Debug с breakpoints

---

*Последнее обновление: 2026-02-16*
