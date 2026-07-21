# ****************************************************************************
#
# Name: VLSPQC
# Auth: Alessandra Dolmeta, Valeria Piscopo - Politecnico di Torino
# Date: September 2025
#
# Algorithm: HQC-2
#
# ****************************************************************************

# Compiled application has the same name of its folder
APPLI   = $(shell basename $(CURDIR))
SOURCES = src/code.c src/crypto_memset.c src/fft.c src/fips202.c src/gf.c src/gf2x.c src/kem.c src/hqc.c src/parsing.c src/reed_muller.c src/reed_solomon.c src/symmetric.c src/vector.c 
SRC 	= $(SOURCES) \
		  main.c

INC         = -I include/ -I 
EXTRA_CFLAGS = -g -O2

EXTRA_CFLAGS += -DTEST_VECT_INCLUDE="\"test_vectors_1.h\""

# Debug print statements
$(info APPLI: $(APPLI))
$(info EXTRA_CFLAGS: $(EXTRA_CFLAGS))
$(info PRINT_VECT: $(PRINT_VECT))
# Compiler
CC = gcc

# Targets
all: $(APPLI)

$(APPLI): $(SRC)
	$(CC) $(SRC) $(INC) $(EXTRA_CFLAGS) -o $(APPLI)

clean:
	rm -f $(APPLI)

.PHONY: all clean
