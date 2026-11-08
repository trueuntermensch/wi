# wi
suckless-reader for wikipedia. one C99 + POSIX.1-2008 file, only 100 LoC.

## build
	make
	doas make install
dependence: `libcurl` (headers `curl/curl.h`).

## usage
    wi -l en "Plan 9 from Bell Labs"
    wi -l ru Навальный
    wi -i -l en London        # only preamble
    wi -w 100 -l fr Napoleon Bonaparte  # 100-column carry width
    
options:
    -l lang   language (above config)
    -w width  carry width, 0 = terminal width
    -i        only intro (preamble)
    -q        don't print the article title
    -v        version
    -h        help

search goes via `generator=search`, so u can write not exact name of article, like `wi -l en napoleon`.

## config

reading `/etc/wi/config`, after `$XDG_CONFIG_HOME/wi/config` (or
`~/.config/wi/config`). format  — `key = value`:

    language   = ru
    api        = https://%s.wikipedia.org/w/api.php
    user_agent = wi/1.0 (suckless wikipedia reader)
    width      = 0
    timeout    = 15
    title      = 1
    intro      = 0

## contact
yourparanoid@tuta.io
