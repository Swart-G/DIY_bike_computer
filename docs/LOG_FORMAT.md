# LOG_FORMAT.md

Формат файлов на SD-карте.

## Статус документа

Финальный формат логов поездок пока не утверждён.

На первом тестовом этапе обязательно реализовать только:

- обнаружение SD-карты;
- создание тестового текстового файла;
- чтение тестового файла обратно;
- показ результата на экране;
- доступ к SD-карте через USB Mass Storage.

Полноценные файлы заездов описаны ниже как рекомендуемый будущий формат. Его можно менять перед реализацией логирования.

## Тестовый файл первого этапа

Путь:

```text
/BIKE_SPEEDOMETER_SD_TEST.txt
```

Содержимое:

```text
Bike Speedometer SD test
If you can read this file, SD write/read works.
Device: ESP32-S3-N16R8
Display: ST7796
Touch: FT6336
```

Требования:

- файл создаётся через экран `SD test`;
- файл читается обратно прошивкой;
- считанный текст показывается на экране;
- файл должен быть виден на компьютере через USB Mass Storage.

## Config-файл

Путь:

```text
/config/bike_config.json
```

Назначение:

- хранить настройки устройства;
- позволять менять настройки через экран;
- позволять вручную править настройки через ПК, когда SD открыта как флешка.

Рекомендуемый начальный формат:

```json
{
  "version": 1,
  "wheel_circumference_m": 2.194,
  "stop_threshold_kmh": 3.0,
  "ui_update_interval_ms": 200,
  "log_sample_interval_ms": 1000,
  "graph_window_seconds": 60,
  "display_brightness_percent": 80,
  "sensor": {
    "pin": 4,
    "active_level": "LOW",
    "interrupt_edge": "FALLING",
    "pullup_enabled": true,
    "min_pulse_interval_ms": 50
  },
  "battery": {
    "enabled": false,
    "adc_pin": null,
    "divider_r_top_ohm": null,
    "divider_r_bottom_ohm": null,
    "calibration_factor": 1.0
  }
}
```

Если файла нет, прошивка должна создать его со значениями по умолчанию, если SD доступна.

Если файл повреждён, прошивка должна:

1. показать ошибку;
2. предложить восстановить defaults;
3. не падать.

## Рекомендуемая структура папок для будущих заездов

```text
/rides/
  2026/
    07/
      04/
        ride_2026-07-04_14-35-22/
          samples.csv
          events.csv
          summary.json
          meta.json
```

Такой формат удобен, потому что:

- файлы не лежат одной огромной кучей;
- заезды легко найти по дате;
- каждый заезд хранится в отдельной папке;
- summary можно быстро открыть без чтения большого samples-файла.

## Предварительный samples.csv

Статус: черновик будущего формата.

Путь:

```text
/rides/YYYY/MM/DD/ride_YYYY-MM-DD_HH-MM-SS/samples.csv
```

Рекомендуемые колонки:

```csv
sample_index,time_ms,ride_state,speed_kmh,distance_m,max_speed_kmh,avg_speed_kmh,moving_time_ms,elapsed_time_ms,pulse_count,battery_voltage
```

Описание:

| Колонка | Значение |
|---|---|
| `sample_index` | номер строки |
| `time_ms` | время от старта устройства или заезда |
| `ride_state` | `RIDING` или `PAUSED` |
| `speed_kmh` | текущая скорость |
| `distance_m` | расстояние текущего заезда |
| `max_speed_kmh` | максимум текущего заезда |
| `avg_speed_kmh` | средняя скорость по moving time |
| `moving_time_ms` | активное время без пауз |
| `elapsed_time_ms` | время от старта заезда, включая паузы |
| `pulse_count` | количество принятых импульсов |
| `battery_voltage` | напряжение аккумулятора, если доступно |

Частота записи по умолчанию:

```text
1 sample per second
```

То есть:

```json
"log_sample_interval_ms": 1000
```

## Предварительный events.csv

Статус: черновик будущего формата.

Путь:

```text
/rides/YYYY/MM/DD/ride_YYYY-MM-DD_HH-MM-SS/events.csv
```

Рекомендуемые колонки:

```csv
time_ms,event,details
```

Примеры:

```csv
0,START,"ride started"
125000,PAUSE,"user pressed pause"
240000,RESUME,"user pressed resume"
560000,FINISH,"ride finished"
```

События:

```text
START
PAUSE
RESUME
FINISH
RECOVERED_AS_PAUSED
SD_ERROR
CONFIG_CHANGED
```

## Предварительный summary.json

Статус: черновик будущего формата.

Путь:

```text
/rides/YYYY/MM/DD/ride_YYYY-MM-DD_HH-MM-SS/summary.json
```

Пример:

```json
{
  "version": 1,
  "started_at_source": "device_time",
  "ride_state": "FINISHED",
  "distance_m": 12340.5,
  "max_speed_kmh": 42.1,
  "avg_speed_kmh": 18.6,
  "elapsed_time_ms": 2700000,
  "moving_time_ms": 2380000,
  "pause_time_ms": 320000,
  "pulse_count": 5620,
  "wheel_circumference_m": 2.194,
  "stop_threshold_kmh": 3.0,
  "battery_min_voltage": null,
  "battery_max_voltage": null
}
```

## Предварительный meta.json

Статус: черновик будущего формата.

Путь:

```text
/rides/YYYY/MM/DD/ride_YYYY-MM-DD_HH-MM-SS/meta.json
```

Пример:

```json
{
  "firmware_version": "0.1.0-test",
  "board": "ESP32-S3-N16R8",
  "display": "ST7796",
  "touch": "FT6336",
  "sensor_pin": 4,
  "log_format_version": 1
}
```

## Recovery-файл

Для восстановления незавершённого заезда после перезагрузки рекомендуется файл:

```text
/state/current_ride.json
```

Пример:

```json
{
  "version": 1,
  "ride_folder": "/rides/2026/07/04/ride_2026-07-04_14-35-22",
  "last_state": "RIDING",
  "distance_m": 1530.2,
  "max_speed_kmh": 31.8,
  "moving_time_ms": 420000,
  "elapsed_time_ms": 500000,
  "pulse_count": 698,
  "last_saved_ms": 500000
}
```

После перезагрузки:

- если `current_ride.json` найден;
- и указанный заезд существует;
- прошивка должна открыть экран восстановления;
- восстановленное состояние должно быть `PAUSED`, даже если до перезагрузки было `RIDING`.

## Безопасная запись

Рекомендованная стратегия:

- не писать на SD каждое изменение UI;
- samples писать буфером;
- flush делать периодически;
- summary писать в temp-файл и потом переименовывать;
- recovery обновлять атомарно, если возможно.

Рекомендуемые пути для атомарной записи:

```text
/state/current_ride.tmp
/state/current_ride.json
```

Алгоритм:

1. Записать новый JSON в `.tmp`.
2. Закрыть файл.
3. Удалить старый `.json`.
4. Переименовать `.tmp` в `.json`.

## USB Mass Storage и файловая система

Когда SD-карта открыта на компьютере через USB Mass Storage:

- прошивка не должна писать samples;
- прошивка не должна менять config;
- прошивка не должна обновлять recovery;
- UI должен показывать, что SD занята USB.

Рекомендуемая логика:

```text
normal mode -> firmware owns SD
usb storage mode -> host owns SD
```

Выход из USB Storage для первого варианта:

```text
safe eject on computer -> reboot device
```

## Что пока не фиксировать

Пока не нужно окончательно фиксировать:

- точный CSV-формат заездов;
- точный JSON summary;
- наличие GPX/FIT/TCX экспорта;
- синхронизацию времени;
- BLE/Wi-Fi экспорт.

Эти функции можно добавить позже после проверки железа.
