# README_RAM_RTOS.md

Руководство по памяти и RTOS-диагностике для `primGPT` на `STM32F411` (BlackPill).

---

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
