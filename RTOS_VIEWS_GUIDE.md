# RTOS Views — мониторинг FreeRTOS в Cursor/VS Code (macOS)

Расширение **RTOS Views** (`mcu-debug.rtos-views`) показывает живое состояние FreeRTOS задач,
очередей и памяти прямо в IDE во время отладки — вместо чтения логов USART6.

---

## Установка

`Cmd+Shift+X` → в строке поиска ввести `RTOS Views` → найти издателя **mcu-debug** → **Install**

Или через палитру:

```text
Cmd+Shift+P → Extensions: Install Extensions → mcu-debug.rtos-views
```

> Требует уже установленного **Cortex-Debug** (`marus25.cortex-debug`).

---

## Настройка launch.json

Открыть [.vscode/launch.json](.vscode/launch.json) — ключевая строка `"rtos": "FreeRTOS"` уже добавлена:

```json
{
  "name": "Debug (OpenOCD)",
  "type": "cortex-debug",
  "request": "launch",
  "servertype": "openocd",
  "executable": "${workspaceFolder}/Debug/F411CE.elf",
  "configFiles": ["Run.cfg"],
  "searchDir": [
    "${workspaceFolder}",
    "/opt/homebrew/share/openocd/scripts"
  ],
  "armToolchainPath": "/opt/homebrew/bin",
  "gdbPath": "/opt/homebrew/bin/arm-none-eabi-gdb",
  "serverpath": "/opt/homebrew/bin/openocd",
  "device": "STM32F411CE",
  "svdFile": "${workspaceFolder}/STM32F411.svd",
  "interface": "swd",
  "runToEntryPoint": "main",
  "preLaunchTask": "Build",
  "rtos": "FreeRTOS"
}
```

**SVD-файл** `STM32F411.svd` в корне проекта обеспечивает просмотр периферийных регистров (CORTEX PERIPHERALS).

---

## Как открыть панель RTOS Views

1. Запустить отладку **`F5`**
2. Дождаться остановки на `main()`
3. Открыть панель одним из способов:

**Способ 1 (палитра команд):**
`Cmd+Shift+P` → набрать `RTOS` → выбрать "RTOS Views: Focus on RTOS View"

**Способ 2 (меню View):**
View → Open View... → набрать `RTOS` → выбрать нужную панель (Tasks, Queues, Heap)

**Способ 3 (боковая панель Debug):**
`Cmd+Shift+D` → прокрутить вниз → найти секции RTOS: Tasks, RTOS: Queues, RTOS: Heap

> Панели RTOS Views видны **только во время активной отладки** (после F5), данные обновляются
> только когда процессор остановлен (на breakpoint или после Pause).
> Если на `main()` панели пустые — FreeRTOS ещё не инициализирован. Поставьте breakpoint
> после `osKernelStart()` (например в `StartDefaultTask`), и там RTOS Views покажет все задачи.

---

## Что показывает панель

### Tasks (Задачи)

Все три задачи проекта отображаются с полной информацией:

| Колонка | Описание |
| ------- | -------- |
| **Name** | Имя задачи (`defaultTask`, `LedTask`, `Uart1Task`) |
| **State** | Состояние: `Running` / `Blocked` / `Ready` / `Suspended` |
| **Priority** | Приоритет (Normal=1, AboveNormal=2) |
| **Stack Used** | Использовано стека в байтах |
| **Stack Size** | Размер стека задачи |
| **Stack Free** | Свободно байт (важно следить!) |

**Пороги безопасности для этого проекта (128KB RAM, 10KB heap):**

| Задача | Стек | Тревога если свободно менее |
| -------- | ------ | --------------------------- |
| defaultTask | 2048B | < 512B |
| LedTask | 512B | < 128B |
| Uart1Task | 2048B | < 512B |

### Queues (Очереди)

Очередь UART1 команд (40 элементов, тип `uint8_t`):

| Колонка | Описание |
| ------- | -------- |
| **Name** | Имя очереди (если задано) |
| **Length** | Максимальный размер (40) |
| **Used** | Сколько элементов сейчас в очереди |
| **Available** | Сколько свободных мест |

Если `Used` постоянно близко к 40 — очередь переполняется, команды теряются.

### Heap (Куча FreeRTOS)

| Колонка | Описание |
| ------- | -------- |
| **Total** | Всего heap: 10240 байт (10KB) |
| **Free** | Свободно сейчас |
| **Min Free** | Минимум за всё время работы (watermark) |

**Норма для этого проекта:**

- `Free` > 2048B — хорошо
- `Free` < 1024B — осторожно
- `Free` < 512B — критично, риск краша

---

## Как работать с панелью во время отладки

### Сценарий 1 — Проверка стека при нагрузке

1. `F5` — запустить отладку
2. Поставить breakpoint в `Uart1Task` ([main.c](Core/Src/main.c))
3. Отправить команду через UART1
4. Когда программа остановится — посмотреть в RTOS Views → Tasks → `Uart1Task` → **Stack Free**
5. Если мало — увеличить стек в `main.c` (`osThreadNew` параметр `stack_size`)

### Сценарий 2 — Мониторинг heap в реальном времени

1. `F5` — запустить отладку
2. Несколько раз нажать **Pause** (кнопка ⏸ в панели отладки)
3. Каждый раз смотреть **Heap → Min Free** — это минимум за всё время
4. Если `Min Free` падает ниже 512B — проблема с памятью

### Сценарий 3 — Зависание задачи

Если программа "зависла" (LED не мигает):

1. Нажать **Pause** в панели отладки
2. В RTOS Views → Tasks посмотреть состояние каждой задачи
3. Если `LedTask` в состоянии `Blocked` слишком долго — проблема в коде
4. Если `Uart1Task` в `Running` постоянно — она захватила CPU (не отдаёт управление)

### Сценарий 4 — Просмотр периферийных регистров

Благодаря SVD-файлу, во время отладки доступна панель **CORTEX PERIPHERALS**:

1. В боковой панели Debug найти секцию **CORTEX PERIPHERALS**
2. Развернуть нужную периферию (USART1, SPI1, GPIOA и т.д.)
3. Видны все регистры и их текущие значения в реальном времени

---

## Сравнение: RTOS Views vs USART6 логи

| | USART6 логи (текущий способ) | RTOS Views |
| -- | ---------------------------- | ---------- |
| Обновление | Каждые 5 секунд | При каждом pause/breakpoint |
| Нужен COM-порт | Да | Нет |
| Данные задач | Только watermark в словах | Полная информация в байтах |
| Очереди | Нет | Да |
| Периферия | Нет | Да (через SVD) |
| Удобство | Нужен терминал | Прямо в IDE |
| Работает без отладчика | Да | Нет |

> Оба способа дополняют друг друга: USART6 логи удобны в полевых условиях,
> RTOS Views — при разработке за компьютером.

---

## Если панель RTOS Views пустая

**Причина 1:** Не добавлен `"rtos": "FreeRTOS"` в `launch.json`
→ Добавить строку, перезапустить отладку

**Причина 2:** Отладчик ещё не дошёл до `osKernelStart()` (FreeRTOS не инициализирован)
→ Поставить breakpoint после `osKernelStart()` или в любой задаче

**Причина 3:** OpenOCD не видит ST-Link
→ Проверить подключение: `openocd -f Run.cfg` — должен показать `Info : stm32f4x.cpu: hardware has 6 breakpoints`

**Причина 4:** Homebrew OpenOCD не установлен
→ `brew install open-ocd arm-none-eabi-gdb`

---

## Быстрый старт (TL;DR)

```text
1. Установить расширение: mcu-debug.rtos-views
2. launch.json уже настроен (rtos: FreeRTOS + svdFile)
3. F5 → дождаться main()
4. Cmd+Shift+P → "RTOS Views: Focus on RTOS View"
5. Поставить breakpoint после osKernelStart() для полных данных
6. Pause (⏸) для обновления данных
```

---
