# TEST_PLAN.md

Ручной план тестирования прошивки велоспидометра.

## 1. Подготовка

Перед тестами проверить:

- плата ESP32-S3-N16R8 подключена к компьютеру;
- экран припаян по `docs/HARDWARE.md`;
- SD-карта вставлена для SD/USB тестов;
- датчик Холла может быть не подключён;
- измеритель батареи может быть не подключён;
- Serial monitor открыт.

## 2. Build test

Команды:

```bash
pio run
pio run -t upload
pio device monitor
```

Ожидаемый результат:

- проект собирается без ошибок;
- прошивка загружается;
- в Serial видно стартовые сообщения;
- нет циклических перезагрузок.

Чеклист:

```text
[ ] pio run successful
[ ] upload successful
[ ] serial boot log visible
[ ] firmware version printed
[ ] flash size printed
[ ] PSRAM status printed
[ ] no boot loop
```

## 3. Boot без SD

Условия:

- вынуть SD-карту;
- перезагрузить устройство.

Ожидаемый результат:

- устройство не зависает;
- появляется экран `SD card not found`;
- доступны кнопки `Retry` и `Continue without saving`;
- после `Continue without saving` открывается главное меню;
- в статусе видно `NO SD`.

Чеклист:

```text
[ ] no SD warning shown
[ ] Retry button works
[ ] Continue without saving works
[ ] main menu opens without SD
[ ] diagnostics open without SD
[ ] ride screen opens without SD
[ ] no crash without SD
```

## 4. TFT display test

Условия:

- SD может быть вставлена или отсутствовать;
- открыть `Diagnostics -> Display test`.

Ожидаемый результат:

- экран включён;
- подсветка работает;
- цвета отображаются корректно;
- текст читается;
- линии и прямоугольники рисуются;
- ориентация экрана правильная.

Чеклист:

```text
[ ] backlight on
[ ] black fill works
[ ] white fill works
[ ] red fill works
[ ] green fill works
[ ] blue fill works
[ ] text visible
[ ] lines visible
[ ] rectangles visible
[ ] screen orientation correct
[ ] no flickering or heavy artifacts
```

## 5. Touch raw test

Открыть:

```text
Diagnostics -> Touch raw test
```

Ожидаемый результат:

- касание определяется;
- x/y меняются при движении пальца;
- координаты соответствуют месту касания;
- кнопка назад нажимается.

Чеклист:

```text
[ ] touch detected
[ ] X coordinate changes
[ ] Y coordinate changes
[ ] coordinates match screen orientation
[ ] top-left area works
[ ] top-right area works
[ ] bottom-left area works
[ ] bottom-right area works
[ ] Back button works
```

## 6. Paint test

Открыть:

```text
Diagnostics -> Paint test
```

Ожидаемый результат:

- палец рисует линию на экране;
- линия появляется там, где было касание;
- кнопка Clear очищает экран;
- кнопка Back возвращает назад.

Чеклист:

```text
[ ] drawing works
[ ] drawing follows finger
[ ] no coordinate inversion
[ ] no X/Y swap
[ ] Clear button works
[ ] Back button works
[ ] entire touch area usable
```

## 7. Главное меню

Ожидаемый результат:

- все пункты меню видны;
- каждый пункт нажимается;
- возврат назад работает;
- быстрые повторные касания не ломают UI.

Чеклист:

```text
[ ] Ride opens
[ ] Diagnostics opens
[ ] Settings opens
[ ] USB Storage opens
[ ] About opens
[ ] Back navigation works
[ ] repeated taps do not freeze UI
```

## 8. SD test

Условия:

- вставить SD-карту;
- перезагрузить устройство;
- открыть `Diagnostics -> SD test`.

Ожидаемый результат:

- SD обнаружена;
- прошивка создаёт файл `/BIKE_SPEEDOMETER_SD_TEST.txt`;
- прошивка читает файл обратно;
- текст файла отображается на экране.

Чеклист:

```text
[ ] SD detected
[ ] SD size/type shown if available
[ ] test file created
[ ] test file read back
[ ] read text matches written text
[ ] result shown on display
[ ] no crash on repeated SD test
```

## 9. USB Mass Storage test

Условия:

- SD-карта вставлена;
- тестовый файл SD уже создан;
- подключить внешний USB корпуса к компьютеру, если он подключён к GPIO19/GPIO20;
- открыть `USB Storage` или `Diagnostics -> USB Mass Storage test`.

Ожидаемый результат:

- устройство определяется компьютером как накопитель;
- SD-карта видна;
- файл `/BIKE_SPEEDOMETER_SD_TEST.txt` виден на компьютере;
- файл можно открыть и прочитать;
- прошивка показывает экран активного USB-режима;
- прошивка не пишет на SD во время USB-режима.

Чеклист:

```text
[ ] USB Storage mode starts
[ ] display shows USB Storage Active
[ ] computer detects mass storage device
[ ] SD content visible on computer
[ ] BIKE_SPEEDOMETER_SD_TEST.txt visible
[ ] test file content readable on computer
[ ] firmware blocks ride logging during USB mode
[ ] safe eject on computer works
[ ] device can be rebooted after USB mode
```

## 10. Sensor GPIO4 test без датчика

Открыть:

```text
Diagnostics -> Sensor test
```

Ожидаемый результат:

- экран открывается даже без датчика;
- видно `GPIO4`;
- видно текущее состояние пина;
- pulse count не растёт сам по себе или растёт только при шуме, что будет заметно;
- прошивка не зависает.

Чеклист:

```text
[ ] Sensor test opens without physical sensor
[ ] GPIO4 shown
[ ] pin level shown
[ ] pulse counter shown
[ ] last pulse time shown
[ ] no crash without sensor
```

## 11. Sensor GPIO4 test с имитацией сигнала

Условия:

- безопасно имитировать сигнал на GPIO4;
- не подавать на GPIO4 больше 3.3V.

Ожидаемый результат:

- изменение состояния пина видно на экране;
- pulse count увеличивается;
- interval меняется;
- скорость рассчитывается при серии импульсов;
- ложные слишком быстрые импульсы фильтруются.

Чеклист:

```text
[ ] pin level changes
[ ] pulse count increments
[ ] last pulse time updates
[ ] interval shown
[ ] speed calculated
[ ] rejected pulse count works for too-fast pulses
[ ] no crash from bounce
```

## 12. Ride screen test

Открыть:

```text
Ride
```

Ожидаемый результат:

- виден главный экран заезда;
- снизу кнопки Start/Pause/Stop;
- сверху справа кнопка меню/настроек;
- центральные страницы листаются;
- скорость отображается;
- статистика отображается.

Чеклист:

```text
[ ] Ride screen opens
[ ] Start button visible
[ ] Pause/Resume button visible
[ ] Stop/Finish button visible
[ ] menu/settings button visible top-right
[ ] speed page visible
[ ] graph page visible
[ ] stats page visible
[ ] page switching works
```

## 13. Start / Pause / Resume / Finish

Ожидаемый результат:

- из `IDLE` можно начать заезд;
- из `RIDING` можно поставить паузу;
- из `PAUSED` можно продолжить;
- из `RIDING` или `PAUSED` можно завершить;
- после завершения показывается summary или экран итогов.

Чеклист:

```text
[ ] initial state IDLE
[ ] Start changes state to RIDING
[ ] Pause changes state to PAUSED
[ ] Resume changes state to RIDING
[ ] Stop/Finish changes state to FINISHED
[ ] summary screen shown
[ ] New ride returns to IDLE
```

## 14. Recovery test

Условия:

- SD-карта вставлена;
- начать заезд;
- дождаться сохранения recovery;
- перезагрузить устройство во время `RIDING`.

Ожидаемый результат:

- после перезагрузки устройство находит незавершённый заезд;
- заезд восстанавливается как `PAUSED`;
- есть кнопки `Resume`, `Finish`, `Discard`.

Чеклист:

```text
[ ] unfinished ride detected
[ ] recovered state is PAUSED
[ ] Resume works
[ ] Finish works
[ ] Discard works
[ ] ride does not auto-resume without user action
```

## 15. Settings test

Открыть:

```text
Settings
```

Проверить настройки:

- окружность колеса;
- порог остановки;
- яркость;
- полярность датчика;
- интервал обновления UI;
- параметры батареи, если доступны.

Ожидаемый результат:

- настройки открываются;
- значения можно менять;
- значения сохраняются на SD, если SD доступна;
- после перезагрузки значения загружаются.

Чеклист:

```text
[ ] Settings opens
[ ] wheel circumference editable
[ ] stop threshold editable
[ ] brightness editable
[ ] sensor polarity editable
[ ] values saved to config
[ ] values restored after reboot
[ ] invalid values rejected
```

## 16. Battery monitor disabled test

Ожидаемый результат:

- пока ADC-пин не выбран, батарейный монитор отключён;
- UI показывает `Battery: N/A` или `Battery monitor disabled`;
- диагностика батареи не падает.

Чеклист:

```text
[ ] battery monitor disabled by default
[ ] no ADC read from invalid pin
[ ] Battery test screen opens
[ ] Battery test says ADC pin not configured
[ ] no crash
```

## 17. Error handling test

Проверить ошибки:

```text
[ ] boot without SD
[ ] SD removed before SD test
[ ] corrupted config file
[ ] USB Storage without SD
[ ] touch init fail simulation if possible
[ ] repeated reboot during ride
```

Ожидаемый результат:

- ошибка показана на экране;
- ошибка записана в Serial;
- устройство не зависает;
- есть безопасный путь назад.

## 18. Критерии принятия первого тестового варианта

Первый тестовый вариант можно считать принятым, если выполнено:

```text
[ ] firmware builds
[ ] firmware uploads
[ ] TFT works
[ ] backlight works
[ ] touch works
[ ] Paint test works
[ ] main menu works
[ ] SD warning works without SD
[ ] SD test file works with SD
[ ] USB Mass Storage exposes SD
[ ] sensor GPIO4 test works without sensor
[ ] sensor GPIO4 reacts to signal
[ ] ride screen opens
[ ] Start/Pause/Resume/Finish state machine works
[ ] battery monitor disabled state works
[ ] no critical crashes during tests
```
