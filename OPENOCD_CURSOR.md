# OPENOCD_CURSOR.md

Короткий рабочий гайд по OpenOCD в Cursor для проекта `F411CE` (Windows).

---

## 1) Рабочие параметры проекта

- Конфиг: `Run.cfg`
- MCU target: `stm32f4x`
- Интерфейс: `stlink-dap` + `dapdirect_swd`
- SWD clock: `4000 kHz`
- ELF: `Debug/F411CE.elf`

---

## 2) Почему раньше падало в Cursor

Ошибка вида:

`Can't find interface/stlink-dap.cfg`

означает, что OpenOCD запущен без каталога scripts.

Нужно передавать:

- `-s C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.3.200.202510310951\resources\openocd\st_scripts`

---

## 3) Проверочные команды (PowerShell)

## Проверка конфига

```powershell
& "C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.100.202501161620\tools\bin\openocd.exe" `
  -s "C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.3.200.202510310951\resources\openocd\st_scripts" `
  -f "Run.cfg" `
  -c "shutdown"
```

## Прошивка

```powershell
& "C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.100.202501161620\tools\bin\openocd.exe" `
  -s "C:\ST\STM32CubeIDE_1.17.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.3.200.202510310951\resources\openocd\st_scripts" `
  -f "Run.cfg" `
  -c "program Debug/F411CE.elf verify reset exit"
```

В норме:
- `Programming Finished`
- `Verified OK`
- `Resetting Target`

---

## 4) Что уже настроено в `.vscode`

- `tasks_windows.json`:
  - `Flash` использует `-s ...\st_scripts`
  - правильные пути к GCC/Make/OpenOCD из STM32CubeIDE 1.17.0

- `launch.json`:
  - `searchDir` включает `st_scripts` для запуска через `F5`

---

## 5) Быстрый troubleshooting

### ST-Link не найден

1. установить драйвер ST-Link (`STSW-LINK009`);
2. проверить USB-кабель и питание;
3. закрыть STM32CubeIDE/другие сессии OpenOCD.

### OpenOCD не запускается из терминала

- используйте полный путь к `openocd.exe` (как в командах выше);
- либо запускайте `Flash` задачей из Cursor.

### Cursor/VS Code видит старые пути

- перезапустить окно Cursor (`Developer: Reload Window`);
- убедиться, что открыт именно проект `F411CE`.

