CC=gcc
CFLAGS=-Wall -Wextra -std=c99 -Icore/inc -Iworld/inc -Isim/inc -Iui/inc -Iassets
LDFLAGS=-lraylib -lm -ldl -lGL -lpthread -ldl -lrt -lX11
SRC=$(wildcard core/src/*.c world/src/*.c sim/src/*.c ui/src/*.c)

# Définition des chemins
BUILD_DIR=build
# Liste des répertoires d'objets nécessaires (ex: build/core/src/)
OBJ_DIRS=$(patsubst %/,$(BUILD_DIR)/%,$(dir $(SRC)))
# Liste des fichiers objets (ex: build/core/src/app.o)
OBJ=$(patsubst %.c,$(BUILD_DIR)/%.o,$(SRC))
BIN=bin/containment_tycoon

# --- RÈGLES DE COMPILATION ---

# Règle pour créer TOUS les répertoires d'objets nécessaires
$(OBJ_DIRS):
	@mkdir -p $@

# La cible 'all' dépend des sous-répertoires pour s'assurer qu'ils sont créés
all: $(OBJ_DIRS) $(BIN)

# Règle pour l'édition de liens (création de l'exécutable)
$(BIN): $(OBJ) main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Règle générique pour compiler les fichiers source en fichiers objet
# Elle dépend de la création du répertoire cible (via la dépendance ordonnée)
$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- RÈGLE DE NETTOYAGE ---

clean:
	rm -rf $(BUILD_DIR) $(BIN)