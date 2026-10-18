# ALB-macOS-source
A Lisp-like programming language with standard libraries and IDE support.

## Features
- C++11 interpreter with REPL, bytecode compilation, and packaging
- Standard libraries: math, string, io, gui, agame (2D game engine)
- Package manager: `alb -m install <name>`
- IDE with Python (tkinter)

## Build
```bash
make            # Build ALB interpreter
make app        # Build ALB.app bundle
make dmg        # Build DMG installer
```

## Libraries
- `lib/math` - Math utilities (vector, interpolation, trig, random)
- `lib/string` - String operations (query, extract, transform, padding, convert)
- `lib/io` - File and directory utilities (CSV, path, lines)
- `lib/gui` - GUI toolkit (widgets, menus, events)
- `lib/agame` - 2D game engine (SDL2 backend on macOS)

## Games
- `games/snake.alb` - Snake game demo using agame library
