FROM nfnty/arch-mini

RUN pacman -Syyu --noconfirm
RUN pacman -S --noconfirm gcc gcc-libs git curl jshon expac yajl wget unzip cmake && pacman -Sc
RUN pacman -S --noconfirm curl sdl2 speex fontconfig openssl libpng sdl2_ttf jansson && pacman -Sc
RUN pacman -S --noconfirm --needed make pkg-config
