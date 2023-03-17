# snakery 

Puzzle game of a snake eating fruit.

## Building

Build cart.wasm:

```shell
make
```

Then run it with:

```shell
w4 run build/cart.wasm
```

To rebuild automatically:

```shell
w4 watch


# if you don't want to open a browser:
w4 watch --no-open
```

For more info about setting up WASM-4, see the [quickstart guide](https://wasm4.org/docs/getting-started/setup?code-lang=c#quickstart).

### convert png to pixel values

Importing images in WASM-4 works a bit different compared to other game engines and Fantasy Consoles. Images have to meet certain criteria:

    PNG only
    Index only
    4 colors max

``` shell
w4 png2src --c fruit.png
```

## controls

2	Save state
4	Load state
R	Reboot cartridge
F8	Open devtools
F9	Take screenshot
F10	Record 4 second video
F11	Fullscreen

### Gamepad 1

arrow keys, z,x

## Links

- [WASM-4](https://wasm4.org) fantasy console.
