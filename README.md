# Diamond Detector Signal Simulator

Qt-приложение: генерация сигнала алмазного детектора, модель усилителя и амплитудный спектр.

## Сборка

Компилятор Visual Studio Community 2022 17.14.37614.0 (amd 64)

Отладчик CDB

Профиль Qt 6.4.1 (msvc2019_64)

CMake generator - Ninja 

```bash
cmake -S . -B build
cmake --build build
```

В Qt Creator: Open Project → `CMakeLists.txt`. Цель: `DiamondDetectorSIgnalGenerator`.

## Структура

```
src/app/          main.cpp, MainWindow
src/core/         расчёт (без виджетов)
src/ui/           графики и вкладка усилителя
third_party/      QCustomPlot — не править
data/             примеры шаблона и спектров
```

Зависимости только вниз: `app` → `ui` → `core`.

## Вкладки

1. **Signal Generator** — шаблон импульса + амплитуды + времена событий → синтез → CSV.
2. **Amplifier Model** — CR-RC, усиление, шум, насыщение.
3. **Spectrum Analysis** — гистограммы пиков входа/выхода усилителя.

Экспорт CSV: `MainWindow::onExportCsv()` в `src/app/mainwindow.cpp`.

## Форматы `data/`

Шаблон импульса (`diamond_pulse_template.txt`) — **одно число на строку** (отсчёты). Пик нормируется при загрузке. Кнопка **Load Pulse Template**.

Спектр амплитуд — две колонки: `амплитуда  вес`. Пример: `amplitude_spectrum_example.txt`. Кнопка **Load Amplitude Spectrum**, режим амплитуд: *From file*.

CSV сигнала (вкладка усилителя): `time,amplitude` или одна колонка амплитуд.

## Куда править

| Задача | Где |
|---|---|
| Сборка импульсов в дорожку | `src/core/signal_synthesis_service.cpp` |
| CR-RC усилитель | `src/core/amplifier_model.cpp` |
| Poisson / фиксированный шаг | `src/core/Distribution.cpp` |
| Новый виджет | `src/ui/`, затем список в `CMakeLists.txt` |

## Сборка для release 

"C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe" --release "DiamondDetectorSignalGenerator.exe"