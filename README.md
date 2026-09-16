# QGroundControl FPV

Модифицированный QGroundControl для Linux с поддержкой TX-12, Pocket и статистики WFB-ng. Рабочая ветка — `fpv-modified`.

## Доработки

- Обнаружение TX-12 и Pocket по имени, отдельные индикаторы подключения.
- Автоматическое восстановление выбора TX-12 после переподключения.
- Встроенный мост управления Pocket.
- RSSI и SNR: **Вынос** — RADXA, **Ретранслятор** — Raspberry Pi.
- **Packet Loss** — оценка потерь видеопотока в процентах с цветной шкалой; обновление интерфейса каждые 0,5 секунды.
- Видеопрофиль RTP/H.264 с низкой задержкой и исправление падения при ошибке запуска GStreamer.

## Сборка

Требуются Qt 6.11.1, компилятор C++, CMake, Ninja, Python, GStreamer и `just` ≥ 1.30. Установка зависимостей описана в [tools/README.md](tools/README.md#quick-start).

```bash
git clone --branch fpv-modified https://github.com/CyberCutIet/qgroundcontrol.git
cd qgroundcontrol
python3 tools/configure.py -B build -t Debug --testing --qt-root "$HOME/Qt/6.11.1/gcc_64"
JOBS=4 just build
```

Если Qt установлена в другом месте, измените путь `--qt-root`. Репозиторий содержит исходники; готовый бинарник создаётся при сборке.

## Запуск

Из папки проекта:

```bash
./run_qgc.sh
```

Скрипт запускает готовую сборку без компиляции. Перед повторным запуском закройте предыдущий экземпляр QGC.

## Видеопрофиль

После установки или сброса настроек запустите и закройте QGC, затем примените профиль:

```bash
python3 fpv/apply_video_profile.py
./run_qgc.sh
```

Профиль: RTP/H.264, локальный адрес ноутбука `10.10.10.1`, UDP 5602, декодирование `avdec_h264`, преобразование `videoconvert`. Скрипт сохраняет резервную копию настроек перед изменением. Просмотр изменений без применения: `python3 fpv/apply_video_profile.py --dry-run`.

## Управление и статистика

RADXA и Raspberry Pi требуют отдельной настройки WFB-ng и сервисов. Отправитель статистики RPi и инструкция установки находятся в [fpv/bridge/rpi-stats](fpv/bridge/rpi-stats/README.md).

Мост Pocket встроен в QGC; внешний Python-мост одновременно с ним запускать не следует. Подключения и калибровки пультов настраиваются в QGC. Пользовательские настройки хранятся отдельно от проекта в `~/.config/QGroundControl/` и не входят в репозиторий.

## Обновление

Закройте QGC и сохраните локальные изменения. Из папки проекта:

```bash
git switch fpv-modified
git pull --ff-only origin fpv-modified
JOBS=4 just build
./run_qgc.sh
```

Обновления оригинального QGC интегрируются и проверяются отдельно перед включением в `fpv-modified`.

## Документация

- [Параметры индикаторов, протоколы и результаты проверок](fpv/README.md)
- [Инструменты сборки](tools/README.md)
- [Исходный проект QGroundControl](https://github.com/mavlink/qgroundcontrol)
- [Лицензия](.github/COPYING.md)
