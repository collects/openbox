include $(SMART.DECLARE)

LIBRARIES := libobt.a

SOURCES := $(patsubst $(SRCDIR)/%,%,$(wildcard $(SRCDIR)/*.c))

include $(SMART.RULES)
