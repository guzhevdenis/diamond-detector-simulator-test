# Навигация по C++ в VS Code

Откройте корневую папку проекта через **File → Open Folder**. Нужны расширения
**C/C++** и **CMake Tools** от Microsoft (указаны в рекомендациях проекта).
После изменения настроек выполните **Developer: Reload Window** через `Ctrl+Shift+P`
и дождитесь завершения индексации C/C++.

- `F12` / `Ctrl+клик` — переход к определению.
- `Alt+F12` — просмотр определения в текущем редакторе.
- `Shift+F12` — поиск использований.
- `Ctrl+Shift+O` — символы текущего файла.
- `Ctrl+T` — поиск символа в проекте.

Например, поставьте курсор на `process` в `src/core/amplifier_model.h`
и нажмите `F12`: реализация находится в `src/core/amplifier_model.cpp`.

## Компилятор и Qt

В `c_cpp_properties.json` указан установленный на этой машине LLVM-MinGW через
`${env:LOCALAPPDATA}`. После обновления или замены компилятора измените
`compilerPath` через **C/C++: Edit Configurations (UI)**.
Пути стандартной библиотеки расширение получает от компилятора автоматически.

CMake Tools служит поставщиком конфигурации IntelliSense. После успешного
**CMake: Configure** он передаст пути Qt, определения макросов и настройки
целевого приложения. Пока CMake не настроен, используются явно указанные
пути исходников и компилятор; полная навигация по Qt недоступна.

В текущем `build/CMakeCache.txt` Qt не найден (`QT_DIR-NOTFOUND`). Для полной
настройки установите Qt с модулями Widgets, Charts, Concurrent и PrintSupport
и совместимым компилятором. Через **CMake: Select a Kit** выберите этот компилятор,
а в локальных пользовательских настройках VS Code задайте
`cmake.configureSettings.CMAKE_PREFIX_PATH` как путь к каталогу комплекта Qt
(каталог с `bin`, `include` и `lib`). При смене компилятора используйте новый
`cmake.buildDirectory`, чтобы не смешивать разные комплекты в одном CMake cache.
Затем выполните **CMake: Configure** и **CMake: Build**: сборка создаст также
заголовки `ui_*.h` для форм Qt.

Если после перезагрузки переходы не работают, выполните
**C/C++: Reset IntelliSense Database**, затем **C/C++: Log Diagnostics**
и проверьте путь компилятора и список include-путей.

Описание механизма конфигурации:
[документация VS Code](https://code.visualstudio.com/docs/cpp/configure-intellisense).
