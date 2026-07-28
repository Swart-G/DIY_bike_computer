# Формат данных production-прошивки

Все форматы имеют `format_version: 1`. Устройство не создаёт фиктивные даты: так как GPS/RTC нет, идентификатор заезда выдаётся в NVS.

## Конфигурация

`/config/bike_config.json` — человекочитаемая конфигурация. Важные параметры также дублируются в NVS; корректный SD-файл имеет приоритет при старте. Некорректные значения заменяются безопасными defaults.

```json
{"format_version":1,"wheel_circumference_m":2.194,"pulses_per_revolution":1,"stop_threshold_kmh":3.0,"max_plausible_speed_kmh":100,"display_brightness_percent":80,"battery":{"enabled":true,"adc_pin":6,"calibration_factor":1.0,"low_percent":29,"critical_percent":15}}
```

## Папка заезда

```text
/rides/ride_000001/
  meta.json
  samples.csv
  events.csv
  summary.json
```

`meta.json` содержит версию формата, `ride_id`, версию firmware, плату, экран, тач, окружность колеса, pulses/revolution, active level датчика и `started_at: null`, `started_at_source: "unavailable"`.

`samples.csv` всегда содержит единственный header:

```csv
sample_index,ride_time_ms,state,speed_kmh,raw_speed_kmh,distance_m,avg_speed_kmh,max_speed_kmh,elapsed_time_ms,recording_time_ms,moving_time_ms,pause_time_ms,pulse_count,rejected_pulse_count,battery_voltage,battery_percent
```

Строки append-only, decimal separator — точка. Интервал записи настраивается (`log_sample_interval_ms`, по умолчанию 1000 ms). Во время PAUSED также записываются samples с состоянием `PAUSED`.

`events.csv`:

```csv
ride_time_ms,event,details
```

Поддерживаемые события: `START`, `PAUSE`, `RESUME`, `FINISH`, `RECOVERED_AS_PAUSED`, `SD_ERROR`, `SD_RESTORED`, `BATTERY_LOW`, `BATTERY_CRITICAL`, `CONFIG_CHANGED`, `USB_STORAGE_REQUESTED`.

После штатного Finish создаётся атомарный `summary.json` с полями: `distance_m`, `max_speed_kmh`, `average_moving_speed_kmh`, `average_recording_speed_kmh`, `elapsed_time_ms`, `recording_time_ms`, `moving_time_ms`, `pause_time_ms`, `stopped_time_ms`, accepted/rejected pulse count и battery start/end/min/max voltage. `logging_gap: true` означает, что часть данных была буферизована или не могла быть записана из-за SD error.

## Recovery и надёжность

`/state/current_ride.json` содержит `format_version`, ride ID/folder, prior state, sample index, distance, timers, pulse counts и флаг logging gap. Он записывается через `.tmp`, flush/close, replacement/rename. Небольшой snapshot дублируется в Preferences/NVS на Start, Pause, Resume и периодически. После перезагрузки заезд всегда открывается как `PAUSED`.

CSV никогда не переписывается целиком. Events flush сразу; samples append-only. При потере SD скорость и статистика продолжаются, последние samples сохраняются в RAM ring buffer и выгружаются после успешного восстановления карты.

## USB MSC

В каждый момент SD принадлежит либо firmware, либо USB host. Перед USB закрываются ride operations и сохраняется checkpoint. В USB режиме firmware не читает/не пишет FAT. После safe eject пользователь перезагружает устройство.
