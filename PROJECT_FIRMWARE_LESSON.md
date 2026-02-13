# primGPT (STM32F411 + FreeRTOS): архитектура и алгоритм работы

Практический конспект по текущей прошивке `primGPT` в проекте `STM32F411/primGPT`.

---

## 1) Что это за прошивка

`primGPT` на BlackPill (`STM32F411`) - это RTOS-приложение с командным интерфейсом по UART, обработкой CRC16 и работой с FRAM по SPI.

Основные задачи:

- `Uart1Task` - прием/разбор команд, CRC, действия;
- `LedTask` - периодическая индикация;
- `defaultTask` - сервисные логи и диагностика памяти.

---

## 2) Технологический стек

- MCU: `STM32F411` (Cortex-M4)
- HAL: `stm32f4xx_hal`
- RTOS: `FreeRTOS + CMSIS-RTOS v2`
- Протокол: `UART1 + CRC16 (0xA001)`
- Память: `heap_4`, `configTOTAL_HEAP_SIZE = 10KB`

---

## 3) Интерфейсы и роли

- `USART1` (`38400`) - командный канал.
- `USART6` (`115200`) - debug/log канал.
- `SPI1` - FRAM.
- GPIO:
  - `PC13` - LED/сигнал команды;
  - `PC15` - маркер активности `Uart1Task`;
  - `PA4` - CS для FRAM.

---

## 4) Поток обработки UART-команды

```text
USART1 RX IRQ
 -> HAL_UART_RxCpltCallback
 -> xQueueSendFromISR(uartQueue, byte)
 -> Uart1Task: xQueueReceive
 -> накопление кадра
 -> пауза > UART_TIMEOUT_MS (20ms)
 -> process_crc(...)
 -> выполнение команды
 -> ответ (ID+status+CRC)
```

---

## 5) Реализованные команды (текущие)

- `0x70`:
  - `command[1]==0` -> `PC13 = SET`
  - `command[1]==1` -> `PC13 = RESET`

- `0x02`:
  - тестовая запись/чтение FRAM (`fram_write`, `fram_read`).

---

## 6) Диагностика в runtime

Раз в 5 секунд `defaultTask` печатает:

- `HeapB`, `HeapU`
- `StkB free`, `StkU %`

Это основной механизм контроля стабильности памяти на железе.

---

## 7) Что было важно при переносе с STM32F1

- Сохранена логика протокола и RTOS-пайплайн.
- Адаптирован debug-канал:
  - в F1 был `USART3`,
  - в F411 используется `USART6`.
- Сохранены безопасные правки:
  - ограничение длины лог-строк после `vsnprintf`;
  - увеличенный стек `defaultTask`;
  - runtime-метрики heap/stack.

---

## 8) Правило дальнейших изменений

- Менять код только в блоках `USER CODE BEGIN/END`.
- После каждого изменения делать цикл:
  1) Build (`size/map`);
  2) Прогон на плате;
  3) Фиксация `HeapU peak` и `StkB free`.

---

## 9) Короткий итог

Текущая архитектура корректная для embedded+RTOS:

- ISR короткие,
- основная логика в задачах,
- обмен ISR->Task через очередь,
- память контролируется в runtime метриками.
