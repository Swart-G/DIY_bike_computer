# Формат данных production-прошивки

Конфигурация и recovery сохраняют `format_version: 1`. Новые поездки используют
`format_version: 2`; старые каталоги поездок v1 остаются читаемыми и не
переписываются. Устройство не создаёт фиктивные даты или координаты: идентификатор
заезда выдаётся в NVS, а геопозиция существует только после валидного fix от
авторизованного Android companion.

## Конфигурация

`/config/bike_config.json` — человекочитаемая конфигурация. Важные параметры также дублируются в NVS; корректный SD-файл имеет приоритет при старте. Некорректные значения заменяются безопасными defaults. Legacy-поле `display_brightness_percent` читается ради совместимости, но в UI 2.0 игнорируется: backlight работает в фиксированном штатном режиме.

```json
{"format_version":1,"wheel_circumference_m":2.194,"pulses_per_revolution":1,"stop_threshold_kmh":3.0,"auto_pause_enabled":true,"auto_pause_delay_ms":5000,"max_plausible_speed_kmh":100,"display_brightness_percent":80,"rgb_speed_indicator":{"enabled":true,"pin":48,"comparison_windows_ms":[2000,5000,10000],"stable_tolerance_2s_kmh":0.5,"stable_tolerance_5s_kmh":0.5,"stable_tolerance_10s_kmh":0.5,"brightness_percent":20},"battery":{"enabled":true,"adc_pin":6,"calibration_factor":1.0,"low_percent":29,"critical_percent":15}}
```

При чтении для совместимости также принимается прежнее
`rgb_speed_indicator.stable_tolerance_kmh`; оно становится допуском окна 2 секунды.

## Папка заезда

```text
/rides/ride_000001/
  meta.json
  samples.csv
  events.csv
  summary.json
```

`meta.json` содержит версию формата, `ride_id`, версию firmware, плату, экран, тач,
окружность колеса, pulses/revolution, active level датчика,
`location_source: "android_companion"` и nullable абсолютное время старта.

`samples.csv` всегда содержит единственный header:

```csv
sample_index,ride_time_ms,state,speed_kmh,raw_speed_kmh,distance_m,avg_speed_kmh,max_speed_kmh,elapsed_time_ms,recording_time_ms,moving_time_ms,pause_time_ms,pulse_count,rejected_pulse_count,battery_voltage,battery_percent,timestamp_utc_ms,latitude,longitude,altitude_m,gps_accuracy_m,gps_speed_mps,gps_age_ms
```

Строки append-only, decimal separator — точка. Интервал записи настраивается
(`log_sample_interval_ms`, по умолчанию 1000 ms). Во время PAUSED также записываются
samples с состоянием `PAUSED`.

Android отправляет `LocationFix` только для сообщённого ESP активного `ride_id` и не
сохраняет live fix в Room. Прошивка проверяет ID поездки, UTC-диапазон, широту,
долготу, высоту, точность и скорость. Последний принятый fix добавляется к строке лишь
пока его возраст по монотонному времени ESP не превышает 5000 ms; `gps_age_ms`
фиксирует этот возраст. Если свежего fix нет, все семь location-полей остаются
пустыми. Опциональные altitude/accuracy/speed могут быть пустыми независимо.
Повторение одного timestamp в соседних sample-строках означает удержание того же
свежего fix, а не новую GPS-точку.

`events.csv`:

```csv
ride_time_ms,event,details
```

Поддерживаемые события: `START`, `PAUSE`, `RESUME`, `FINISH`, `RECOVERED_AS_PAUSED`, `SD_ERROR`, `SD_RESTORED`, `BATTERY_LOW`, `BATTERY_CRITICAL`, `CONFIG_CHANGED`, `USB_STORAGE_REQUESTED`.

После штатного Finish создаётся атомарный `summary.json` с полями: `distance_m`,
`max_speed_kmh`, `average_moving_speed_kmh`, `average_recording_speed_kmh`,
`elapsed_time_ms`, `recording_time_ms`, `moving_time_ms`, `pause_time_ms`,
`stopped_time_ms`, accepted/rejected pulse count, `gps_fix_count` и battery
start/end/min/max voltage. `gps_fix_count` считает уникальные timestamp, реально
попавшие в samples текущей сессии. `logging_gap: true` означает, что часть данных была
буферизована или не могла быть записана из-за SD error.

## Recovery и надёжность

`/state/current_ride.json` содержит `format_version`, ride ID/folder, prior state, sample index, distance, timers, pulse counts и флаг logging gap. Он записывается через `.tmp`, flush/close, replacement/rename. Небольшой snapshot дублируется в Preferences/NVS на Start, Pause, Resume и периодически. После перезагрузки заезд всегда открывается как `PAUSED`.

CSV никогда не переписывается целиком. Events flush сразу; samples append-only. При потере SD скорость и статистика продолжаются, последние samples сохраняются в RAM ring buffer и выгружаются после успешного восстановления карты.
Если append не смог даже открыть файл, прошивка может один раз перемонтировать карту
на 1 МГц и повторить строку, поскольку в этом случае на носитель не попал ни один байт.
После `short write` автоматического повтора нет: возможный частичный хвост остаётся
диагностическим свидетельством и исключается риск дублирования строки.

## USB MSC

В каждый момент SD принадлежит либо firmware, либо USB host. Перед USB закрываются ride operations и сохраняется checkpoint. В USB режиме firmware не читает/не пишет FAT. После safe eject пользователь перезагружает устройство.
