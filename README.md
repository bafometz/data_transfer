# Приложение для передачи данных через сокеты между клиентом и сервером

## Сборка

```bash
    git clone https://gitlab.com/useful-libs/data_transfer.git
    cd data_transfer
    mkdir cmake && cd cmake
    cmake -S ../app -B .
    make
```

## Запуск

Исполняемый файл будет находиться в папке .../data_transfer/build/bin

Опции запуска:

Для запуска сервера необходимо добавить опцию -s

```bash
./DataTransfer -s
```

Запустит сервер на стандартном порту **7071**, можно добавить опцию **-p номер_порта** для указания другого порта.


```bash
./DataTransfer -p 7072 -s
```

Для запуска клиента и начала отправки файла на сервер
```bash
./DataTransfer -c </путь/к/файлу> 

```

Или же, если сервер запущен на нестандартном порту

```bash
./DataTransfer -p 7072 -c </путь/к/файлу>
```

Результат передачи будет сохранён в папку с исполняемым файлом, под именем date_time.hex