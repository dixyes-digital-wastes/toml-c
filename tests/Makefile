# Tests for the parser.
#
# `make check` builds the fork the way the driver does: floating point and
# timestamps turned off, and no file reader (the caller supplies the buffer).
# Third party sources build here with the host libc, which is where a parser
# belongs; only the driver needs them to be freestanding.

ifeq ($(origin CC),default)
CC := clang
endif

# The same optional subsets the driver turns on.
DEFS := -DTOML_NO_FLOAT -DTOML_NO_TIMESTAMP -DTOML_NO_FILE

CFLAGS := -std=gnu23 -O2 -Wall -Wextra -I.. $(DEFS)

.PHONY: all check clean
all: test_toml

# Sources are listed explicitly: the header is a prerequisite, and $^ would
# otherwise hand it to the compiler as an input file.
test_toml: test_toml.c ../toml.c ../toml.h
	$(CC) $(CFLAGS) test_toml.c ../toml.c -o $@

check: all
	./test_toml

clean:
	rm -f test_toml
