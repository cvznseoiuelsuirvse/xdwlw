wallpaper manager for wlroots-based compositors

# xdwlw

> [!IMPORTANT]  
> xdwlwd must be running

```bash
# set picture.png as wallpaper (centered) on all monitors
$ xdwlw -i picture.png -m C

# set picture.png as wallpaper (stretched)
$ xdwlw -i picture.png -m F

# set picture.png as wallpaper only on the eDP-1 monitor
$ xdwlw -i picture.png -m C -o eDP-1

# set red color as wallpaper
$ xdwlw -c ff0000

# kill xdwlwd (daemon)
$ xdwlw --kill
```

# installing

dependencies:
* wayland
* wayland-protocols

to build and install:

    make
    sudo make install


### :P
im just trying to learn c :P
