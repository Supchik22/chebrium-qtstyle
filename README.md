# chebrium-qtstyle

A light Qt widget style that follows the look of the [Memrium](https://github.com/Supchik22/memrium-desktop) shell
(accent, dark or light, rounding, glass), so Qt and KDE programs sit well on the desktop. It reads the file the shell writes
(`~/.config/memrium/appearance.toml`).

```
cmake -B build && cmake --build build
QT_PLUGIN_PATH=$PWD/build QT_STYLE_OVERRIDE=chebrium kate
```

`default.nix` / `shell.nix` give a Nix environment with Qt. License: MIT.
