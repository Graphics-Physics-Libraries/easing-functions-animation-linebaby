# File Format

- little-endian / lazy-endian

| Data | Size | Description |
| ==== | ==== | =========== |
| `LINE` | 4 | magic bytes |
| `u32` | 4 | version |
| `float` | 4 | timeline duration |
| `bool` | 1 | artboard set |
| `vec2` | 4 × 2 | artboard area top corner |
| `vec2` | 4 × 2 | artboard area bottom corner |
| `u32` | 4 | stroke count |
| `LB_STROKE` | ? | strokes |

`LB_STROKE`

| Data | Size | Description |
| ==== | ==== | =========== |
| `float` | 4 | global start time |
| `float` | 4 | full duration |
| `float` | 4 | scale |
| `vec4` | 4 × 4 | color |
| `float` | 4 | jitter |
| `u32` | 4 | enter animation method |
| `u32` | 4 | enter easing method |
| `float` | 4 | enter duration |
| `bool` | 1 | enter draw reverse |
| `u32` | 4 | exit animation method |
| `u32` | 4 | exit easing method |
| `float` | 4 | exit duration |
| `bool` | 1 | exit draw reverse |
| `u16` | 2 | vertex count |
| `LB_BEZIER_VERTEX` | ? | vertices |

`LB_BEZIER_VERTEX`

| Data | Size | Description |
| ==== | ==== | =========== |
| `vec2` | 4 × 2 | anchor |
| `vec2` | 4 × 2 | handle 1 |
| `vec2` | 4 × 2 | handle 2 |
