# ====== Projeto SmartGrid (Raspberry Pi + MQTT + LEDs/Botões) ======

# Alvo
TARGET := smartgrid

# Compilador e flags
CC      ?= gcc
BUILD   ?= release            # use: make BUILD=debug  (ou deixe release)
WARN    := -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes
DEFS    :=
INCS    :=

# Perfis
ifeq ($(BUILD),debug)
  CFLAGS := -O0 -g3 $(WARN) -DDEBUG
else
  CFLAGS := -O2 $(WARN)
endif

# Threads e deps automáticas
CFLAGS  += -pthread -MMD -MP
LDFLAGS +=
LDLIBS  += -lmosquitto -lcjson -lbcm2835 -pthread

# Fontes/Objetos
SRCS := \
  main.c \
  logbuf.c \
  cmdq.c \
  state.c \
  mqtt_if.c \
  control.c \
  ui.c \
  hw_io.c \
  gpio.c

OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

# ====== Regras ======
.PHONY: all clean run fmt help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEFS) $(INCS) -c $< -o $@

# Executa com sudo (bcm2835 requer privilégios)
run: $(TARGET)
	sudo ./$(TARGET)

clean:
	$(RM) $(OBJS) $(DEPS) $(TARGET)

# (Opcional) Formatar com clang-format, se instalado
fmt:
	@command -v clang-format >/dev/null 2>&1 && \
	clang-format -i $(SRCS) *.h || \
	echo "clang-format não encontrado; pulando."

help:
	@echo "Targets disponíveis:"
	@echo "  make            -> compila ($(BUILD))"
	@echo "  make BUILD=debug|release"
	@echo "  make run        -> executa com sudo"
	@echo "  make clean      -> remove binários/objetos"
	@echo "  make fmt        -> formata código (clang-format)"

# Inclui dependências geradas (-MMD -MP)
-include $(DEPS)
