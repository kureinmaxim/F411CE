# Алгоритм загрузки и работы primGPT_F411

Полное описание того, что происходит от подачи питания до работающей системы с тремя задачами FreeRTOS.
Контроллер: **STM32F411CEUx** (Cortex-M4F, BlackPill). Прошивка: `Debug/F411CE.elf`.

---

## Что показал отладчик

При нажатии F5 (Debug) GDB сначала подключился к уже работающему контроллеру и поймал его прямо в FreeRTOS:

```text
frame: addr="0x0800080c", func="Uart1Task",
file="../Core/Src/main.c", line="163"
```

Это не ошибка — контроллер **уже работал** с прошлого сеанса. Отладчик подключился и сделал halt прямо там, где был процессор: внутри `Uart1Task`, на строке проверки очереди.

После этого GDB выполнил `monitor reset halt` и перезагрузил прошивку:

```text
xPSR: 0x01000000 pc: 0x0800137c msp: 0x20020000
```

Вот это уже настоящая точка после сброса — `Reset_Handler`. После прошивки ещё один reset, и GDB дал `continue` до `runToEntryPoint: "main"`. Остановился на:

```text
Thread 2 "Current Execution" hit Temporary breakpoint 2, main () at ../Core/Src/main.c:182
182    last_rx_time = 0;
```

Это первая строка `main()` (в блоке `USER CODE BEGIN 1`, до `HAL_Init()`).

### Что такое halt

**halt** — команда принудительной остановки процессора через SWD/JTAG. Процессор замирает на текущей инструкции, регистры и память застывают. Ни одна инструкция не выполняется, пока отладчик не скажет "продолжай".

### Что происходит при нажатии F5

| Шаг | Что делает | Результат |
| ---- | ---------- | --------- |
| 1 | OpenOCD подключается через ST-Link (SWD) | Связь установлена |
| 2 | OpenOCD делает halt | Процессор остановлен (в данном случае — в FreeRTOS задаче) |
| 3 | `monitor reset halt` | Аппаратный сброс, halt сразу после; PC = 0x0800137c (Reset_Handler) |
| 4 | `load` — прошивка ELF в Flash | 33244 байт за ~0.2 с; rate 169 KB/s |
| 5 | Ещё раз `monitor reset halt` | Гарантированно чистый старт с новой прошивкой |
| 6 | Временный breakpoint на `main()`, `continue` | Процессор проходит Reset_Handler → main() |
| 7 | Остановка на `main():182` | Начало C-кода, FreeRTOS ещё не запущен |

### Что такое MSP (Main Stack Pointer)

У Cortex-M4F **два** указателя стека:

| Указатель | Полное имя | Когда используется |
| --------- | ---------- | ------------------ |
| **MSP** | Main Stack Pointer | При сбросе, в прерываниях (ISR), в `main()` до запуска FreeRTOS |
| **PSP** | Process Stack Pointer | FreeRTOS переключает каждую задачу на свой PSP |

Как это работает в проекте:

- **До `osKernelStart()`** — процессор использует **MSP** (= `0x20020000`, вершина 128 KB RAM). Весь `main()`, вся инициализация HAL — на MSP.
- **После `osKernelStart()`** — каждая задача получает **свой PSP**, указывающий на стек из FreeRTOS heap:

| Задача | Стек (PSP) |
| ------- | ---------- |
| Uart1Task | 2048 байт из heap |
| defaultTask | 2048 байт из heap |
| LedTask | 512 байт из heap |

- **Прерывания** (`USART1_IRQHandler`, `TIM1_IRQHandler`) — **всегда** используют MSP, даже когда FreeRTOS работает. Поэтому в linker script зарезервировано 1 KB под MSP (`_Min_Stack_Size = 0x400`) — стек для всех ISR.

---

## Фаза 0: Аппаратный сброс (до первой инструкции)

```text
Питание / Reset кнопка / ST-Link reset
         │
         ▼
┌─────────────────────────────────────────┐
│  Cortex-M4F аппаратная логика:          │
│  1. Читает адрес 0x08000000 → _estack   │
│     (загружает в MSP = 0x20020000)      │
│  2. Читает адрес 0x08000004 → адрес     │
│     Reset_Handler                       │
│  3. Переходит на Reset_Handler          │
└─────────────────────────────────────────┘
```

Процессор **аппаратно** берёт два первых слова из Flash:

- `0x08000000` — начальное значение стека (MSP) → вершина RAM (`0x20020000` = `0x20000000 + 128K`)
- `0x08000004` — адрес `Reset_Handler` → туда прыгает процессор

Эти значения берутся из таблицы векторов прерываний (`g_pfnVectors` в `startup_stm32f411ceux.s`).

Таблица векторов — массив адресов. В F411CE она значительно длиннее, чем в F103:

| Адрес Flash | Смещение | Что лежит |
| ----------- | -------- | --------- |
| 0x08000000 | 0x000 | `_estack` (начальный SP = 0x20020000) |
| 0x08000004 | 0x004 | `Reset_Handler` — сюда прыгает CPU после сброса |
| 0x08000008 | 0x008 | `NMI_Handler` |
| 0x0800000C | 0x00C | `HardFault_Handler` |
| 0x08000010 | 0x010 | `MemManage_Handler` |
| 0x08000014 | 0x014 | `BusFault_Handler` |
| 0x08000018 | 0x018 | `UsageFault_Handler` |
| ... | ... | ... |
| 0x08000198 | 0x198 | `USART1_IRQHandler` — прерывание UART1 |
| ... | ... | ... |
| 0x08000208 | 0x208 | `USART6_IRQHandler` — отладочный UART |
| ... | ... | ... |
| 0x08000244 | 0x244 | `FPU_IRQHandler` — прерывание FPU |

Итого: **101 вектор** (404 байта) — больше, чем у F103, из-за большего числа периферии.

Таблица **не требует ручного редактирования**:

- Каждый Handler объявлен `.weak` — по умолчанию все указывают на `Default_Handler` (бесконечный цикл)
- Когда в C-коде пишется функция с точно таким именем (например `USART1_IRQHandler`), линкер автоматически подставляет её адрес
- Если добавить через CubeMX новое прерывание, CubeMX сгенерирует обработчик в `stm32f4xx_it.c`, и линкер сам подставит адрес

```asm
g_pfnVectors:
  .word _estack          ← MSP (0x20020000)
  .word Reset_Handler    ← PC  (первая инструкция после сброса)
  .word NMI_Handler
  .word HardFault_Handler
  ...                    ← ещё 97 векторов
  .word USART1_IRQHandler
  .word USART6_IRQHandler
  .word FPU_IRQHandler
```

---

## Фаза 1: Reset_Handler (ассемблер, startup_stm32f411ceux.s)

Отладчик после `monitor reset halt` остановил процессор на `0x0800137c` — это и есть `Reset_Handler`. Ассемблерный код готовит C-среду:

```text
Reset_Handler (0x0800137c)
    │
    ├─ ldr sp, =_estack
    │     Явная загрузка MSP = 0x20020000 (вершина 128KB RAM)
    │     (Cortex-M4F тоже делает это аппаратно при сбросе,
    │      но startup дублирует для надёжности)
    │
    ├─ 1. bl SystemInit
    │     Минимальная настройка: сброс регистров тактирования RCC,
    │     установка VTOR (адрес таблицы векторов = 0x08000000).
    │     После этого процессор всё ещё на внутреннем RC-генераторе HSI 16 МГц.
    │     (F411CE использует HSI 16 МГц по умолчанию, не 8 МГц как F103)
    │
    ├─ 2. Копирование .data из Flash в RAM
    │     Инициализированные глобальные переменные
    │     (например, char wr_data[7] = {'K','U','R','E','I','N'} в Uart1Task)
    │     хранятся во Flash. Startup копирует их в RAM (_sdata → _edata).
    │
    ├─ 3. Обнуление .bss в RAM
    │     Неинициализированные глобальные переменные
    │     (например, uint32_t last_rx_time) заполняются нулями (_sbss → _ebss).
    │
    ├─ 4. bl __libc_init_array
    │     Вызов C-конструкторов (static init, newlib init).
    │
    └─ 5. bl main
          Переход в main() — С-код.
```

### Карта памяти на этом этапе

```text
FLASH (512 KB)                          RAM (128 KB)
0x08000000 ┌──────────────┐           0x20000000 ┌──────────────┐
           │ .isr_vector  │ 408 B                │ .data        │ ← скопировано из Flash
           │ (таблица     │                      │ (глобальные  │   (96 байт)
           │  векторов)   │                      │  с начальным │
           ├──────────────┤                      │  значением)  │
           │ .text        │ 32276 B              ├──────────────┤
           │ (весь код)   │                      │ .bss         │ ← обнулено
           │              │                      │ (глобальные  │
           ├──────────────┤                      │  без значения)│
           │ .rodata      │ 448 B                ├──────────────┤
           │ (строки,     │                      │ FreeRTOS     │
           │  константы)  │                      │ heap (10 KB) │
           ├──────────────┤                      ├──────────────┤
           │ .ARM         │ 8 B                  │              │
           │ .init_array  │ 4 B                  │ (свободно    │
           │ .fini_array  │ 4 B                  │ ~115 KB)     │
           ├──────────────┤                      │              │
           │ .data init   │ 96 B ──копирование──▶├──────────────┤
           │ (образ для   │                      │ Heap (malloc)│ 512 B min
           │  RAM)        │                      ├──────────────┤
           │              │                      │ Stack (MSP)  │ 1 KB min
0x08008400 └──────────────┘                      │ (ISR-стек)   │
           (занято 33244 B)           0x20020000 │ _estack      │ ← MSP стартует тут
                                                 └──────────────┘
```

> Из лога отладчика: `load-size="33244"` — именно столько байт было залито в Flash.

---

## Фаза 2: main() — инициализация (до запуска FreeRTOS)

GDB остановился на `main():182` — строка `last_rx_time = 0;`. Это **первая строка `main()`**, она в блоке `USER CODE BEGIN 1`, до вызова `HAL_Init()`:

```c
main()
  │
  ├─ 1. last_rx_time = 0                   ← строка 182 (breakpoint сработал здесь)
  │     Инициализация глобальной переменной вручную до HAL_Init()
  │
  ├─ 2. reset_cause = RCC->CSR             ← читаем флаги причины сброса
  │     RCC->CSR |= RCC_CSR_RMVF           ← очищаем флаги
  │
  ├─ 3. HAL_Init()
  │     ├─ Настройка Flash prefetch
  │     ├─ Запуск SysTick → HAL_GetTick() начинает считать миллисекунды
  │     └─ Настройка NVIC приоритетов (группы 4: все биты = preemption)
  │        Время тактирования: всё ещё HSI 16 МГц
  │
  ├─ 4. SystemClock_Config()
  │     HSE 25 МГц (кварц на BlackPill) → PLL → SYSCLK = 96 МГц
  │     ├─ PLLM=25, PLLN=192, PLLP=2 → 25×192/(25×2) = 96 МГц
  │     ├─ AHB  = 96 МГц (÷1)
  │     ├─ APB1 = 48 МГц (÷2) — для USART2, SPI2, I2C, TIM2-5
  │     ├─ APB2 = 96 МГц (÷1) — для USART1, USART6, SPI1, TIM1
  │     └─ Flash latency = 3 wait states (для 96 МГц; F103 нужно было 2)
  │
  ├─ 5. Инициализация периферии
  │     ├─ MX_GPIO_Init()
  │     │   ├─ Включить тактирование GPIOC, GPIOH, GPIOA
  │     │   ├─ PC13, PC15 → выход (LED, индикатор активности)
  │     │   └─ PA4 → выход (FRAM chip select)
  │     │
  │     ├─ MX_USART1_UART_Init()
  │     │   ├─ USART1: 38400 бод, 8N1
  │     │   └─ HAL_UART_Receive_IT() → запуск приёма первого байта по прерыванию
  │     │
  │     ├─ MX_SPI1_Init()
  │     │   └─ SPI1: Master, prescaler/128 → 96МГц/128 = 750 кГц, CPOL=0 CPHA=0
  │     │
  │     └─ MX_USART6_UART_Init()
  │         └─ USART6: 115200 бод, 8N1 (отладочный вывод; вместо USART3 на F103)
  │
  ├─ 6. Лог в USART6
  │     RCC->CSR |= RCC_CSR_RMVF           ← повторная очистка флагов
  │     log_printf("Rst:0x%08lX\r\n", reset_cause)
  │     HAL_Delay(1000)                    ← пауза 1 секунда
  │     log_printf("Main started, time: %lu ms\r\n", HAL_GetTick())
  │
  ├─ 7. FRAM
  │     fram_cfg_setup(&cfgFRAM) + fram_init(&fram, &cfgFRAM) → готовность SPI FRAM
  │
  ├─ 8. osKernelInitialize()
  │     ├─ Инициализация FreeRTOS ядра
  │     ├─ Создание Idle-задачи (внутренняя, приоритет 0)
  │     └─ Выделение heap (10 KB из RAM) для задач
  │
  ├─ 9. Создание очереди и задач
  │     ├─ uartQueue = xQueueCreate(40, sizeof(char))  ← очередь 40 байт
  │     ├─ osThreadNew(StartDefaultTask, 2048B, Normal)
  │     ├─ osThreadNew(LedTask, 512B, Normal)
  │     └─ osThreadNew(Uart1Task, 2048B, AboveNormal)
  │
  └─ 10. osKernelStart()
         ├─ Запуск планировщика FreeRTOS
         ├─ Настройка SysTick для FreeRTOS (1 кГц тик = 1 мс)
         ├─ Переключение на Uart1Task (наивысший приоритет)
         └─ *** main() больше НИКОГДА не возвращается ***
```

---

## Фаза 3: Работающая система (FreeRTOS)

После `osKernelStart()` управление полностью у планировщика. Три задачи + Idle работают по вытеснению:

```text
Время →
         ┌─────┐     ┌─────┐     ┌─────┐
Uart1Task│ run │     │ run │     │ run │   Приоритет: AboveNormal (25)
(2048B)  └──┬──┘     └──┬──┘     └──┬──┘   Обработка команд UART1
            │           │           │
     ┌──────┴─────┬─────┴──────┬────┴──────┐
     │   ┌───┐    │   ┌───┐   │   ┌───┐   │
Dflt │   │run│    │   │run│   │   │run│   │   Приоритет: Normal (24)
(2048B)  └─┬─┘        └─┬─┘       └─┬─┘       Диагностика каждые 5с
           │             │           │
     ┌─────┴───┬─────────┴───┬───────┴─────┐
     │ ┌──┐    │   ┌──┐     │   ┌──┐      │
Led  │ │r │    │   │r │     │   │r │      │   Приоритет: Normal (24)
(512B) └──┘        └──┘         └──┘           LED каждые 2с
     │         │             │             │
     │  ┌──────┴─────────────┴─────────────┴─
Idle │  │████████████████████████████████████  Приоритет: 0
     │  └────────────────────────────────────  Работает когда все спят
```

### Задача: Uart1Task (обработка команд)

```text
Uart1Task (приоритет AboveNormal)
    │
    ├─ uart1_put_ch('S')           ← сигнал «задача запущена»
    │
    └─ Бесконечный цикл:
         │
         ├─ Toggle PC15             ← индикация активности
         │
         ├─ xQueueReceive(uartQueue, 2мс таймаут)
         │   ├─ Есть байт → добавить в command[index++]
         │   └─ Нет байта → пропустить
         │
         ├─ Проверка таймаута (20мс тишины = конец кадра):
         │   │
         │   └─ Если index > 0 И прошло > 20мс с последнего байта:
         │       │
         │       ├─ process_crc(command, index, true)
         │       │   └─ CRC16-MODBUS проверка
         │       │
         │       ├─ Если CRC верна:
         │       │   ├─ command[0] == 0x70 → GPIO управление LED
         │       │   │   ├─ [1]==0 → PC13 HIGH
         │       │   │   └─ [1]==1 → PC13 LOW
         │       │   │
         │       │   └─ command[0] == 0x02 → FRAM тест
         │       │       └─ Запись/чтение "KUREIN" по адресу 0x0150
         │       │
         │       ├─ Отправить ответ в UART1: [ID_BU, 0x00, CRC_L, CRC_H]
         │       ├─ Отправить ответ в UART6: [ID_BU, 0x00, CRC_L, CRC_H]
         │       ├─ Очистить очередь (сбросить мусор)
         │       └─ index = 0
         │
         └─ vTaskDelay(3мс)         ← отдать CPU другим задачам
```

### Задача: StartDefaultTask (диагностика)

```text
StartDefaultTask (приоритет Normal)
    │
    ├─ printf_uart3("Default Task running...")
    │
    └─ Каждые 1000мс (vTaskDelay):
         │
         ├─ printf_uart3("Tick:%lu", HAL_GetTick())
         │
         └─ Каждые 5 итераций (doп. диагностика):
              ├─ HeapB F:XXXX Min:XXXX Tot:10240
              ├─ HeapU now:XX% peak:XX%
              ├─ StkB free D:XXX L:XXX U:XXX
              └─ StkU % D:XX L:XX U:XX
```

### Задача: LedTask (светодиод)

```text
LedTask (приоритет Normal)
    │
    └─ Каждые 2000мс:
         └─ HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)    ← LED мигает (период 4с)
```

> Обратите внимание: у F103 период был 1с (`vTaskDelay(1000)`), у F411CE — 2с (`vTaskDelay(2000)`).

---

## Обработка прерывания UART1 (ISR-контекст)

Это происходит **вне задач**, в контексте прерывания:

```text
Байт приходит по UART1 (PA10)
         │
         ▼
USART1_IRQHandler()                    ← аппаратное прерывание (приоритет ≥ 5)
    │
    └─ HAL_UART_IRQHandler(&huart1)
         │
         └─ HAL_UART_RxCpltCallback()  ← наш callback в uart.c
              │
              ├─ Читаем байт из uart1_rx_buf[head]
              │
              ├─ Продвигаем head (кольцевой буфер)
              │   └─ Если буфер полон → сброс head=0, tail=0
              │
              ├─ xQueueSendFromISR(uartQueue, &ch)
              │   └─ Байт → очередь → Uart1Task проснётся
              │
              ├─ last_rx_time = xTaskGetTickCountFromISR()
              │   └─ Метка времени для детекции конца кадра (20мс)
              │
              ├─ HAL_UART_Receive_IT(..., 1)
              │   └─ Перезапуск приёма следующего байта
              │
              └─ portYIELD_FROM_ISR() если Uart1Task имеет
                 более высокий приоритет, чем текущая задача
```

> **ISR-приоритеты**: все прерывания, вызывающие FreeRTOS API, должны иметь приоритет ≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5). Числовое значение приоритета NVIC ≥ 5 означает, что вызов `...FromISR()` безопасен.

---

## Полная временная линия (первые ~2 секунды)

```text
t=0мс     ─── Питание / Сброс ───
          │ Cortex-M4F: MSP = 0x20020000, PC = Reset_Handler (0x0800137c)
          │
t~0.001мс │ Reset_Handler: ldr sp, =_estack (загрузка MSP = 0x20020000)
          │ bl SystemInit() — сброс RCC, установка VTOR
          │ Копирование .data (96 байт), обнуление .bss
          │ __libc_init_array()
          │ Прыжок в main()   ← *** GDB остановился здесь (main():182) ***
          │
t~0.01мс  │ main(): last_rx_time=0, читаем reset_cause из RCC->CSR
          │ HAL_Init(), SysTick запущен (16 МГц, временно)
          │
t~0.1мс   │ SystemClock_Config(): HSE 25МГц → PLL → 96МГц
          │ Flash latency = 3 wait states
          │ Теперь всё работает на полной скорости
          │
t~0.5мс   │ GPIO, USART1, SPI1, USART6 инициализированы
          │ USART1 прерывание включено (ожидание первого байта)
          │
t~1мс     │ USART6: "Rst:0xXXXXXXXX"
          │
t~1001мс  │ HAL_Delay(1000) — пауза 1 секунда
          │ USART6: "Main started, time: 1001 ms"
          │
t~1002мс  │ FRAM init
          │
t~1003мс  │ osKernelInitialize() + создание очереди + 3 задачи
          │
t~1004мс  │ osKernelStart()
          │ ═══════════════════════════════════════
          │ *** С этого момента — FreeRTOS ***
          │ *** main() больше не выполняется ***
          │ ═══════════════════════════════════════
          │
t~1005мс  │ Uart1Task: uart1_put_ch('S')
          │ StartDefaultTask: "Default Task running..."
          │ LedTask: первый toggle PC13
          │
t~2005мс  │ StartDefaultTask: "Tick:2005"
          │
t~3005мс  │ LedTask: второй toggle PC13 (период 2с, не 1с!)
          │
t~6005мс  │ StartDefaultTask: первый вывод диагностики
          │   HeapB / HeapU / StkB / StkU
          │
          └─── ... система работает бесконечно ───
```

---

## Особенности F411CE vs F103C8

| Параметр | F103C8 (старый) | F411CE (новый) |
| --------- | --------------- | -------------- |
| Ядро | Cortex-M3 | Cortex-M4F (FPU!) |
| Flash | 64 KB | 512 KB |
| RAM | 20 KB | 128 KB |
| MSP (_estack) | 0x20005000 | 0x20020000 |
| Clock | HSE 8МГц → 72МГц | HSE 25МГц → 96МГц |
| Flash latency | 2 wait states | 3 wait states |
| Startup default clk | HSI 8 МГц | HSI 16 МГц |
| Debug UART | USART3 | USART6 |
| SPI1 clock | 4.5 МГц | 750 кГц (/ 128) |
| LED период | 1с | 2с |
| Прерывание HAL Tick | SysTick | TIM1 (через `HAL_TIM_PeriodElapsedCallback`) |
| Аппаратных breakpoints | 6 | 6 |

> **Важно про HAL Tick:** В F411CE таймер `HAL_GetTick()` тикает от **TIM1**, а не от SysTick. SysTick отдан FreeRTOS. Поэтому в `main.c` есть `HAL_TIM_PeriodElapsedCallback` — именно он вызывает `HAL_IncTick()`.

---

## Про сообщения отладчика

### Ошибка watch на `crc`

```text
21^error,msg="-var-create: unable to create variable object"
watch -var-create: unable to create variable object (from var-create ... @ "crc")
```

**Причина:** переменная `crc` объявлена внутри функции `Uart1Task()`:

```c
uint16_t crc = 0;
```

В момент остановки на `main():182` функция `Uart1Task` ещё не создана и не запущена — она не находится в стеке вызовов. GDB не может создать watch на локальную переменную вне её scope. **Это нормально**, просто watch сработает позже, когда поставите breakpoint внутри `Uart1Task`.

### `[Remote target exited]` и смена thread-id

```text
-> [Remote target exited]
-> [Switching to Thread 1]
-> Thread 2 "Current Execution" hit Temporary breakpoint 2, main () at ...
```

**Причина:** при `monitor reset halt` GDB теряет первоначальный Thread 1 (который был в FreeRTOS задаче), и после загрузки прошивки создаётся новый Thread 2. Это нормальное поведение Cortex-Debug при перезагрузке таргета.

### Аппаратные breakpoints (лимит 6)

Cortex-M4F имеет **6 аппаратных breakpoints**. При выполнении из Flash программные breakpoints невозможны — только аппаратные.

`runToEntryPoint: "main"` автоматически использует **1 временный breakpoint** на `main()`. Итого доступно пользователю:

| Ресурс | Лимит | Примечание |
| ------- | ----- | ---------- |
| Hardware breakpoints | 6 | Физическое ограничение Cortex-M4F |
| Hardware watchpoints | 4 | Для отслеживания изменения переменных |
| `runToEntryPoint` | -1 слот | Автоматический временный breakpoint |
| **Доступно пользователю** | **5** | При включённом `runToEntryPoint: "main"` |

---

*Файлы: `startup_stm32f411ceux.s` → `system_stm32f4xx.c` → `main.c` → `uart.c`*
*Linker: `STM32F411CEUX_FLASH.ld` (RAM: 128K @ 0x20000000, Flash: 512K @ 0x08000000)*
*Toolchain: arm-none-eabi-gcc 14.2.Rel1 · GDB 15.2 · OpenOCD (Homebrew) · Cortex-Debug 1.13.0-pre6*
