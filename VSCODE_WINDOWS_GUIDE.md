# Работа с проектом F411CE (STM32F411CEUx) в VS Code на Windows

Подробное руководство по редактированию, сборке и отладке STM32F4-проекта в Visual Studio Code на Windows.

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

### Обязательное ПО

1. **Visual Studio Code** — [скачать](https://code.visualstudio.com/)
2. **STM32CubeIDE** — [скачать](https://www.st.com/en/development-tools/stm32cubeide.html) (содержит ARM GCC Toolchain, Make, OpenOCD)
3. **ST-Link drivers** — [STSW-LINK009](https://www.st.com/en/development-tools/stsw-link009.html)

> STM32CubeIDE включает в себя всё необходимое: `arm-none-eabi-gcc`, `make`, `openocd`. Отдельная установка не требуется.

### Рекомендуемые расширения VS Code

```bash
# C/C++ IntelliSense
code --install-extension ms-vscode.cpptools

# Cortex-Debug (для отладки STM32)
code --install-extension marus25.cortex-debug
```

Или через GUI: Extensions (Ctrl+Shift+X) — искать расширение — Install.

---

## Настройка окружения

### Вариант 1: Автоматический PATH через tasks.json (рекомендуется)

Файл `.vscode/tasks_windows.json` содержит пути к toolchain из STM32CubeIDE. Чтобы использовать:

1. Переименуйте `tasks_windows.json` в `tasks.json` (заменив macOS-версию)
2. Сборка будет работать сразу через **Ctrl+Shift+B** без ручной настройки PATH

### Вариант 2: Глобальный PATH (опционально)

Если хотите использовать toolchain в любом терминале:

1. Откройте **Параметры системы** — **Переменные среды**
2. Найдите переменную `Path` и добавьте пути:

```
C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.1.0.202410251130\tools\bin
C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin
C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.100.202409170845\tools\bin
```

> Пути зависят от версии STM32CubeIDE. Проверьте актуальные пути в `C:\ST\`.

### Проверка установки

Откройте терминал (CMD или PowerShell) и выполните:

```bash
arm-none-eabi-gcc --version
make --version
openocd --version
```

Все команды должны вернуть версии программ.

> Если команды не найдены, но `tasks.json` с Windows-путями настроен — сборка через VS Code (Ctrl+Shift+B) всё равно будет работать.

---

## Структура проекта

```
F411CE/
├── .vscode/                    # Конфигурация VS Code
│   ├── c_cpp_properties.json   # IntelliSense (пути к заголовкам)
│   ├── tasks.json              # Задачи сборки/прошивки (macOS)
│   ├── tasks_windows.json      # Задачи сборки/прошивки (Windows)
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
└── VSCODE_WINDOWS_GUIDE.md     # Это руководство
```

---

## Сборка проекта

### Первая сборка

**ВАЖНО**: Перед первой сборкой в VS Code необходимо один раз собрать проект в STM32CubeIDE. Это генерирует папку `Debug/` с полным Makefile.

Если `Debug/makefile` уже существует (как в этом репозитории) — можно сразу собирать в VS Code.

#### Шаги для первой сборки в STM32CubeIDE:

1. **Откройте проект**: File — Open Projects from File System — выберите папку проекта
2. **Выполните сборку**: Project — Build All (Ctrl+B)
3. **Проверьте результат**: в папке `Debug/` должны появиться `makefile`, `F411CE.elf`, `.hex`, `.bin`
4. **Теперь можно работать в VS Code**

### Через терминал VS Code

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

### Через VS Code Tasks (Ctrl+Shift+B)

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

### Метод 2: ST-Link Utility

Используйте ST-Link Utility GUI для прошивки `Debug/F411CE.hex`.

### Метод 3: VS Code Task

Ctrl+Shift+P — `Tasks: Run Task` — `Flash`.

Или используйте `Build & Flash` для сборки и прошивки за один шаг.

---

## Отладка

### Cortex-Debug (рекомендуется)

1. Установите расширение **Cortex-Debug**:
   ```bash
   code --install-extension marus25.cortex-debug
   ```

2. `launch.json` уже настроен в проекте

3. Подключите ST-Link к плате BlackPill

4. Нажмите **F5** или откройте вкладку **Run and Debug** (Ctrl+Shift+D) и выберите конфигурацию **Debug (OpenOCD)**

### Возможности отладчика

- **Breakpoints** — точки останова (клик на номере строки)
- **Step Over (F10)** — выполнить строку
- **Step Into (F11)** — войти в функцию
- **Step Out (Shift+F11)** — выйти из функции
- **Continue (F5)** — продолжить выполнение
- **Variables** — просмотр переменных
- **Watch** — отслеживание выражений
- **Call Stack** — стек вызовов

> SVD файл для просмотра периферийных регистров не включён в проект. Скачайте `STM32F411.svd` из [cmsis-svd репозитория](https://github.com/posborne/cmsis-svd/tree/master/data/STMicro) и добавьте в `launch.json`: `"svdFile": "${workspaceFolder}/STM32F411.svd"`.

> Подробное руководство по отладке: см. [DEBUG_GUIDE.md](DEBUG_GUIDE.md) (в нём горячие клавиши для macOS; на Windows замените Cmd на Ctrl).

---

## Конфигурационные файлы

### `.vscode/tasks_windows.json`

Версия tasks.json с Windows-путями к STM32CubeIDE toolchain:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build",
      "type": "shell",
      "command": "make",
      "args": ["-C", "Debug", "all", "-j8"],
      "options": {
        "env": {
          "PATH": "C:\\ST\\STM32CubeIDE_1.17.0\\STM32CubeIDE\\plugins\\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.1.0.202410251130\\tools\\bin;C:\\ST\\STM32CubeIDE_1.17.0\\STM32CubeIDE\\plugins\\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\\tools\\bin;${env:PATH}"
        }
      },
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
      "options": {
        "env": {
          "PATH": "C:\\ST\\STM32CubeIDE_1.17.0\\STM32CubeIDE\\plugins\\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.1.0.202410251130\\tools\\bin;C:\\ST\\STM32CubeIDE_1.17.0\\STM32CubeIDE\\plugins\\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\\tools\\bin;${env:PATH}"
        }
      },
      "problemMatcher": [],
      "presentation": {
        "reveal": "always",
        "panel": "shared"
      }
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
      "options": {
        "env": {
          "PATH": "C:\\ST\\STM32CubeIDE_1.17.0\\STM32CubeIDE\\plugins\\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.100.202409170845\\tools\\bin;${env:PATH}"
        }
      },
      "problemMatcher": [],
      "presentation": {
        "reveal": "always",
        "panel": "shared"
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
      "options": {
        "env": {
          "PATH": "C:\\ST\\STM32CubeIDE_1.17.0\\STM32CubeIDE\\plugins\\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.1.0.202410251130\\tools\\bin;${env:PATH}"
        }
      },
      "problemMatcher": [],
      "presentation": {
        "reveal": "always",
        "panel": "shared"
      }
    }
  ]
}
```

### `.vscode/c_cpp_properties.json` (Windows-версия)

Для IntelliSense на Windows замените `compilerPath` и добавьте include-пути toolchain:

```json
{
    "configurations": [
        {
            "name": "STM32",
            "includePath": [
                "${workspaceFolder}/**",
                "${workspaceFolder}/Core/Inc",
                "${workspaceFolder}/Drivers/STM32F4xx_HAL_Driver/Inc",
                "${workspaceFolder}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy",
                "${workspaceFolder}/Drivers/CMSIS/Device/ST/STM32F4xx/Include",
                "${workspaceFolder}/Drivers/CMSIS/Include",
                "${workspaceFolder}/Middlewares/Third_Party/FreeRTOS/Source/include",
                "${workspaceFolder}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2",
                "${workspaceFolder}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F",
                "C:/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.1.0.202410251130/tools/arm-none-eabi/include",
                "C:/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.1.0.202410251130/tools/arm-none-eabi/include/sys"
            ],
            "defines": [
                "USE_HAL_DRIVER",
                "STM32F411xE"
            ],
            "compilerPath": "C:/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.1.0.202410251130/tools/bin/arm-none-eabi-gcc.exe",
            "cStandard": "c99",
            "cppStandard": "c++11",
            "intelliSenseMode": "gcc-arm"
        }
    ],
    "version": 4
}
```

### `.vscode/launch.json`

Конфигурация отладки — одинаковая для Windows и macOS:

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
      "showDevDebugOutput": "raw",
      "device": "STM32F411CE",
      "interface": "swd"
    },
    {
      "name": "Attach (OpenOCD)",
      "cwd": "${workspaceFolder}",
      "executable": "${workspaceFolder}/Debug/F411CE.elf",
      "request": "attach",
      "type": "cortex-debug",
      "servertype": "openocd",
      "configFiles": ["Run.cfg"],
      "searchDir": ["${workspaceFolder}"],
      "showDevDebugOutput": "raw",
      "device": "STM32F411CE",
      "interface": "swd"
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

# Генерация HEX из ELF
arm-none-eabi-objcopy -O ihex Debug/F411CE.elf Debug/F411CE.hex

# Генерация BIN из ELF
arm-none-eabi-objcopy -O binary Debug/F411CE.elf Debug/F411CE.bin
```

### OpenOCD команды

```bash
# Запуск GDB сервера (отладка без VS Code)
openocd -f Run.cfg

# Сброс платы
openocd -f Run.cfg -c "init; reset; exit"

# Чтение Flash памяти (512 KB)
openocd -f Run.cfg -c "init; flash read_bank 0 flash_dump.bin 0 0x80000; exit"
```

---

## Горячие клавиши

### Редактирование

| Клавиша | Действие |
|---------|----------|
| `Ctrl+Space` | Автодополнение |
| `F12` | Перейти к определению |
| `Shift+F12` | Найти все ссылки |
| `Ctrl+.` | Быстрое исправление |
| `F2` | Переименовать символ |
| `Ctrl+K Ctrl+0` | Свернуть все |
| `Ctrl+K Ctrl+J` | Развернуть все |

### Сборка и отладка

| Клавиша | Действие |
|---------|----------|
| `Ctrl+Shift+B` | Запустить задачу сборки |
| `F5` | Начать отладку |
| `Ctrl+F5` | Запустить без отладки |
| `Shift+F5` | Остановить отладку |
| `Ctrl+Shift+F5` | Перезапустить отладку |
| `F9` | Установить/снять breakpoint |
| `F10` | Step Over (следующая строка) |
| `F11` | Step Into (войти в функцию) |
| `Shift+F11` | Step Out (выйти из функции) |

### Навигация

| Клавиша | Действие |
|---------|----------|
| `Ctrl+P` | Быстрый переход к файлу |
| `Ctrl+Shift+F` | Поиск в проекте |
| `Ctrl+G` | Перейти к строке |
| `Alt+Left/Right` | Назад/Вперёд по истории |
| `Ctrl+Tab` | Переключение между файлами |

---

## Troubleshooting

### Проблема: `arm-none-eabi-gcc not found`

**Решение:**
1. Убедитесь, что STM32CubeIDE установлен
2. Добавьте путь к toolchain в PATH (см. раздел "Настройка окружения")
3. Или используйте `tasks_windows.json` который включает пути автоматически
4. Перезапустите VS Code после изменения PATH

### Проблема: `make: command not found`

**Решение:**
- Установите Make через STM32CubeIDE (уже включён) и добавьте в PATH
- Или установите через [MinGW](https://www.mingw-w64.org/)
- Или используйте `tasks_windows.json`

### Проблема: `openocd` не находит ST-Link

**Решение:**
1. Установите [ST-Link drivers](https://www.st.com/en/development-tools/stsw-link009.html)
2. Проверьте подключение: `openocd -f Run.cfg`
3. Убедитесь, что ST-Link не занят другой программой (STM32CubeIDE, ST-Link Utility)
4. Проверьте в Device Manager — ST-Link должен отображаться

### Проблема: `timed out while waiting for target halted`

**Причина:** `Run.cfg` использует программный сброс (`reset_config none`). Если это не работает — попробуйте аппаратный.

**Решение:** в `Run.cfg` замените на аппаратный сброс (если подключён NRST):
```
# Программный сброс (текущий):
reset_config none
set CONNECT_UNDER_RESET 0

# Аппаратный сброс (если подключён NRST):
reset_config srst_only srst_nogate connect_assert_srst
set CONNECT_UNDER_RESET 1
```

### Проблема: ошибка `-fcyclomatic-complexity`

**Решение:** Удалите флаг `-fcyclomatic-complexity` из всех файлов `Debug\*\subdir.mk`. Этот флаг нужен только GCC из STM32CubeIDE.

> Если вы используете toolchain из STM32CubeIDE (через PATH или tasks.json), эта проблема не возникнет.

### Проблема: IntelliSense не работает

**Решение:**
1. Откройте Command Palette (Ctrl+Shift+P)
2. Выполните: `C/C++: Reset IntelliSense Database`
3. Перезапустите VS Code
4. Убедитесь, что `c_cpp_properties.json` содержит правильные Windows-пути к toolchain (см. раздел "Конфигурационные файлы")

### Проблема: отладчик не останавливается на breakpoint

**Решение:**
1. Убедитесь, что проект собран с флагами `-g3 -O0` (Debug конфигурация)
2. Проверьте, что прошивка актуальная (пересоберите и прошейте)
3. В `launch.json` должно быть: `"runToEntryPoint": "main"`

### Проблема: сборка проходит, но прошивка не запускается

**Решение:**
1. Проверьте linker script (`STM32F411CEUX_FLASH.ld`)
2. Убедитесь, что OpenOCD корректно прошил: должна быть строка `verified XXX bytes`
3. Сделайте полный сброс: `openocd -f Run.cfg -c "init; reset halt; reset; exit"`

### Проблема: недостаточно памяти (RAM переполнена)

**Решение:**
STM32F411CEUx имеет 128 KB RAM и 512 KB Flash — запас есть!
1. Проверьте использование памяти: `arm-none-eabi-size Debug/F411CE.elf`
2. Оптимизируйте при необходимости:
   - Уменьшите размеры буферов в `DataFile.h`
   - Используйте `const` для строк (в FLASH вместо RAM)
   - Уменьшите размеры стеков FreeRTOS задач
   - Примените флаг `-Os` для оптимизации по размеру

---

## Переключение между macOS и Windows

Проект поддерживает обе ОС. Отличается только `tasks.json`:

| Файл | ОС | Особенность |
|------|----|-------------|
| `tasks.json` | macOS | Homebrew toolchain в системном PATH |
| `tasks_windows.json` | Windows | Пути к STM32CubeIDE toolchain в env |

**Для Windows:** переименуйте `tasks_windows.json` — `tasks.json`

**Остальные файлы** (`launch.json`, `Run.cfg`) — работают на обеих ОС.

> Для Windows-версии `c_cpp_properties.json` см. раздел "Конфигурационные файлы" выше — она включает полные пути к Windows toolchain для IntelliSense.

---

## Дополнительные ресурсы

- [STM32CubeIDE Documentation](https://www.st.com/en/development-tools/stm32cubeide.html)
- [ARM GCC Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)
- [OpenOCD Documentation](https://openocd.org/doc/)
- [Cortex-Debug Extension](https://github.com/Marus/cortex-debug)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [VSCODE_GUIDE.md](VSCODE_GUIDE.md) — руководство для macOS
- [DEBUG_GUIDE.md](DEBUG_GUIDE.md) — полное руководство по отладке

---

## Workflow: CubeMX + VS Code

1. **Настройка периферии**: откройте `F411CE.ioc` в STM32CubeMX — измените конфигурацию — Generate Code
2. **Редактирование**: работайте с кодом в VS Code (добавляйте логику в `USER CODE` блоки)
3. **Сборка**: `Ctrl+Shift+B` — Build
4. **Прошивка**: Task `Flash` или `Build & Flash`
5. **Отладка**: `F5` — Debug с breakpoints

---

*Последнее обновление: 2026-02-16*
