
.section .rodata

.global default_cursor_png
default_cursor_png:
.incbin "default_cursor.png"
default_cursor_png_end:
.global default_cursor_png_size
default_cursor_png_size:		.word default_cursor_png_end-default_cursor_png

.global default_font_png
default_font_png:
.incbin "default_font_2.png"
.global default_font_png_size
default_font_png_end:
default_font_png_size:		.word default_font_png_end-default_font_png
