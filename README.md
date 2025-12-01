# Wayland Lock
Set a video for your wayland session lock screen

## Building
1. Ensure required lib dependencies exist
    
    - gtk4
    - gtk4-layer-shell
2. Ensure build tools exist
    
    - C compiler
    - libc
    - make
3. Run `make` to build the program

## Configuration
1. Ensure env variable `WLLOCK_IMAGE_FILE` is defined with the absolute path of
   a media(picture or video) file.

   File format is handled by GTK
2. Create a config dir `$HOME/.config/wllock`
3. Copy the `style.css` file to that directory

