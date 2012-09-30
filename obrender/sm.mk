include $(SMART.DECLARE)

LIBRARIES := libobrender.a

SOURCES := $(patsubst $(SRCDIR)/%,%,$(wildcard $(SRCDIR)/*.c))

include $(SMART.RULES)
