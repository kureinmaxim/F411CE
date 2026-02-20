# README_RAM_RTOS.md

Руководство по памяти и RTOS-диагностике для `primGPT` на `STM32F411` (BlackPill).

---

# Шпаргалка: память в STM32 + FreeRTOS

## Общая картина: два типа памяти в микроконтроллере

У STM32F411CEUx есть два блока памяти:

```
┌─────────────────────────────────────────────────────────┐
│                    FLASH (512 KB)                        │
│                  Адреса: 0x08000000                      │
│                                                          │
│  Здесь живёт программа. Пишется один раз при прошивке.  │
│  После выключения питания — сохраняется.                │
│                                                          │
│  Что внутри:                                            │
│  - .isr_vector — таблица прерываний (вектора)           │
│  - .text       — машинный код всех функций              │
│  - .rodata     — строковые константы ("Hello\r\n")      │
│  - копия .data — начальные значения глобальных          │
│                  переменных (копируется в RAM при старте)│
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                     RAM (128 KB)                         │
│                  Адреса: 0x20000000                      │
│                                                          │
│  Здесь работает программа. Переменные, стеки, кучи.     │
│  После выключения питания — всё теряется.               │
└─────────────────────────────────────────────────────────┘
```

**Ключевое правило:** FLASH = "жёсткий диск" (код + константы), RAM = "оперативка" (переменные + стеки).

---

## Как RAM поделена между секциями

Линкер (файл `STM32F411CEUX_FLASH.ld`) размещает данные в RAM по порядку:

```
Адрес RAM          Секция              Что хранится
─────────────────────────────────────────────────────────────
0x20000000    ┌── .data (96 B)         Глобальные переменные
              │                        с начальным значением
              │                        (например: int x = 5;)
              │                        Их значения копируются
              │                        из FLASH при старте
              │                        (функция Reset_Handler)
              ├── .bss (17 904 B)      Глобальные переменные
              │                        без начального значения
              │                        (например: int y;)
              │                        Заполняются нулями
              │                        при старте
              │
              │   Сюда входят:
              │   - ucHeap[10240]      — куча FreeRTOS
              │   - Idle_Stack[400]    — стек Idle задачи
              │   - Idle_TCB           — TCB Idle задачи
              │   - Timer_Stack[800]   — стек Timer задачи
              │   - Timer_TCB          — TCB Timer задачи
              │   - huart1, huart6     — хендлы UART
              │   - hspi1              — хендл SPI
              │   - и прочие глобальные
              │
              ├── ._user_heap_stack    Резерв для C-runtime:
              │   (1 536 B)            - _Min_Heap_Size (512 B)
              │                          для malloc/free (без RTOS)
              │                        - _Min_Stack_Size (1024 B)
              │                          для MSP (Main Stack Pointer)
              │
              ├── [свободно]           ~111 KB не используется
              │   (85% RAM)            Можно увеличивать heap
              │                        или стеки задач
              │
0x20020000    └── _estack              MSP стартует здесь
                                       и растёт ВНИЗ (↓)
─────────────────────────────────────────────────────────────
```

**Откуда берутся числа:**
- `.data` и `.bss` — компилятор считает сколько места нужно глобальным переменным
- `._user_heap_stack` — задаётся в линкер-скрипте (строки 41-42 файла `STM32F411CEUX_FLASH.ld`):
  ```c
  _Min_Heap_Size = 0x200;  /* 512 B — резерв под C malloc */
  _Min_Stack_Size = 0x400; /* 1024 B — резерв под MSP */
  ```

---

## Три разных "стека" — не путать!

В проекте с FreeRTOS существуют **три отдельных вида стеков**:

```
┌──────────────────────────────────────────────────────┐
│ 1. MSP (Main Stack Pointer)                          │
│    Где задаётся: _Min_Stack_Size в .ld (0x400=1024B) │
│    Где живёт: верх RAM, растёт вниз от 0x20020000    │
│    Когда используется:                               │
│    - до запуска FreeRTOS (main → osKernelStart)      │
│    - во всех прерываниях (ISR)                       │
│    Кто регулирует: линкер-скрипт                     │
├──────────────────────────────────────────────────────┤
│ 2. Task Stacks (стеки задач FreeRTOS)                │
│    Где задаются: .stack_size в osThreadAttr_t         │
│    Где живут: внутри ucHeap (куча FreeRTOS)          │
│    Когда используются:                               │
│    - во время работы каждой задачи                   │
│    - локальные переменные функций задачи             │
│    Кто регулирует: программист в main.c              │
│                                                      │
│    В этом проекте (main.c, строки 64-80):            │
│    - defaultTask: 1024 B                             │
│    - LedTask:      512 B                             │
│    - Uart1Task:   2048 B                             │
├──────────────────────────────────────────────────────┤
│ 3. Static Stacks (Idle и Timer задачи)               │
│    Где задаются: FreeRTOSConfig.h                    │
│    - configMINIMAL_STACK_SIZE = 400 (слов) → Idle    │
│    - configTIMER_TASK_STACK_DEPTH = 800 (слов) → Tmr │
│    Где живут: static-массивы в .bss (НЕ в ucHeap!)   │
│    Создаются в: cmsis_os2.c (функции                 │
│    vApplicationGetIdleTaskMemory и                    │
│    vApplicationGetTimerTaskMemory)                    │
│    Кто регулирует: CubeMX → FreeRTOSConfig.h         │
└──────────────────────────────────────────────────────┘
```

**Важно:** `configMINIMAL_STACK_SIZE` и `configTIMER_TASK_STACK_DEPTH` задаются в **словах** (1 слово = 4 байта на Cortex-M4). Т.е. Idle stack = 400 слов = 1600 байт, Timer stack = 800 слов = 3200 байт.

А `.stack_size` в `osThreadAttr_t` задаётся в **байтах**.

---

## Две разных "кучи" — не путать!

```
┌──────────────────────────────────────────────────────┐
│ 1. FreeRTOS Heap (ucHeap) — ОСНОВНАЯ                 │
│                                                      │
│    Размер: configTOTAL_HEAP_SIZE = 10*1024 = 10240 B │
│    Файл: FreeRTOSConfig.h, строка 71                 │
│    Менеджер: heap_4.c (pvPortMalloc / vPortFree)     │
│    Где в RAM: массив ucHeap[10240] внутри .bss       │
│                                                      │
│    Что из неё выделяется:                            │
│    ┌────────────────────────────────────────────┐    │
│    │ TCB defaultTask  (~168 B)                  │    │
│    │ Stack defaultTask (1024 B)                 │    │
│    │ TCB LedTask       (~168 B)                 │    │
│    │ Stack LedTask     (512 B)                  │    │
│    │ TCB Uart1Task     (~168 B)                 │    │
│    │ Stack Uart1Task   (2048 B)                 │    │
│    │ uartQueue (40 элементов + overhead, ~120 B)│    │
│    │ heap_4 заголовки (~32 B)                   │    │
│    │ ─── Свободно: ~6000 B (59%) ───           │    │
│    └────────────────────────────────────────────┘    │
│                                                      │
│    TCB (Task Control Block) — служебная структура    │
│    FreeRTOS для каждой задачи (~168 байт).           │
│    Хранит: приоритет, указатель на стек, имя,        │
│    состояние задачи, списки ожидания.                │
│                                                      │
│    Если heap закончится — osThreadNew вернёт NULL,   │
│    xQueueCreate вернёт NULL.                         │
├──────────────────────────────────────────────────────┤
│ 2. C-runtime Heap — запасной, почти не используется  │
│                                                      │
│    Размер: _Min_Heap_Size = 0x200 = 512 B            │
│    Файл: STM32F411CEUX_FLASH.ld, строка 41           │
│    Менеджер: стандартный malloc/free из newlib        │
│    Где в RAM: секция ._user_heap_stack               │
│                                                      │
│    В проектах с FreeRTOS почти не нужна.             │
│    Нужна только если код вызывает обычный malloc().  │
│    Можно уменьшить до 0x100 если не используется.    │
└──────────────────────────────────────────────────────┘
```

---

## Что куда влияет: таблица регулировки

| Что хочу сделать | Где менять | Файл | На что влияет |
|---|---|---|---|
| Увеличить стек задачи | `.stack_size = XXXX` | `Core/Src/main.c` | Больше расход FreeRTOS heap. Нужно если `StkB free` маленький |
| Увеличить FreeRTOS heap | `configTOTAL_HEAP_SIZE` | `Core/Inc/FreeRTOSConfig.h` | Больше ucHeap в .bss. Нужно если не хватает места для новых задач/очередей |
| Добавить новую задачу | `osThreadNew(...)` | `Core/Src/main.c` | Расходует FreeRTOS heap (TCB + stack_size) |
| Добавить новую очередь | `xQueueCreate(N, size)` | `Core/Src/main.c` | Расходует FreeRTOS heap (N * size + overhead) |
| Увеличить MSP (для ISR) | `_Min_Stack_Size` | `STM32F411CEUX_FLASH.ld` | Больше резерв в ._user_heap_stack |
| Увеличить стек Idle | `configMINIMAL_STACK_SIZE` | CubeMX → FreeRTOS Config | Больше static-массив в .bss (в словах!) |
| Увеличить стек Timer | `configTIMER_TASK_STACK_DEPTH` | CubeMX → FreeRTOS Config | Больше static-массив в .bss (в словах!) |

---

## Что расходует стек задачи

Каждая задача FreeRTOS имеет свой стек. Он тратится на:

1. **Локальные переменные функции задачи:**
   ```c
   // Uart1Task — эти переменные лежат в стеке задачи:
   uint8_t command[UART_BUF_SIZE];  // 40 байт (если UART_BUF_SIZE=40)
   uint8_t ok[2] = {ID_BU, 0x00};  // 2 байта
   char wr_data[7];                 // 7 байт
   char rd_data[7];                 // 7 байт
   // + выравнивание до 4 байт
   ```

2. **Вложенные вызовы функций** — каждый вызов (HAL_GPIO_WritePin, fram_write, printf_uart3, ...) добавляет в стек:
   - адрес возврата (4 B)
   - сохранённые регистры (до 32 B)
   - локальные переменные вызванной функции

3. **printf_uart3 / log_printf** — особенно "дорогие" по стеку! snprintf/vsnprintf внутри может тратить 200-400 байт стека.

**Правило:** если задача вызывает printf-подобные функции — стек минимум 1024 B. Если ещё и работает с массивами/буферами — 2048 B.

---

## Жизненный цикл памяти при запуске

```
Включение питания / Reset
         │
         ▼
┌─ Reset_Handler (startup_stm32f411ceux.s) ─────────────────┐
│  1. MSP = _estack (0x20020000)     ← стек для main()      │
│  2. Копирует .data из FLASH в RAM  ← глобальные с init     │
│  3. Заполняет .bss нулями          ← глобальные без init   │
│  4. Вызывает SystemInit()          ← базовая настройка     │
│  5. Вызывает main()                                        │
└────────────────────────────────────────────────────────────┘
         │
         ▼
┌─ main() ──────────────────────────────────────────────────┐
│  HAL_Init()              ← настройка Flash, SysTick       │
│  SystemClock_Config()    ← HSE 25MHz → PLL → 96MHz        │
│  MX_GPIO/UART/SPI_Init() ← периферия                     │
│  osKernelInitialize()    ← инициализация планировщика     │
│                                                            │
│  xQueueCreate(40, 1)    ← выделяет ~120 B из ucHeap       │
│  osThreadNew(default)   ← выделяет TCB+1024 B из ucHeap   │
│  osThreadNew(Led)       ← выделяет TCB+512 B из ucHeap    │
│  osThreadNew(Uart1)     ← выделяет TCB+2048 B из ucHeap   │
│                                                            │
│  osKernelStart()        ← запускает планировщик            │
│  ┌────────────────────────────────────────────────────┐   │
│  │ Создаёт Idle задачу (static: .bss, 400 слов)      │   │
│  │ Создаёт Timer задачу (static: .bss, 800 слов)     │   │
│  │ Переключает MSP → PSP (Process Stack Pointer)     │   │
│  │ Начинает переключать задачи по приоритетам         │   │
│  └────────────────────────────────────────────────────┘   │
│  ↓ сюда код НИКОГДА не доходит (while(1) в конце main)    │
└────────────────────────────────────────────────────────────┘
         │
         ▼
┌─ FreeRTOS крутит задачи по приоритетам ───────────────────┐
│                                                            │
│  ► Uart1Task (AboveNormal) — обработка UART команд        │
│  ► defaultTask (Normal)    — диагностика каждые 5 сек     │
│  ► LedTask (Normal)        — мигание PC13                 │
│  ► Timer (Normal+2)        — таймеры FreeRTOS             │
│  ► Idle (самый низкий)     — когда все ждут               │
│                                                            │
│  Прерывания (ISR) — используют MSP, не стеки задач!       │
│  HAL_UART_RxCpltCallback → xQueueSendFromISR              │
└────────────────────────────────────────────────────────────┘
```

---

## Как читать диагностику (вывод defaultTask на USART6)

Каждые 5 секунд `defaultTask` выводит через USART6 (115200 бод):

```
HeapB F:5968 Min:5968 Tot:10240
HeapU now:41% peak:41%
StkB free D:328 L:408 U:1744
StkU % D:67 L:20 U:14
```

**Расшифровка построчно:**

| Строка | Поле | Значение | Что значит |
|---|---|---|---|
| HeapB | F:5968 | 5968 байт свободно в FreeRTOS heap | `xPortGetFreeHeapSize()` |
| | Min:5968 | Минимум свободного за всё время работы | `xPortGetMinimumEverFreeHeapSize()` — если Min сильно меньше F, значит были пики потребления |
| | Tot:10240 | Общий размер heap | `configTOTAL_HEAP_SIZE` |
| HeapU | now:41% | Сейчас занято 41% heap | (Tot - F) / Tot * 100 |
| | peak:41% | Максимум занятости за всё время | (Tot - Min) / Tot * 100 |
| StkB free | D:328 | defaultTask: 328 B стека свободно | `uxTaskGetStackHighWaterMark()` * 4 |
| | L:408 | LedTask: 408 B свободно | |
| | U:1744 | Uart1Task: 1744 B свободно | |
| StkU % | D:67 | defaultTask: 67% стека использовано | (stack_size - free) / stack_size * 100 |
| | L:20 | LedTask: 20% использовано | |
| | U:14 | Uart1Task: 14% использовано | |

**Когда тревожиться:**

| Метрика | Норма | Внимание | Опасно |
|---|---|---|---|
| StkB free | > 128 B | < 128 B | < 64 B — близко к переполнению |
| HeapU peak | < 70% | 70-85% | > 85% — нет запаса для новых объектов |
| Min << F | Min ~ F | Min < F/2 | Min ~ 0 — были критические пики |

---

## Частые ошибки и как их избегать

### 1. Переполнение стека задачи (Stack Overflow)

**Симптомы:** Hard Fault, случайные зависания, порча данных.

**Причина:** задаче назначили маленький `stack_size`, а в ней вызывается printf или большой буфер на стеке.

**Решение:** увеличить `.stack_size` в `main.c`. Следить за `StkB free`.

### 2. FreeRTOS heap закончился

**Симптомы:** `osThreadNew` возвращает NULL, `xQueueCreate` возвращает NULL.

**Причина:** `configTOTAL_HEAP_SIZE` слишком маленький для всех задач + очередей.

**Решение:** увеличить `configTOTAL_HEAP_SIZE` в `FreeRTOSConfig.h`. Проверить `HeapU peak`.

### 3. Путаница "слова vs байты"

**Ловушка:** `configMINIMAL_STACK_SIZE = 400` — это 400 **слов** = 1600 **байт**. А `.stack_size = 512` — это 512 **байт**.

**Правило:** всё что в `FreeRTOSConfig.h` — в словах. Всё что в `osThreadAttr_t` — в байтах.

### 4. Вызов printf из ISR

**Ловушка:** printf использует MSP (стек прерываний), который всего 1024 B. Длинный printf из ISR может переполнить MSP.

**Правило:** в ISR — только `xQueueSendFromISR()`, без printf. Логирование — только из задач.

---

# Руководство по проекту

## 1) Что важно в этом проекте

Даже при более комфортной RAM у `STM32F411`, проблемы остаются типовыми:

- переполнение стека задач;
- небезопасные строки логирования;
- неверный баланс static RAM / heap / task stacks.

Поэтому контролируем память в двух плоскостях:

1. build-time (`size`, `map`);
2. runtime (`HeapB/HeapU/StkB/StkU`).

---

## 2) Актуальные runtime-метрики в прошивке

Раз в 5 секунд выводятся:

- `HeapB F:%u Min:%u Tot:%u`
- `HeapU now:%u%% peak:%u%%`
- `StkB free D:%u L:%u U:%u`
- `StkU %% D:%u L:%u U:%u`

Расшифровка:

- `F` - текущий свободный heap;
- `Min` - минимальный остаток heap за все время;
- `Tot` - `configTOTAL_HEAP_SIZE`;
- `StkB free` - watermark стека в байтах;
- `StkU %` - процент занятого стека.

---

## 3) Где задаются heap и stack

### Task stacks

В `Core/Src/main.c` через `osThreadAttr_t.stack_size`:

- `defaultTask_attributes.stack_size = 1024`
- `LedTask_attributes.stack_size = 512`
- `Uart1Task_attributes.stack_size = 2048`

### FreeRTOS heap

В `Core/Inc/FreeRTOSConfig.h`:

- `configTOTAL_HEAP_SIZE = 10 * 1024`

### Linker reserve

В `.ld` задаются `_Min_Heap_Size` и `_Min_Stack_Size` для C-runtime.
Это отдельный слой от heap FreeRTOS.

---

## 4) Как проверять память после изменений

1. Сборка и `arm-none-eabi-size`.
2. Проверка `*.map`:
   - `.data`, `.bss`, крупные символы;
   - `ucHeap` (heap_4).
3. Прогон на плате под нагрузкой.
4. Фиксация worst-case:
   - `HeapU peak`;
   - минимальные `StkB free` по задачам.

---

## 5) Пороговые ориентиры

- `StkB free < 128 B` - зона внимания;
- `StkB free < 64 B` - критично;
- `StkB free = 0` - высокий риск overflow;
- `HeapU peak > 80%` - пора оптимизировать;
- `HeapU peak > 90%` - риск malloc-fail.

---

## 6) STM32CubeIDE: где смотреть метрики

- Build Console: `text data bss`.
- `Project Explorer`: файл `${ProjName}.map`.
- Линкерный флаг (если нужно вручную):
  - `-Wl,-Map=${ProjName}.map,--cref`

---

## 7) Таблица контроля

### Конфигурация памяти (текущая)

| Параметр | Значение | Источник |
|---|---:|---|
| FLASH total | 512 KB | linker script |
| RAM total | 128 KB (131 072 B) | linker script |
| `configTOTAL_HEAP_SIZE` | 10 240 B (10 KB) | FreeRTOSConfig.h |
| `_Min_Heap_Size` (C-runtime) | 0x200 (512 B) | .ld |
| `_Min_Stack_Size` (MSP) | 0x400 (1 024 B) | .ld |
| Stack defaultTask | 1 024 B | main.c |
| Stack LedTask | 512 B | main.c |
| Stack Uart1Task | 2 048 B | main.c |
| uartQueue | 40 × 1 B | main.c |

### Карта RAM (из .map)

```
0x20000000 ─── .data          96 B
0x20000060 ─── .bss       17 904 B  (включает ucHeap 10 240 B)
0x20004650 ─── ._user_heap_stack  1 536 B  (512 heap + 1024 MSP reserve)
0x20004C50 ─── [свободно]   111 536 B  (85% RAM)
0x20020000 ─── _estack (MSP, растёт вниз)
```

### Оценка FreeRTOS heap (расчётная)

| Объект | Размер (оценка) |
|---|---:|
| defaultTask (TCB + stack) | ~1 192 B |
| LedTask (TCB + stack) | ~680 B |
| Uart1Task (TCB + stack) | ~2 216 B |
| uartQueue (40×1 + overhead) | ~120 B |
| heap_4 заголовки блоков | ~32 B |
| **Итого занято (оценка)** | **~4 240 B (41%)** |
| **Свободно (оценка)** | **~6 000 B (59%)** |

*Idle и Timer задачи размещены статически в .bss (не из ucHeap).*

### Build-time метрики

| Версия | Изменение | text | data | bss | RAM static | FLASH % | RAM static % | HeapU est. | Риск | Комментарий |
|---|---|---:|---:|---:|---:|---:|---:|---|---|---|
| v0.1 | Перенос с F103C8T, -O0 Debug | 32 968 | 96 | 19 440 | 19 536 | 6.3% | 14.9% | ~41% | Низкий | 112 KB RAM свободно, heap 10 KB |
| v0.2 | | | | | | | | | | |
| v0.3 | | | | | | | | | | |

### Runtime метрики (заполнять после прошивки)

| Версия | HeapB F/Min/Tot | HeapU now/peak | StkB free D/L/U | StkU % D/L/U | Комментарий |
|---|---|---|---|---|---|
| v0.1 | 5968/5968/10240 | 41%/41% | 328/408/1744 | 67/20/14 | defaultTask 328 B free — выше порога 128 B, но стек загружен на 67% |
| v0.2 | | | | | |
| v0.3 | | | | | |
