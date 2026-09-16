# QGroundControl FPV — инструкция на русском

Это наша версия QGroundControl с доработками для FPV, пультов TX-12 и Pocket, статистики радиосвязи WFB-ng на RADXA и Raspberry Pi.

**Все наши изменения находятся в ветке `fpv-modified`.** Ветка — это отдельная версия проекта, а не папка. В `master` наших доработок нет.

## Где найти нашу версию на GitHub

1. Откройте [репозиторий CyberCutIet/qgroundcontrol](https://github.com/CyberCutIet/qgroundcontrol).
2. Слева над списком файлов нажмите кнопку `master`.
3. Выберите `fpv-modified`.

Можно сразу открыть [ветку со всеми нашими изменениями](https://github.com/CyberCutIet/qgroundcontrol/tree/fpv-modified).

## Что добавлено

- Отдельные значки TX-12 и Pocket, обнаружение пультов по имени и восстановление выбора TX-12 после переподключения.
- Встроенный мост управления Pocket для Linux.
- **Вынос** — RSSI и SNR с RADXA.
- **Ретранслятор** — RSSI и SNR с Raspberry Pi через отдельный поток статистики WFB-ng.
- **Packet Loss** — оценка потерь пакетов видеопотока в процентах, цветная шкала и обновление интерфейса каждые 0,5 секунды.
- Видеопрофиль RTP/H.264 на UDP 5602 с настройками низкой задержки.
- Исправление обнаруженного падения при ошибке запуска видеопотока GStreamer.

Подробности, шкалы цветов и результаты проверок: [описание наших доработок](fpv/README.md).

## Как запустить уже собранную программу на нашем ноутбуке

Откройте терминал и вставьте команду целиком, вместе с кавычками:

```bash
"/home/ed/Рабочий стол/Ретранслятор/qgroundcontrol/run_qgc.sh"
```

Этот путь относится к нашему текущему ноутбуку. Скрипт запускает готовую программу без повторной сборки. Если появляется сообщение `A second instance of QGroundControl is already running`, закройте ранее запущенное окно QGC.

## Как скачать нашу версию заново

Команды ниже скачивают проект в папку `qgroundcontrol` внутри вашей домашней папки. Если такая папка уже существует, сначала выберите другое место: не удаляйте её, пока не сохранили свои изменения.

```bash
cd ~
git clone --branch fpv-modified https://github.com/CyberCutIet/qgroundcontrol.git
cd qgroundcontrol
```

`git clone` скачивает исходники. **Готовая программа и зависимости сборки в репозиторий не входят.** После скачивания нужна сборка.

## Как собрать после скачивания

Наши доработки проверялись на Linux с Qt 6.11.1. На новом компьютере сначала установите зависимости QGC: компилятор C++, CMake, Ninja, Python, Qt с необходимыми модулями, GStreamer и `just` версии не ниже 1.30. Инструкции и штатные скрипты установки находятся в [документации инструментов](tools/README.md#quick-start). Одного скачивания исходников на чистый компьютер недостаточно.

На подготовленном ноутбуке, из папки скачанного проекта:

```bash
python3 tools/configure.py -B build -t Debug --testing --qt-root "$HOME/Qt/6.11.1/gcc_64"
JOBS=4 just build
./run_qgc.sh
```

Путь после `--qt-root` должен указывать на установленную Qt. Первая сборка занимает время и может скачивать зависимости. Если загрузка прервалась из-за интернета, повторите команду, которая завершилась ошибкой. Успешно собранные файлы обычно используются повторно.

В дальнейшем из этой папки достаточно запускать:

```bash
./run_qgc.sh
```

## Видео, настройки и платы

На нашем ноутбуке видеопрофиль уже настроен. Для нового компьютера сначала запустите и закройте QGC, затем выполните из папки проекта:

```bash
python3 fpv/apply_video_profile.py --dry-run
python3 fpv/apply_video_profile.py
```

Первая команда показывает будущие изменения. Вторая применяет видеопрофиль и сохраняет резервную копию настроек. Для этого профиля ноутбуку нужен локальный адрес `10.10.10.1`, видео поступает на UDP 5602.

Личные настройки QGC и калибровки пультов не хранятся в GitHub. На нашем Linux они находятся отдельно от исходников, обычно в `~/.config/QGroundControl/`. Удаление только папки проекта не удаляет эти настройки; перед переносом на другой компьютер сохраните их отдельно при закрытом QGC.

Клонирование проекта также не настраивает RADXA и Raspberry Pi. Их конфиги WFB-ng, ключи и необходимые сервисы должны быть подготовлены отдельно. Отправитель статистики RPi и инструкция его установки: [fpv/bridge/rpi-stats](fpv/bridge/rpi-stats/README.md). Мост Pocket уже встроен в QGC: старый Python-мост одновременно с ним запускать не нужно.

## Как получить новые изменения нашей версии

Закройте QGC. В терминале перейдите в папку вашей рабочей копии и выполните:

```bash
git status
git switch fpv-modified
git pull --ff-only origin fpv-modified
JOBS=4 just build
./run_qgc.sh
```

Если `git status` показывает изменённые файлы или Git сообщает об ошибке, остановитесь и сохраните свои правки перед обновлением. Не используйте команды принудительного сброса ради устранения ошибки.

Эти команды получают обновления **нашей ветки**. Обновления оригинального QGC сначала объединяются с нашими изменениями в отдельной пробной ветке, собираются и проверяются. Кнопка `Sync fork` сама по себе не переносит новые функции оригинала в наши доработки.

## Где лежат файлы

| Папка или файл | Что внутри |
|---|---|
| `src/` | Исходники QGC, включая наши изменения |
| `src/Toolbar/` | Значки и расположение элементов верхней панели |
| `src/Comms/` | Приём статистики RADXA и RPi |
| `src/Joystick/` | Работа с пультами и встроенный мост |
| `fpv/profile/video.ini` | Видеопрофиль |
| `fpv/bridge/rpi-stats/` | Отправитель статистики для Raspberry Pi |
| `fpv/README.md` | Подробное описание доработок и ограничений |
| `run_qgc.sh` | Запуск уже собранной программы |
| `build/` | Локальные результаты сборки; в GitHub их нет |

Репозиторий публичный. Пароли, ключи WFB, личные настройки, записи полётов и готовые сборки сюда не включены. Текущая версия предназначена для стендового тестирования; результаты автоматических проверок и известные ограничения описаны в [fpv/README.md](fpv/README.md#проверки-и-ограничения).

## Документация оригинального QGroundControl

Ниже сохранено описание исходного проекта. Ссылки на его готовые релизы ведут на обычный QGC **без наших доработок**.

---

<p align="center">
  <img src="https://raw.githubusercontent.com/Dronecode/UX-Design/35d8148a8a0559cd4bcf50bfa2c94614983cce91/QGC/Branding/Deliverables/QGC_RGB_Logo_Horizontal_Positive_PREFERRED/QGC_RGB_Logo_Horizontal_Positive_PREFERRED.svg" alt="QGroundControl Logo" width="500">
</p>

<p align="center">
  <a href="https://github.com/mavlink/QGroundControl/releases"><img src="https://img.shields.io/github/v/release/mavlink/QGroundControl" alt="Latest Release"></a>
  <a href="https://github.com/mavlink/qgroundcontrol/blob/master/.github/COPYING.md"><img src="https://img.shields.io/github/license/mavlink/QGroundControl" alt="License"></a>
  <a href="https://github.com/mavlink/QGroundControl/actions/workflows/linux.yml"><img src="https://github.com/mavlink/QGroundControl/actions/workflows/linux.yml/badge.svg" alt="Linux Build"></a>
  <a href="https://securityscorecards.dev/viewer/?uri=github.com/mavlink/qgroundcontrol"><img src="https://img.shields.io/ossf-scorecard/github.com/mavlink/qgroundcontrol?label=openssf%20scorecard" alt="OpenSSF Scorecard"></a>
  <a href="https://crowdin.com/project/qgroundcontrol"><img src="https://badges.crowdin.net/qgroundcontrol/localized.svg" alt="Crowdin"></a>
  <a href="https://discord.com/channels/1022170275984457759/1022185820683255908"><img src="https://img.shields.io/discord/1022170275984457759?logo=discord&logoColor=white&label=Discord" alt="Dronecode Discord"></a>
  <a href="https://doi.org/10.5281/zenodo.595404"><img src="https://zenodo.org/badge/DOI/10.5281/zenodo.595404.svg" alt="DOI"></a>
</p>

**QGroundControl** (QGC) is a Ground Control Station (GCS) for UAVs, providing full flight control
and mission planning for any *MAVLink-enabled* drone, including *PX4* and *ArduPilot* platforms.

## Features

- **Mission planning** — plan, edit, and fly autonomous waypoint, survey, and structure-scan missions.
- **Live Fly View** — real-time flight display with map, instruments, and full vehicle telemetry.
- **Vehicle setup** — guided wizards for sensor calibration, radio, flight modes, and power.
- **Parameter tuning** — inspect and edit every vehicle parameter through the Fact System.
- **Video streaming** — GStreamer-based UDP RTP / RTSP video with recording in the Flight Display.
- **Multi-vehicle** — connect to and monitor multiple vehicles simultaneously.
- **MAVLink tooling** — built-in MAVLink Inspector, console, and log download/analysis.
- **Cross-platform** — Windows, macOS, Linux, Android, and iOS from a single codebase.

## Download

Grab the latest stable build for your platform, or see all assets on the
[releases page](https://github.com/mavlink/QGroundControl/releases/latest):

<p align="center">
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl-installer.exe"><img src="https://img.shields.io/badge/Windows-0078D6?logo=windows&logoColor=white" alt="Windows"></a>
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl.dmg"><img src="https://img.shields.io/badge/macOS-000000?logo=apple&logoColor=white" alt="macOS"></a>
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl-x86_64.AppImage"><img src="https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black" alt="Linux (AppImage)"></a>
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl.apk"><img src="https://img.shields.io/badge/Android-3DDC84?logo=android&logoColor=white" alt="Android"></a>
</p>

## Links

- [Official Website](http://qgroundcontrol.com)
- [User Manual](https://docs.qgroundcontrol.com/en/)
- [Developer Guide](https://dev.qgroundcontrol.com/en/) / [Build Instructions](https://dev.qgroundcontrol.com/en/getting_started/)
- [Discussion & Support](https://docs.qgroundcontrol.com/en/Support/Support.html)
- [Dronecode Discord](https://discord.com/channels/1022170275984457759/1022185820683255908)
- [Security Policy](.github/SECURITY.md)
- [Code of Conduct](.github/CODE_OF_CONDUCT.md)
- [License](https://github.com/mavlink/qgroundcontrol/blob/master/.github/COPYING.md)

## Contributing

QGC is open source and welcomes contributions. See [AGENTS.md](AGENTS.md) for build/test/lint
commands and coding conventions, and [.github/CONTRIBUTING.md](.github/CONTRIBUTING.md) for
architecture patterns and the contribution workflow.

QGC's interface is translated by the community — help translate it into your language on
[Crowdin](https://crowdin.com/project/qgroundcontrol).

## Star history

[![Star History Chart](https://api.star-history.com/svg?repos=mavlink/qgroundcontrol&type=Date)](https://star-history.com/#mavlink/qgroundcontrol&Date)
