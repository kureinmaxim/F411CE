# README_INTERRUPTS.md

Документ описывает систему прерываний в `primGPT` для `STM32F411` и ее связь с FreeRTOS.

---

## 1) Задействованные прерывания

### Системные (Cortex-M4)

- `SVC_Handler`
- `PendSV_Handler`
- `SysTick_Handler`
- `HardFault/MemManage/BusFault/UsageFault`

### Периферийные

- `USART1_IRQHandler`
  - вызывает `HAL_UART_IRQHandler(&huart1)`;
  - далее `HAL_UART_RxCpltCallback()` в `uart.c`.

- `TIM1_UP_TIM10_IRQHandler`
  - вызывает `HAL_TIM_IRQHandler(&htim1)`;
  - `HAL_TIM_PeriodElapsedCallback()` делает `HAL_IncTick()`.

---

## 2) Поток приема UART1

```text
USART1_RX byte
  -> USART1 IRQ
  -> USART1_IRQHandler()
  -> HAL_UART_IRQHandler(&huart1)
  -> HAL_UART_RxCpltCallback()
  -> xQueueSendFromISR(uartQueue, byte, &xHigherPriorityTaskWoken)
  -> HAL_UART_Receive_IT(..., 1)   // rearm RX
  -> portYIELD_FROM_ISR(...)       // при необходимости
  -> Uart1Task в task context
```

---

## 3) Приоритеты и FreeRTOS

Ключевые параметры в `FreeRTOSConfig.h`:

- `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`
- `configMAX_SYSCALL_INTERRUPT_PRIORITY` вычисляется из него.

Правило:

- ISR, где используются `...FromISR`, должны иметь допустимый приоритет относительно `configMAX_SYSCALL_INTERRUPT_PRIORITY`.

---

## 4) Что можно/нельзя в ISR

Можно:

- короткая фиксация события;
- отправка в очередь `xQueueSendFromISR`;
- обновление таймштампа;
- rearm приема `HAL_UART_Receive_IT`.

Нельзя:

- долгие циклы, парсинг протокола целиком;
- блокирующие вызовы (`HAL_Delay`);
- тяжелые `printf`;
- обычные (не ISR) API FreeRTOS.

---

## 5) Практические рекомендации

- Если прием "останавливается", первым делом проверять rearm `HAL_UART_Receive_IT`.
- Если callback не вызывается - проверять `USART1_IRQHandler`, NVIC enable и приоритет.
- Для роста скорости рассмотреть `DMA RX + ring buffer`.
