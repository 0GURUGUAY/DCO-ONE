# Menu generator conventions

`tools/generate_menu.py` reads the single source of truth [menu.json](menu.json) and regenerates:

- `daisy/src/menu_generated.inc`
- `esp32/src/menu_generated.inc`

## Supported JSON types

| JSON type | Generated behavior |
|---|---|
| `ENUM` | Becomes a navigable list of options. Both firmwares get the same labels and same default index. |
| `INT` / `FLOAT` | Becomes a numeric parameter with `min`, `max`, `step`, `default`, `unit`. |
| `TOGGLE` | Treated as an INT 0/1 parameter for now. |
| `ACTION` | Becomes a leaf placeholder with no numeric param. |

## Important rules

1. **Keep both firmware includes up to date**: after editing `menu.json`, run:
   ```bash
   python3 tools/generate_menu.py
   ```
2. **Then rebuild both firmwares**:
   ```bash
   make build-all
   ```
3. Do not edit `*_generated.inc` files manually; your changes will be overwritten the next time the generator runs.

## Daisy-specific callbacks

Numeric parameters fire an `Apply*` callback if the parameter `id` is known by the generator. Unknown IDs still get a numeric variable but no callback.
