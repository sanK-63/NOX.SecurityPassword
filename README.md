# Генератор Паролей / Password Generator

Кроссплатформенный генератор паролей с графическим интерфейсом (Win32 + DirectX 11 + Dear ImGui) и консольной утилитой. Использует libsodium для криптографически стойкой генерации случайных чисел.

## Возможности

| Режим | Описание |
|-------|----------|
| **Маски** | Генерация по алфавиту: строчные/заглавные/цифры/спецсимволы + исключение символов |
| **Детерминированный** | Argon2id на основе мастер-пароля + домен + логин — одинаковые входы дают одинаковый пароль |
| **Фонетический** | Чередование гласных и согласных — легко читается и запоминается |
| **Шум** | Сбор энтропии с движений мыши → BLAKE2b → CSPRNG |

### Особенности

- Светлая фиолетовая тема, скруглённые углы
- Per-Monitor DPI v2, корректная работа на HiDPI
- Шкала энтропии с цветовой индикацией (красный < 50, жёлтый 50–80, фиолетовый > 80 бит)
- Автоочистка буфера обмена через 15 секунд
- SecureBuffer — безопасное хранение паролей в памяти (`sodium_malloc` + `sodium_mlock`, zero-on-destroy)
- Русскоязычный интерфейс

## Сборка

### Требования

- Windows 10/11
- Visual Studio 2022 (Build Tools) — `cl.exe`, `rc.exe`, `link.exe`
- CMake ≥ 3.16
- Git

### Шаги

```powershell
# 1. Склонировать репозиторий
git clone https://github.com/YOUR_USER/password-generator.git
cd password-generator

# 2. Собрать libsodium (требуется однократно)
cmake -P build_libsodium.cmake

# 3. Настроить и собрать проект
cmake -S . -B build
cmake --build build --config Release

# 4. Запустить
.\build\Release\pwgen_gui.exe   # GUI
.\build\Release\pwgen.exe        # консольная демонстрация
.\build\Release\pwgen_tests.exe  # тесты
```

## Структура проекта

```
├── CMakeLists.txt              # Основной конфиг сборки
├── src/
│   ├── core/
│   │   ├── SecureBuffer.h/.cpp         # Безопасное выделение памяти
│   │   ├── EntropyAccumulator.h/.cpp   # Сбор энтропии с мыши
│   │   └── StatelessGenerator.h/.cpp   # Argon2id детерминированная генерация
│   ├── gui/
│   │   ├── gui_app.cpp                 # ImGui GUI (Win32 + D3D11)
│   │   ├── app.rc / app.ico            # Иконка приложения
│   └── main.cpp                        # Консольная демонстрация
├── tests/
│   └── core_tests.cpp                  # 9 юнит-тестов
└── .gitignore
```

## Технологии

- **Язык:** C++17
- **Сборка:** CMake + MSBuild (v143)
- **GUI:** Dear ImGui 1.91.8, Win32 API, DirectX 11
- **Криптография:** libsodium (Argon2id, BLAKE2b, CSPRNG)

## Лицензия

MIT
