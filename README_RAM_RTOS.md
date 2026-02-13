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

## 7) Таблица контроля (шаблон)

| Версия | Изменение | text | data | bss | RAM static | HeapU now/peak | StkB free D/L/U | Риск | Комментарий |
|---|---|---:|---:|---:|---:|---|---|---|---|
| v0.1 | Базовый перенос на F411 |  |  |  |  |  |  |  |  |
| v0.2 | Настройка heap/stack |  |  |  |  |  |  |  |  |
| v0.3 | Финальная валидация |  |  |  |  |  |  |  |  |
