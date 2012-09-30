# common configuration
PKG_DEPS := glib-2.0 pango pangoxft freetype2 libxml-2.0 x11

CFLAGS += -include config.h
CFLAGS += -I$(OB_ROOT)
CFLAGS += $(shell pkg-config --cflags $(PKG_DEPS))

LDFLAGS += $(shell pkg-config --libs-only-other $(PKG_DEPS))
LDFLAGS += $(shell pkg-config --libs-only-L $(PKG_DEPS))

LOADLIBS += $(shell pkg-config --libs-only-l $(PKG_DEPS))
