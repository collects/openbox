include $(SMART.DECLARE)

PROGRAMS := openbox

SOURCES := $(patsubst $(SRCDIR)/%,%,$(wildcard $(SRCDIR)/*.c $(SRCDIR)/actions/*.c))

LOADLIBS := \
  $(OB_ROOT)/obrender/libobrender.a \
  $(OB_ROOT)/obt/libobt.a \
  $(LOADLIBS)

include $(SMART.RULES)
